#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import socket
import struct
import threading
import select
import sys
import getopt
import signal
import http.server
from subprocess import check_output
import re

class ShroomLight:
	def __init__(self):
		self.ip = self.getIP()
		self.multicast_group = '239.255.0.1'
		self.multicastport = 10420
		self.webserverport = 8000
		self.sending_multicast_group = ('239.255.0.1', 10420)
		self.server_address = ('', self.multicastport)
		self.keepListening = True
		self.t = threading.Thread(target=self.multicastlistener, args=())
		self.t.start()
		self.httpdTread = threading.Thread(target=self.webserver, args=())
		self.httpdTread.start()

	def stopSignal(self, signum, frame):
		print("Recives Signal, stop")
		self.keepListening = False
		self.httpd.shutdown()

	def stop(self):
		self.keepListening = False
		self.httpd.shutdown()

	def multicastlistener(self):
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
				print('[%d] %s' % (len(data), data))
		print("Stop multicast listener")

	def information(self):
		self.sock.sendto(b'information', self.sending_multicast_group)

	def ota(self):
		out = 'OTA %s' % self.getHttpBuild()
		self.sock.sendto(out.encode(), self.sending_multicast_group)

	def restart(self):
		self.sock.sendto(b'restart', self.sending_multicast_group)

	def webserver(self):
		handler = http.server.SimpleHTTPRequestHandler
		server_address = ('', self.webserverport)
		self.httpd = http.server.HTTPServer(server_address, handler)
		self.httpd.timeout = 0.5
		self.httpd.serve_forever(poll_interval=0.1)

	def getIP(self):
		hostnameIP = check_output(['hostname', '--all-ip-addresses'])
		hostnameIPstring = hostnameIP.decode('utf-8')
		extractedIPs = re.findall('\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}', hostnameIPstring)
		return extractedIPs[0]

	def getHttpBuild(self):
		return 'http://%s:%d/build/shroomlight.bin' % (self.ip, self.webserverport)

	def getHttpRoot(self):
		return 'http://%s:%d/' % (self.ip, self.webserverport)

def usage():
	print ("--help : shows this help")

def commandUsage():
	print("q - Exit this command")
	print("i - Report MAC, version and physical grid address")
	print("r - Restart shrooms")
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
		elif s == 'i':
			shroom.information()
		elif s == 'o':
			shroom.ota()
		elif s == 'q':
			print('Exit')
			shroom.stop()
		else:
			commandUsage()

