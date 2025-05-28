from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn
import time
import logging
import paho.mqtt.client as mqtt

MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883
MQTT_TOPIC = "discotest"

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
logger = logging.getLogger("zephyrus-green")

app = FastAPI()

DELAY = 10
FAKE_AIS_DATA = [
    [0 * DELAY, 503123456, -27.496767459424884, 153.01952753903188, 345],
    [1 * DELAY, 503123456, -27.492589806949766, 153.01665580689638, 347],
    [2 * DELAY, 503123456, -27.490804557080086, 153.00997428464225, 343],
    [3 * DELAY, 503123456, -27.48998333241987, 153.00341351279033, 346],
    [4 * DELAY, 503123456, -27.49119731453502, 153.0027292605113, 344],
    [5 * DELAY, 503123456, -27.492357726103666, 153.0021255085004, 348],
    [6 * DELAY, 503123456, -27.490501061703664, 152.99673199043346, 342],
    [7 * DELAY, 503123456, -27.486127065082915, 152.9959068626884, 345],
    [8 * DELAY, 503123456, -27.48321692214612, 152.99693324110694, 347],
    [9 * DELAY, 503123456, -27.480860185525977, 152.99960987504696, 343],
    [10 * DELAY, 503123456, -27.477699936920835, 153.00232675910473, 343],
    [11 * DELAY, 503123456, -27.47353969742742, 153.00568764531508, 343],
    [12 * DELAY, 503123456, -27.4725576437457, 153.00755927654893, 343],
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


@app.get("/ferry")
async def get_ferry():

    latest = 0

    for i in range(len(FAKE_AIS_DATA)):

        # Find latest packet to send
        if FAKE_AIS_DATA[i][0] > time.time() - sim_start_time:
            latest = max(0, i - 1)
            break

    if latest in already_sent_packets:
        return {
            "status": 404,
            "lat": -1,
            "lon": -1
        }

    already_sent_packets.append(latest)

    if len(already_sent_packets) >= len(FAKE_AIS_DATA):
        reset_path

    return {
        "status": 200,
        "lat": FAKE_AIS_DATA[latest][2],
        "lon": FAKE_AIS_DATA[latest][3]
    }

class VolumeReq(BaseModel):
    direction: str

@app.post("/volume")
async def change_volume(vol: VolumeReq):
    if vol.direction == "up":
        logger.info("Increase volume")
        # send to mqtt broker
        client.publish(MQTT_TOPIC, payload="Volume up", qos=1)
    else:
        logger.info("Decrease volume")
        client.publish(MQTT_TOPIC, payload="Volume down", qos=1)
    return {"status": "ok"}


if __name__ == "__main__":
    sim_start_time = time.time()
    uvicorn.run(app, host='0.0.0.0', port=8000, log_level="info")
