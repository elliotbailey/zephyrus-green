import serial
from collections import deque
from pynput import keyboard
import csv
import threading
import time

PORT = 'COM3'
RATE = 115200
WINDOW = 30
OUT_CSV = 'new_data.csv'

current_label = 0
status = False
recording = True

# Double Ended Queue
buff_queue = deque(maxlen=WINDOW)

def category_select(key):
    key_list = ['1', '2', '3', '4', '5', '6']
    try:
        if key.char in key_list:
            print("Key Pressed")
            global current_label
            current_label = int(key.char)
            global status
            status = True
        if key == keyboard.Key.esc:
            global recording
            recording = False
            return False
    except AttributeError:
        pass

def obtain_values():
    ser = serial.Serial(PORT, RATE)
    with open(OUT_CSV, mode='a', newline='') as f:
        writer = csv.writer(f)
        header = [f"dist_{i}" for i in range(WINDOW)] + ["label"]
        writer.writerow(header)
        global buff_queue
        global status
        global recording
        #time0 = time.time()
        while recording:
            if status:
                ser.reset_input_buffer()
                sent_val = ser.readline().decode().strip()
                try:
                    #print(str(len(buff_queue)))
                    value = float(sent_val)
                    buff_queue.append(value)
                    if len(buff_queue) == WINDOW:
                        writer.writerow(list(buff_queue) + [current_label])
                        buff_queue.clear()
                        status = False
                        print("Done")
                    #time0 = time.time() - time0
                    #print(str(time0))
                except ValueError:
                    pass
            else:
                time.sleep(0.05)
    ser.close()
    #print(f"Data saved to {OUT_CSV}")

listener = keyboard.Listener(on_press=category_select)
listener.start()
obtain_values()
listener.join()