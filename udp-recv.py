import sys
import socket
import binascii
import ctypes
from bitstring import BitArray, BitStream

UDP_IP = "127.0.0.1"
UDP_PORT = 40868

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

while True:
	try:
		data, addr = sock.recvfrom(1024)
		data = data.split(b'\x5e\xd5')[1]
		data = data.split(b'\r\n')[0]

		tlm = BitStream(data)
		print(tlm.bin)
		fps, err, Ax, Ay, Az, Gx, Gy, Gz, roll, pitch, yaw, alt, temp, volts, resv = tlm.readlist('bits:6, bits:6, int:6, int:6, int:9, int:10, int:10, int:12, int:9, int:9, int:9, uint:12, int:8, uint:8, bits:8')

		Ax /= 10
		Ay /= 10
		Az /= 10
		volts /= 10

		print("FPS = {}".format(fps))
		print("err = {}".format(err))
		print(" Ax = {}".format(Ax))
		print(" Ay = {}".format(Ay))
		print(" Az = {}".format(Az))
		print(" Gx = {}".format(Gx))
		print(" Gy = {}".format(Gy))
		print(" Gz = {}".format(Gz))
		print("rol = {}".format(roll))
		print("pit = {}".format(pitch))
		print("yaw = {}".format(yaw))
		print("alt = {}".format(alt))
		print(" C  = {}".format(temp))
		print(" V  = {}".format(volts))
	except bitstring.ReadError:
		print("BS ReadError")
		continue
	except KeyboardInterrupt:
		sys.exit()