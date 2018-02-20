import sys
import serial
import binascii
import socket

UDP_IP = "127.0.0.1"
UDP_PORT = 40868

ser = serial.Serial('/dev/ttyUSB2', 115200)

while True:
	try:
		line = ser.readline()
		print(binascii.hexlify(line))

		sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		sock.sendto(line, (UDP_IP, UDP_PORT))
	except KeyboardInterrupt:
		ser.close()
		sys.exit()