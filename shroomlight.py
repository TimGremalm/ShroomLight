#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket
import struct
import threading
import select
import sys
import getopt
import signal

class ShroomLight:
	def __init__(self):
		self.multicast_group = '239.255.0.1'
		self.port = 10420
		self.sending_multicast_group = ('239.255.0.1', 10420)
		self.server_address = ('', self.port)
		self.keepListening = True
		self.t = threading.Thread(target=self.listener, args=())
		self.t.start()

	def stopSignal(self, signum, frame):
		print("Recives Signal, stop")
		self.keepListening = False

	def stop(self):
		self.keepListening = False

	def listener(self):
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
		self.sock.bind(self.server_address)

		group = socket.inet_aton(self.multicast_group)
		mreq = struct.pack('4sL', group, socket.INADDR_ANY)
		self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
		self.sock.setblocking(0)

		while self.keepListening:
			ready = select.select([self.sock], [], [], 1)
			if ready[0]:
				data, address = self.sock.recvfrom(1024)
				print('received %d bytes from %s' % (len(data), address))
				print(data)
		print("Stop listener")

	def ota(self):
		self.sock.sendto(b'YO OTA', self.sending_multicast_group)

	def restart(self):
		self.sock.sendto(b'YO restart', self.sending_multicast_group)

def usage():
	print ("--help : shows this help")

def commandUsage():
	print("r - restart shrooms")
	print("o - Do a OTA (Over the air upgrade)")

def parseArgs():
	try:
		opts, args = getopt.getopt(sys.argv[1:], "h", ["help"])
	except getopt.GetoptError as err:
		print(err)
		usage()
		sys.exit(2)
	for o, a in opts:
		if o in ("-h", "--help"):
			usage()
			sys.exit(0)
		else:
			assert False, "unhandled option"

if __name__ == '__main__':
	parseArgs()
	import time
	shroom = ShroomLight()
	signal.signal(signal.SIGINT, shroom.stopSignal)
	signal.signal(signal.SIGTERM, shroom.stopSignal)
	print("Shroomlight")
	while shroom.keepListening:
		s = input("Shroom command: ")
		if s == 'r':
			shroom.restart()
		elif s == 'o':
			shroom.ota()
		else:
			commandUsage()

