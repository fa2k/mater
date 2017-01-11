#!/usr/bin/env python
import time, os, sys, requests, subprocess, psutil, datetime
import socket

SERVER = "http://localhost:5001/"
SERVER_SCRIPT = "/home/poppetode/feedergw.py"

#REPORT_SERVER = "http://www.fa2k.net/mater/ping.php"

host = socket.gethostbyname('www.fa2k.net')
REPORT_SERVER = "http://{0}/mater/ping.php".format(host)

def reboot():
	if len(sys.argv) != 2 or sys.argv[1] != "noreboot":
		print "Rebooting"
		try:
			subprocess.call(["/sbin/faboot"])
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
			elif p.name == "skype": # also kill skype! process may be called sk.
				serv = True
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
	

def checkSkypeUp():
	found = False
	for pid in  psutil.get_pid_list():
		p = psutil.Process(pid)
		if p.name == "skype":
			found = True
			break
	
	if found:
		print "Skype was running"
	else:
		print "Starting skype"
		env = dict(os.environ)
		env['DISPLAY'] = ":0"
		try:
			subprocess.Popen(["/usr/bin/skype"], env = env)
		except OSError:
			pass
	

def postReport():
	# get status
	try:
		rs = requests.get(SERVER+"status")
		if rs.status_code == 200:
			print "Got a good reply from local server, forwarding it"
			h = {"Content-type": "application/json", "Host": "www.fa2k.net"}
			rr = requests.post(REPORT_SERVER, data=rs.text, headers=h)
			if rr.status_code == 200 and rr.text == "OK\n":
				print "Successfully sent report"
				return True
	except requests.exceptions.RequestException:
		pass
	return False


def main():
	print "Maintenance at ", str(datetime.datetime.now())
	print "Checking that server is up"
	# ensure machine gateway service is active
	try:
		if ensureServerUp():

			print "Server is up, making sure skype is on"
			# ensure skype is up (not fatal if it can't be started)
			checkSkypeUp()

			
			print "Reporting to report server"
			# if we can't post report, maybe reboot
			if postReport():
				print "Finished, all good"
			else:
				print "Report failure, maybe a reboot will help"
				reboot()
	except:
		print "Exception, rebooting"
		reboot()

 
if __name__ == "__main__":
	main()

