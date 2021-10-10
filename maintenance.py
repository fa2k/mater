#!/usr/bin/env python
import time, os, sys, requests, subprocess, psutil, datetime
import socket
import miniupnpc

SERVER = "http://localhost:5001/"
SERVER_SCRIPT = "/home/pi/feedergw.py"

REPORT_SERVER = "http://www.fa2k.net/mater/ping.php"
REPORT_HOSTNAME = "www.fa2k.net"

host = None
i = 0
while not host:
	try:
		host = socket.gethostbyname(REPORT_HOSTNAME)
	except:
		if i > 4:
			raise
		time.sleep(30)
		i += 1


def reboot():
	if len(sys.argv) != 2 or sys.argv[1] != "noreboot":
		print "Rebooting"
		try:
			subprocess.call(["sudo", "/sbin/reboot"])
			pass
		except OSError:
			print "Shit deep like the Mariana Trench"
	else:
		print "Not rebooting due to boot loop"


def ensureServerUp():
	for itry in [0,1,2]:
		try:
			rq = requests.get(SERVER+"server-status", timeout=5.0)
			if rq.status_code == 200 and rq.text == "OK":
				print "Got a good reply from server, continuing"
				return True
			else:
				# we're in deep shit, server is running, but returns bad data
				# try to restart computer
				reboot()
				return False
		except requests.exceptions.RequestException:
			pass # ordinary fail, server probably died

		print "Could not contact server, trying to kill it then start one"
		for pid in  psutil.get_pid_list():
			p = psutil.Process(pid)
			serv = False # determine if it's the server
			if p.name == "python":
				serv = len(p.cmdline) > 1 and p.cmdline[1] == SERVER_SCRIPT
			if serv:
				print "Killing ", pid
				p.terminate()
				p.wait(3000)


		# try to start the server process
		subprocess.Popen(["/usr/bin/python", SERVER_SCRIPT])

		# wait a little while for server to come up
		time.sleep(10)
	
	# can't start the server on 3 tries, reboot!
	reboot()
	return False
	

def postReportGetPublicIp():
	# get status
	try:
		rs = requests.get(SERVER+"status")
		if rs.status_code == 200:
			print "Got a good reply from local server, forwarding it"
			h = {"Content-type": "application/json", "Host": REPORT_HOSTNAME}
			rr = requests.post(REPORT_SERVER, data=rs.text, headers=h)
			if rr.status_code == 200:
				print "Successfully sent report, and received our IP,", rr.text.strip()
				return rr.text
	except requests.exceptions.RequestException:
		pass
	return False


def checkPortForwarding(publicIp):
	try:
		rs = requests.get("http://{}:5001/status".format(publicIp))
		if rs.status_code == 200:
			print "Got a good reply from local server through forwarded port."
			return True
	except requests.exceptions.RequestException:
		pass
	print "Port forwarding is not working."
	return False


def fixPortForwarding():
	upnp = miniupnpc.UPnP()
	upnp.discoverdelay = 10
	upnp.discover()
	upnp.selectigd()
	print "Adding port forwarding for ports 5001 & 5003 to", upnp.lanaddr, "..."
	upnp.addportmapping(5001, 'TCP', upnp.lanaddr, 5001, 'Mater', '')
	upnp.addportmapping(5003, 'TCP', upnp.lanaddr, 5003, 'Mater-video', '')


def main():
	print "Maintenance at ", str(datetime.datetime.now())
	print "Checking that server is up"
	# ensure machine gateway service is active
	try:
		if ensureServerUp():

			print "Reporting to report server"
			# if we can't post report, maybe reboot
			public_ip = postReportGetPublicIp()
			if public_ip:
				if not checkPortForwarding(public_ip):
					print "Fixing port forwarding with UPnP..."
					fixPortForwarding()
				print "Finished, all good"
			else:
				print "Report failure, maybe a reboot will help"
				reboot()
	except Exception as e:
		print "Exception {}, rebooting".format(e)
		reboot()

 
if __name__ == "__main__":
	main()

