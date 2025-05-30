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

MMSI_NEVILLE_BONNER = 503102970
MMSI_SPIRIT_OF_BRISBANE = 503586200
MMSI_KURILPA = 503575300

# Ferrys to display on the dashboard
ferrys = [MMSI_NEVILLE_BONNER, MMSI_SPIRIT_OF_BRISBANE, MMSI_KURILPA]

# Coordinate index of each ferry
# NEVILLE_BONNER, SPIRIT_OF_BRISBANE, KURILPA
ferry_index = [0, 25, 55]

FERRY_PATH_COORDS = [
    [-27.49654111297432, 153.01956958655086],
    [-27.49646160675099, 153.01958626195818],
    [-27.496367308646388, 153.01962169727514],
    [-27.49621939004738, 153.01966755459827],
    [-27.49611030006049, 153.01970298974024],
    [-27.495999361152645, 153.01970298945147],
    [-27.49589396917427, 153.0197009047569],
    [-27.495881025599633, 153.0196967371281],
    [-27.495491094981887, 153.01965052185457],
    [-27.495293150009164, 153.01956964806757],
    [-27.495071291863603, 153.0194064039956],
    [-27.494796293507864, 153.01921320701456],
    [-27.49454653550824, 153.01902749770335],
    [-27.494194482231755, 153.01869352112945],
    [-27.494093516013432, 153.01859767153417],
    [-27.493914167474614, 153.01842394351414],
    [-27.493700277525285, 153.01817683069282],
    [-27.493503657459904, 153.01794169941226],
    [-27.493503657459904, 153.01794169941226],
    [-27.493180828274834, 153.0175388308949],
    [-27.492978505437687, 153.01727360466853],
    [-27.492817754193315, 153.01703547815265],
    [-27.492667630740367, 153.0168407840753],
    [-27.49250422223549, 153.0166011585954],
    [-27.492370041619715, 153.01641395163276],
    [-27.492192018638974, 153.01614736978695],
    [-27.491974138919602, 153.01578943056398],
    [-27.491802758735705, 153.01549439223527],
    [-27.491618092285385, 153.01518587553292],
    [-27.491497195880864, 153.0149013214007],
    [-27.491369656629804, 153.01456584715336],
    [-27.491250086968737, 153.0142812931469],
    [-27.491149118756752, 153.0138963964363],
    [-27.491066749322638, 153.01348903463924],
    [-27.490984378055728, 153.01309365397083],
    [-27.49094186630349, 153.01274619893806],
    [-27.490908652979144, 153.01237328342143],
    [-27.490872781206633, 153.01193896379792],
    [-27.490848868482274, 153.01153010469025],
    [-27.490831596011127, 153.01101341391305],
    [-27.490834254253592, 153.01062252686884],
    [-27.490836910183166, 153.0102630897986],
    [-27.490851524714756, 153.0097733576089],
    [-27.49085019639813, 153.00932106650802],
    [-27.49086082372986, 153.0087879015756],
    [-27.49086613815559, 153.00831164803577],
    [-27.490890052559234, 153.00774853133345],
    [-27.490933893656205, 153.0070401415909],
    [-27.49097375035064, 153.006514465433],
    [-27.491013606907124, 153.00586897738035],
    [-27.490939770596906, 153.00486096429955],
    [-27.490746325821085, 153.0039719155233],
    [-27.490587587099323, 153.00315555939522],
    [-27.491098472329842, 153.00276413950803],
    [-27.49164407785921, 153.00237271370355],
    [-27.492075642994845, 153.00123204933016],
    [-27.49169371867366, 152.99994600567905],
    [-27.491321693239513, 152.99882211020144],
    [-27.490984413010334, 152.99768703555202],
    [-27.490657032301524, 152.9970272404906],
    [-27.490299910160275, 152.99630592573135],
    [-27.489927887267257, 152.99580828544555],
    [-27.48957571956819, 152.99552869559935],
    [-27.48891104026664, 152.99518203427755],
    [-27.488231484617813, 152.99510376939375],
    [-27.487750341942846, 152.99518765324947],
    [-27.486993562869948, 152.99543571111596],
    [-27.48622262261095, 152.99581122088833],
    [-27.485613483809995, 152.99611162508637],
    [-27.4854421775095, 152.99616527151238],
    [-27.4847854335487, 152.99655687856583],
    [-27.48424290586655, 152.99687337924524],
    [-27.48374796816487, 152.9971255042204],
    [-27.48340055924328, 152.9973561709974],
    [-27.483072188142504, 152.99732934580533]
]

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

    logger.info(f"Disco got volume change: {change}")

    return {
        "status": 200,
        "change": change
    }


