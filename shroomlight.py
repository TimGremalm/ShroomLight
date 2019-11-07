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
from websocket_server import WebsocketServer #pip3 install git+https://github.com/Pithikos/python-websocket-server


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
		self.websocketport = 9001
		self.sending_multicast_group = ('239.255.0.1', 10420)
		self.server_address = ('', self.multicastport)
		self.keepListening = True
		self.t = threading.Thread(target=self.multicastlistener, args=())
		self.t.start()
		self.httpdTread = threading.Thread(target=self.webserver, args=())
		self.httpdTread.start()
		self.wsThread = threading.Thread(target=self.websocketServerThread, args=())
		self.wsThread.start()
		self.shrooms = {}

	def websocketReceive(self, client, server, message):
		# send out incoming message out on the shroom multicast network
		print("Websocket receieved %s" % message)
		self.sock.sendto(bytes(message,"ascii"), self.sending_multicast_group)

	def websocketServerThread(self):
		self.websocketsrv = WebsocketServer(port=self.websocketport, host='0.0.0.0')
		self.websocketsrv.set_fn_message_received(self.websocketReceive)
		print("Serve Websocket server on port %d" % self.websocketport)
		self.websocketsrv.run_forever()

	def stopSignal(self, signum, frame):
		print("Recives Signal, stop")
		self.keepListening = False
		self.httpd.shutdown()

	def stop(self):
		self.keepListening = False
		self.httpd.shutdown()
		self.websocketsrv.shutdown()

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
		try:
		    self.websocketsrv.send_message_to_all(message)
		except AttributeError:
		    pass

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

	def shroomwave(self, mac, hops, wavegeneration, x, y, z, uniqueu):
		out = 'SHROOM %s %d %d %d %d %d %d' % (mac, hops, wavegeneration, x, y, z, uniqueu)
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
		print("Serve HTTP server on port %d" % self.webserverport)
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

	def commandUsage(self):
		sOut = ""
		sOut += "q                  - Exit this command\n"
		sOut += "i                  - Report MAC, version and physical grid address\n"
		sOut += "r                  - Restart shrooms\n"
		sOut += "o                  - Do a OTA (Over the air upgrade)\n"
		sOut += "c                  - Do a OTA for detected units one at a time\n"
		sOut += "s MAC              - Do a OTA on a specific ShroomLight\n"
		sOut += "l MAC Mode         - Set light mode\n"
		sOut += "t MAC              - Trigger shroom\n"
		sOut += "x MAC X Y Z        - Set shroom grid address\n"
		sOut += "w MAC hops wavegen - Send light wave\n"
		return sOut

	def parseInput(self, s):
		args = s.split()
		sOut = ""
		if s == 'h':
			sOut = self.commandUsage()
		if s == 'r':
			self.restart()
		elif s == 'i':
			self.information()
		elif s == 'o':
			self.ota()
		elif s == 'c':
			for mac in self.shrooms.keys():
				self.otaspecific(mac)
				previousversion = self.shrooms[mac].version
				start = time.time()
				delta = 0
				timeout = 60
				while previousversion == self.shrooms[mac].version and delta < timeout:
					delta = time.time() - start
					#Sleep until unit is updated
					sOut += "Wait for OTA of %s %0.2f\n" % (mac, delta)
					time.sleep(10.5)
				if delta > timeout:
					sOut += "OTA timeout\n"
			sOut += "OTA done\n"
		elif s.startswith('s') and len(args) == 2:
			#sOut += 'Specific OTA |%s|' % args[1]
			res = self.findMac(args[1])
			if len(res) == 0:
				sOut += 'No MAC address found by %s\n' % args[1]
				return sOut
			if len(res) > 1:
				sOut += '%s matced too many MAC addresses %s\n' % (args[1], res)
				return sOut
			sOut += 'Specific OTA %s\n' % res[0]
			self.otaspecific(res[0])
		elif s.startswith('l') and len(args) == 3:
			res = self.findMac(args[1])
			if len(res) == 0:
				sOut += 'No MAC address found by %s\n' % args[1]
				return sOut
			if len(res) > 1:
				sOut += '%s matced too many MAC addresses %s\n' % (args[1], res)
				return sOut
			mode = int(args[2])
			sOut += 'Light Mode MAC %s to %d\n' % (res[0], mode)
			self.lightmode(res[0], int(args[2]))
		elif s.startswith('t') and len(args) == 2:
			res = self.findMac(args[1])
			if len(res) == 0:
				sOut += 'No MAC address found by %s\n' % args[1]
				return sOut
			if len(res) > 1:
				sOut += '%s matced too many MAC addresses %s\n' % (args[1], res)
				return sOut
			x = self.shrooms[res[0]].gridx
			y = self.shrooms[res[0]].gridy
			z = self.shrooms[res[0]].gridz
			sOut += 'Trigger shroom %s %d %d %d\n' % (res[0], x, y, z)
			self.triggershroom(res[0])
		elif s.startswith('x') and len(args) == 5:
			res = self.findMac(args[1])
			if len(res) == 0:
				sOut += 'No MAC address found by %s\n' % args[1]
				return sOut
			if len(res) > 1:
				sOut += '%s matced too many MAC addresses %s\n' % (args[1], res)
				return sOut
			x = int(args[2])
			y = int(args[3])
			z = int(args[4])
			sOut += 'Set shroom %s grid to %d %d %d\n' % (res[0], x, y, z)
			self.setgridaddress(res[0], x, y, z)
		elif s.startswith('w') and len(args) == 4:
			res = self.findMac(args[1])
			if len(res) == 0:
				sOut += 'No MAC address found by %s\n' % args[1]
				return sOut
			if len(res) > 1:
				sOut += '%s matced too many MAC addresses %s\n' % (args[1], res)
				return sOut
			hops = int(args[2])
			wavegeneration = int(args[3])
			x = self.shrooms[res[0]].gridx
			y = self.shrooms[res[0]].gridy
			z = self.shrooms[res[0]].gridz
			uniq = int(time.time())
			sOut += 'Shroom Wave %s Hops %d Wavegeneration %d %d %d %d %d\n' % (res[0], hops, wavegeneration, x, y, z, uniq)
			self.shroomwave(res[0], hops, wavegeneration, x, y, z, uniq)
		elif s == 'q':
			sOut += 'Exit\n'
			self.stop()
		else:
			sOut += 'Not a valid command. Type h for help.\n'
		return sOut


def usage():
	print ("--help : shows this help")

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
		print(shroomcommander.parseInput(s))

