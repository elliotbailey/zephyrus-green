from fastapi import FastAPI, WebSocket
from fastapi.responses import HTMLResponse
from pydantic import BaseModel
import uvicorn
import time
import logging
import paho.mqtt.client as mqtt
import paho.mqtt.subscribe as subscribe
import threading
import os
import asyncio
import json

rootpath = os.path.dirname(os.path.abspath(__file__))

MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883
MQTT_TOPIC = "discotest"

MQTT_ULTRASONIC_TOPIC = "esp32/receive"
MQTT_SPEAKER_TOPIC = "zephyrus/green/speaker"

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("zephyrus-green")

app = FastAPI()

DELAY = 10
FAKE_AIS_DATA = [
    [0 * DELAY, 503586200, -27.496767459424884, 153.01952753903188, 345],
    [1 * DELAY, 503586200, -27.492589806949766, 153.01665580689638, 347],
    [2 * DELAY, 503586200, -27.490804557080086, 153.00997428464225, 343],
    [3 * DELAY, 503586200, -27.48998333241987, 153.00341351279033, 346],
    [4 * DELAY, 503586200, -27.49119731453502, 153.0027292605113, 344],
    [5 * DELAY, 503586200, -27.492357726103666, 153.0021255085004, 348],
    [6 * DELAY, 503586200, -27.490501061703664, 152.99673199043346, 342],
    [7 * DELAY, 503586200, -27.486127065082915, 152.9959068626884, 345],
    [8 * DELAY, 503586200, -27.48321692214612, 152.99693324110694, 347],
    [9 * DELAY, 503586200, -27.480860185525977, 152.99960987504696, 343],
    [10 * DELAY, 503586200, -27.477699936920835, 153.00232675910473, 343],
    [11 * DELAY, 503586200, -27.47353969742742, 153.00568764531508, 343],
    [12 * DELAY, 503586200, -27.4725576437457, 153.00755927654893, 343],
]

FERRY_SPIRIT_OF_BRISBANE = [
    [503586200, -27.491273078778477, 152.99884405165088],
    [503586200, -27.49219897447619, 153.0022697246721],
    [503586200, -27.4903709165069, 153.00325995827978],
    [503586200, -27.490869480781242, 153.0055883454114],
    [503586200, -27.490916962975348, 153.00815760017733],
    [503586200, -27.490988186228115, 153.0127340852291],
    [503586200, -27.491890343442083, 153.01578507526366],
    [503586200, -27.493955780806832, 153.01835433002955],
    [503586200, -27.4945299969971, 153.0189425884881],
    [503586200, -27.49617698290502, 153.01955530075008],
    [503586200, -27.496901648898334, 153.0195181666736],
    [503586200, -27.496045224939163, 153.0200937448591],
    [503586200, -27.494167656790644, 153.0185898147615],
    [503586200, -27.492981807776975, 153.01738295727574],
    [503586200, -27.491960649780143, 153.01563765568096],
    [503586200, -27.491054775887818, 153.01274119771517],
]
MMSI_SPIRIT_OF_BRISBANE = 503586200

FERRY_NEVILLE_BONNER = [
    [503102970, -27.498812109104225, 153.02104066380946],
    [503102970, -27.497362797501147, 153.02037225043273],
    [503102970, -27.496918118524547, 153.01942533148238],
    [503102970, -27.496489907441912, 153.01959243482656],
    [503102970, -27.4954852518249, 153.01959243482656],
    [503102970, -27.494612346877705, 153.01901685664106],
    [503102970, -27.49304768861282, 153.01743865839046],
    [503102970, -27.492010060869017, 153.01561908864272],
    [503102970, -27.491087716887222, 153.01372525074203],
    [503102970, -27.490840659151544, 153.01118156650284],
    [503102970, -27.49095595283051, 153.00711538512778],
    [503102970, -27.49090654125031, 153.0047388041772],
]
MMSI_NEVILLE_BONNER = 503102970


TERMINAL_LOCATIONS = {
    "UQ": {"lat": -27.496794118158004, "lon": 153.019545830108},
    "West End": {"lat": -27.490377956126146, "lon": 153.0032654581362},
    "Guyatt Park": {"lat": -27.49232410927266, "lon": 153.00212960553657},
    "Regatta": {"lat": -27.483200149234516, "lon": 152.9968978536975},
    "Milton": {"lat": -27.473530974935436, "lon": 153.00563885557887}
}


sim_start_time = 0
already_sent_packets = []

# connect to MQTT
client = mqtt.Client()
client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
client.loop_start()


@app.get("/")
async def root():
    return {
        "message": "Hello World"
    }


def reset_path():
    global sim_start_time, already_sent_packets
    already_sent_packets = []
    sim_start_time = time.time()
    logger.info("Reset path")