@app.get("/rtc")
async def get_time():
    year, month, day, hour, minute, second = time.localtime()[:6]

    return {
        "year": year,
        "mon": month,
        "day": day,
        "hour": hour,
        "min": minute,
        "sec": second
    }


def send_volume_to_speaker(volume):
    logger.info(f"Sending volume to speaker: {volume}")
    client.publish(MQTT_SPEAKER_TOPIC, payload=f"Volume {volume}", qos=1)


class VolumeReq(BaseModel):
    volume: int

volume = 100

@app.get("/currentvolume")
async def get_current_volume():
    return {
        "volume": volume
    }


@app.post("/volume")
async def change_volume(vol: VolumeReq):
    global volume
    volume = vol.volume
    send_volume_to_speaker(vol.volume)
    return {"status": "ok"}



class VolumeChange(BaseModel):
    change: int

@app.post("/volume_value")
async def change_volume(vol: VolumeChange):
    volume_changes.append(vol.change)  # Append the change to the queue
    logger.info(f"Dashboard vol change: {vol.change}")
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

# Global list of dictionaries for each ferry being displayed
# Each dictionary should contain {"mmsi": int, "lat": int, "lon": int}
current_position_data = []  

def supply_ferry_data_thread():
    global current_position_data

    DELAY = 2.5

    ferry_dir = [-1, -1, -1]

    while True:
        # Move ferry's index to next coordinate for all ferries in the list
        for i in range(len(ferrys)):  # ferrys: list[int]
            # logger.info("Ferry index")
            ferry_index[i] += 1 * ferry_dir[i] # Increase index for the ferry
        # Add ferry to queue with updated coordinate
        for ferry_no, mmsi in enumerate(ferrys):
            if ferry_index[ferry_no] >= len(FERRY_PATH_COORDS):
                # Assert the current_ferry_index is at the end bound of the list
                # ferry_index[ferry_no] = 0  # Reset to the first list position
                ferry_dir[ferry_no] = ferry_dir[ferry_no] * -1
                ferry_index[ferry_no] -= 1
                # Get the current index for the updated current ferry
                current_ferry_index = ferry_index[ferry_no]
            elif ferry_index[ferry_no] <= 0:
                ferry_dir[ferry_no] = ferry_dir[ferry_no] * -1
                ferry_index[ferry_no] = 1
                current_ferry_index = ferry_index[ferry_no]
            else:
                # Assert the current ferry index is not at the bounds of the list
                current_ferry_index = ferry_index[ferry_no]

            # Return the coordinates associated with the current ferry index
            lat, long = FERRY_PATH_COORDS[current_ferry_index]
            # logger.info(f"Ferry No. {ferry_no}, Ferry data: {mmsi} {lat} {long}")

            # Display current position on dashboard
            # Add ferry data for each ferry to the current position list,
            # Each ferry should only have one data point which is updated,
            # Each unique mmsi is displayed
            # Add to the list of dictionaries, so for each ferry index
            # update the corresponding ferry dictionary
            # current_position_data = [{}, {}, {}]
            if ferry_no >= len(current_position_data):
                # Assert the ferry is not in the current_position_data
                current_position_data.append({
                    "mmsi": mmsi,
                    "lat": lat,
                    "lon": long
                })
            else:
                # Assert the ferry is in the current_position_data
                current_position_data[ferry_no] = {
                    "mmsi": mmsi,
                    "lat": lat,
                    "lon": long
                }
            # FOR BASENODE: Append a list to the queue [MMSI, LAT, LONG]  
            ferry_data_queue.append([mmsi, lat, long])

            if len(ferry_data_queue) >= 24:
                # Assert ferry_data_queue has 6 or more elements
                data = ferry_data_queue.pop(0)  # pop an element to save memory

            if len(volume_changes) >= 6:
                volume_changes.pop(0)

        # Delay before updating to next coordinate
        time.sleep(DELAY)


MAP_OUTPUT = "dashboard.html"

@app.websocket("/ws/position")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    try:
        while True:
            # Send the latest position as JSON
            await ws.send_text(json.dumps(current_position_data))
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
