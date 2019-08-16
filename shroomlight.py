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
import time

class ShroomLight:
	def __init__(self, mac, version, gridx, gridy, gridz):
		self.mac = mac
		self.version = version
		self.gridx = gridx
		self.gridy = gridy
		self.gridz = gridz

	def __repr__(self):
		return 'Shroom %s V%s %d %d %d' % (self.mac, self.version, self.gridx, self.gridy, self.gridz)

class ShroomLights:
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
		self.shrooms = {}

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
				self.parseMulticast(data)
		print("Stop multicast listener")

	def parseMulticast(self, data):
		print('[%d] %s' % (len(data), data))
		message = data.decode('utf-8')
		args = message.split()
		#Parse shroom
		if len(args) == 8 and args[0] == 'Shroom' and args[2] == 'Version' and args[4] == 'Grid':
			mac = args[1]
			version = args[3]
			x = int(args[5])
			y = int(args[6])
			z = int(args[7])
			if mac in self.shrooms:
				#Just modify
				self.shrooms[mac].version = version
				self.shrooms[mac].gridx = x
				self.shrooms[mac].gridy = y
				self.shrooms[mac].gridz = z
			else:
				#Create new
				self.shrooms[mac] = ShroomLight(mac, version, x, y, z)
			#print(self.shrooms[mac])


	def information(self):
		self.sock.sendto(b'information', self.sending_multicast_group)

	def lightmode(self, mac, mode):
		out = 'LIGHTMODE %s %d' % (mac, mode)
		self.sock.sendto(out.encode(), self.sending_multicast_group)

	def triggershroom(self, mac):
		x = self.shrooms[mac].gridx
		y = self.shrooms[mac].gridy
		z = self.shrooms[mac].gridz
		out = 'TRIGGER %s %d %d %d' % (mac, x, y, z)
		self.sock.sendto(out.encode(), self.sending_multicast_group)

	def setgridaddress(self, mac, x, y, z):
		out = 'SETGRID %s %d %d %d' % (mac, x, y, z)
		self.sock.sendto(out.encode(), self.sending_multicast_group)

	def otaspecific(self, mac):
		out = 'SOTA %s %s' % (mac, self.getHttpBuild())
		self.sock.sendto(out.encode(), self.sending_multicast_group)

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

	def findMac(self, searchkey):
		out = []
		for key in self.shrooms.keys():
			if re.match('.*'+searchkey+'.*', key):
				out.append(key)
		return out

def usage():
	print ("--help : shows this help")

def commandUsage():
	print("q           - Exit this command")
	print("i           - Report MAC, version and physical grid address")
	print("r           - Restart shrooms")
	print("o           - Do a OTA (Over the air upgrade)")
	print("c           - Do a OTA for detected units one at a time")
	print("s MAC       - Do a OTA on a specific ShroomLight")
	print("l MAC Mode  - Set light mode")
	print("t MAC       - Trigger shroom")
	print("x MAC X Y Z - Set shroom grid address")

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
	shroomcommander = ShroomLights()
	signal.signal(signal.SIGINT, shroomcommander.stopSignal)
	signal.signal(signal.SIGTERM, shroomcommander.stopSignal)
	print("Shroomlight")
	shroomcommander.information()
	while shroomcommander.keepListening:
		s = input("Shroom command: ")
		args = s.split()
		if s == 'h':
			commandUsage()
		if s == 'r':
			shroomcommander.restart()
		elif s == 'i':
			shroomcommander.information()
		elif s == 'o':
			shroomcommander.ota()
		elif s == 'c':
			for mac in shroomcommander.shrooms.keys():
				shroomcommander.otaspecific(mac)
				previousversion = shroomcommander.shrooms[mac].version
				start = time.time()
				delta = 0
				timeout = 30
				while previousversion == shroomcommander.shrooms[mac].version and delta < timeout:
					delta = time.time() - start
					#Sleep until unit is updated
					print("Wait for OTA of %s %0.2f" % (mac, delta))
					time.sleep(10.5)
				if delta > timeout:
					print("OTA timeout")
			print("OTA done")

		elif s.startswith('s') and len(args) == 2:
			#print('Specific OTA |%s|' % args[1])
			res = shroomcommander.findMac(args[1])
			if len(res) == 0:
				print('No MAC address found by %s' % args[1])
				continue
			if len(res) > 1:
				print('%s matced too many MAC addresses %s' % (args[1], res))
				continue
			print('Specific OTA %s' % res[0])
			shroomcommander.otaspecific(res[0])
		elif s.startswith('l') and len(args) == 3:
			res = shroomcommander.findMac(args[1])
			if len(res) == 0:
				print('No MAC address found by %s' % args[1])
				continue
			if len(res) > 1:
				print('%s matced too many MAC addresses %s' % (args[1], res))
				continue
			mode = int(args[2])
			print('Light Mode MAC %s to %d' % (res[0], mode))
			shroomcommander.lightmode(res[0], int(args[2]))
		elif s.startswith('t') and len(args) == 2:
			res = shroomcommander.findMac(args[1])
			if len(res) == 0:
				print('No MAC address found by %s' % args[1])
				continue
			if len(res) > 1:
				print('%s matced too many MAC addresses %s' % (args[1], res))
				continue
			x = shroomcommander.shrooms[res[0]].gridx
			y = shroomcommander.shrooms[res[0]].gridy
			z = shroomcommander.shrooms[res[0]].gridz
			print('Trigger shroom %s %d %d %d' % (res[0], x, y, z))
			shroomcommander.triggershroom(res[0])
		elif s.startswith('x') and len(args) == 5:
			res = shroomcommander.findMac(args[1])
			if len(res) == 0:
				print('No MAC address found by %s' % args[1])
				continue
			if len(res) > 1:
				print('%s matced too many MAC addresses %s' % (args[1], res))
				continue
			x = int(args[2])
			y = int(args[3])
			z = int(args[4])
			print('Set shroom %s grid to %d %d %d' % (res[0], x, y, z))
			shroomcommander.setgridaddress(res[0], x, y, z)
		elif s == 'q':
			print('Exit')
			shroomcommander.stop()
		else:
			print('Not a valid command. Type h for help.')

