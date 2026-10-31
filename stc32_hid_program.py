# Example on how to use hid to write STC chip series: STC8H8K64U，STC32G12K128，STC32F12K54
# Opens USB HID device and programs flash. No error check. Use at own risk 
# Only tested on STC32F12K54

import hid
import time
import struct
from intelhex import IntelHex
from tqdm import tqdm

PACKET_START = bytes([0x46, 0xb9])
PACKET_END = bytes([0x16])
PACKET_MCU = bytes([0x68])
PACKET_HOST = bytes([0x6a])

h = hid.device()
h.open(0x34bf, 0x1001)
print(f'Found device: {h.get_product_string()}')
# h.set_nonblocking(1)

def send_packet(packet_data):
    packet = bytes()
    packet += PACKET_START
    packet += PACKET_HOST
    
    # packet length and payload
    packet += struct.pack(">H", len(packet_data) + 6)
    packet += packet_data
    
    # checksum and end code
    packet += struct.pack(">H", sum(packet[2:]) & 0xffff)
    packet += PACKET_END
    
    h.write(packet)
    if packet_data[0] != 0xFF: # do not read if reset package
            d = h.read(64) # every HID packet is 64 bytes: currently not read and discarded

## init and erase
send_packet(bytes([0x00, 0x00])) # start
send_packet(bytes([0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00])) # info
send_packet(bytes([0x05, 0x00, 0x00, 0x5a, 0xa5])) # unlock
send_packet(bytes([0x03, 0x00, 0x00, 0x5a, 0xa5])) # erase

ih = IntelHex()
ih.loadhex('led.hex')

for i in tqdm(range(ih.minaddr(), ih.maxaddr(), 0x80)):
    chunk = ih[i:i+0x80].tobinarray()
    packet = bytes()
    if i == ih.minaddr():
        packet += bytes([0x32]) # first package, diff number apperently
    else:
        packet += bytes([0x12])
    packet += bytes([i >> 8 & 0xff, i & 0xff])
    packet += bytes([0x5a, 0xa5])
    packet += bytes(chunk)
    send_packet(packet)

send_packet(bytes([0xFF])) # reset
