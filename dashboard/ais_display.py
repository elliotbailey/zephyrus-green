import socket
import pyais
import folium
import csv
import time
# from pathlib import Path


vessels = {}

CSV_FILE_NAME = "ais_log.csv"

MAP_OUTPUT = "ais_map.html"
UPDATE_EVERY_N_MESSAGES = 1
message_count = 0

# open csv file
fd = open(CSV_FILE_NAME, 'a', newline='')
writer = csv.writer(fd)

def update_map():
    fmap = folium.Map(location=[-27.47, 153.02], zoom_start=12)

    for mmsi, (lat, lon) in vessels.items():
        folium.Marker(
            location=[lat, lon],
            popup=f"MMSI: {mmsi}",
            icon=folium.Icon(color="blue", icon="compass", prefix="fa")
        ).add_to(fmap)

    fmap.save(MAP_OUTPUT)
    print(f"[+] Map updated with {len(vessels)} vessels.")


with socket.create_connection(("127.0.0.1", 10111)) as s:
    for line in s.makefile("r", newline="\n", encoding="ascii", errors="replace"):
        try:
            msg = pyais.decode(line.strip())
            print(msg.asdict())
            data = msg.asdict()

            lat = data.get("lat")
            lon = data.get("lon")
            mmsi = data.get("mmsi")
            msg_type = data.get("mmsi")

            if msg_type == 18:
                # packet indicates a ship
                now_ts = int(time.time_ns() / 1000000)
                writer.writerow([now_ts, mmsi, lat, lon])
                fd.flush()

            # Ensure valid coordinates
            if lat is not None and lon is not None:
                vessels[mmsi] = (lat, lon)
                message_count += 1

            if message_count >= UPDATE_EVERY_N_MESSAGES:
                update_map()
                message_count = 0

        except Exception as e:
            print(f"[!] Failed to parse line: {e}")