ferry_data_queue = []


@app.get("/ferry")
async def get_ferry():

    if not ferry_data_queue:
        return {
            "status": 404,
            "mmsi": -1,
            "lat": -1,
            "lon": -1
        }

    data = ferry_data_queue.pop(0)

    return {
        "status": 200,
        "mmsi": data[0],
        "lat": data[1],
        "lon": data[2]
    }


# a 1 means increase, 0 means decrease
volume_changes = []


@app.get("/volumechange")
async def get_volume_change():
    if not volume_changes:
        # No volume changes to process
        return {
            "status": 404,
            "change": -1
        }

    # There is a volume update present
    change = volume_changes.pop(0)

    logger.info("Disco got volume change")

    return {
        "status": 200,
        "change": change
    }


def send_volume_to_speaker(volume):
    logger.info(f"Sending volume to speaker: {volume}")
    client.publish(MQTT_SPEAKER_TOPIC, payload=f"Volume {volume}", qos=1)


class VolumeReq(BaseModel):
    volume: int


@app.post("/volume")
async def change_volume(vol: VolumeReq):

    send_volume_to_speaker(vol.volume)
    return {"status": "ok"}


def send_arrival_to_speaker():
    client.publish(MQTT_SPEAKER_TOPIC, payload="Arrive", qos=1)


def send_departure_to_speaker():
    client.publish(MQTT_SPEAKER_TOPIC, payload="Depart", qos=1)


class FerryStatusReq(BaseModel):
    mmsi: int


@app.post("/arriving")
async def ferry_arriving(ferry: FerryStatusReq):
    mmsi = ferry.mmsi
    logger.info(f"Base node says ferry {mmsi} arriving")
    send_arrival_to_speaker()


@app.post("/departing")
async def ferry_departing(ferry: FerryStatusReq):
    mmsi = ferry.mmsi
    logger.info(f"Base node says ferry {mmsi} departing")
    send_departure_to_speaker()


@app.get("/dashboard")
async def show_dashboard():
    # Serve the HTML (see next section)
    with open("test2.html") as f:
        return HTMLResponse(f.read())


def on_message_from_ultrasonic(client, userdata, message):
    global volume_changes
    logger.info(f"Payload: {message.payload.decode()}")
    content = message.payload.decode()

    # Volume changes received, add to volume change queue
    if content == "Volume Down":
        volume_changes.append(0)
    elif content == "Volume Up":
        volume_changes.append(1)


def mqtt_sub_thread():
    subscribe.callback(on_message_from_ultrasonic, topics=MQTT_ULTRASONIC_TOPIC, hostname=MQTT_BROKER)


current_positions = []


def supply_ferry_data_thread():
    global current_positions

    DELAY = 5

    while True:
        # first ferry
        logger.info("Starting first ferry")
        for i in range(len(FERRY_NEVILLE_BONNER)):
            ferry_data_queue.append(FERRY_NEVILLE_BONNER[i])
            current_positions = [{
                "mmsi": FERRY_NEVILLE_BONNER[i][0],
                "lat": FERRY_NEVILLE_BONNER[i][1],
                "lon": FERRY_NEVILLE_BONNER[i][2]
            },
            {
                "mmsi": FERRY_SPIRIT_OF_BRISBANE[i][0],
                "lat": FERRY_SPIRIT_OF_BRISBANE[i][1],
                "lon": FERRY_SPIRIT_OF_BRISBANE[i][2]
            }]
            time.sleep(DELAY)

        logger.info("Starting second ferry")
        # second ferry
        for i in range(len(FERRY_SPIRIT_OF_BRISBANE)):
            ferry_data_queue.append(FERRY_SPIRIT_OF_BRISBANE[i])
            time.sleep(DELAY)


MAP_OUTPUT = "dashboard.html"

@app.websocket("/ws/position")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    try:
        while True:
            # Send the latest position as JSON
            await ws.send_text(json.dumps(current_positions))
            await asyncio.sleep(1)  # broadcast once per second
    except Exception:
        await ws.close()


if __name__ == "__main__":
    mqtt_sub_thread_handle = threading.Thread(target=mqtt_sub_thread)
    mqtt_sub_thread_handle.daemon = True
    mqtt_sub_thread_handle.start()
    ferry_data_thread_handle = threading.Thread(target=supply_ferry_data_thread)
    ferry_data_thread_handle.daemon = True
    ferry_data_thread_handle.start()
    sim_start_time = time.time()
    # send_arrival_to_speaker()
    current_volume = 10
    # send_volume_to_speaker()
    send_arrival_to_speaker()
    # send_departure_to_speaker()
    uvicorn.run(app, host='0.0.0.0', port=8000, log_level="info")
