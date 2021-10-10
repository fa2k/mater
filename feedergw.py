# -*- coding: utf-8 -*-
from flask import Flask
import time, sys, os, subprocess, psutil, serial
from flask import jsonify, request, make_response
app = Flask(__name__)


PING_SERVER="www.fa2k.net"

# Don't start the server unless we have Internet connection
# We don't want to reset the Arduino by opening the serial
# port unless connected, otherwise the maint. script will 
# reboot after <=24h, and the 48h failsafe in the Arduino will 
# never trigger.
for i in range(50):
    ping_response = subprocess.Popen(["/bin/ping", "-c1", PING_SERVER], stdout=subprocess.PIPE).stdout.read()
    if ping_response:
        break
    else:
        time.sleep(2)
else:
    # This is called if it doesn't break
    sys.exit(1)


class feeder:
	PORTIONS_CAPACITY = 14

	STATUS_REQUEST = 's'
	FEED = 'f'
	FORCE_FEED = 'F'
	RESET = 'r'
	ENABLE_AUTO = 'a'
	DISABLE_AUTO = 'd'
	CHECKIN_AUTO = 'c'
	INCREMENT_COUNTER = 'i'

	# feed results
	SUCCESS = 's'
	NO_MORE_FOOD = 'o'
	DISPENSE_FAILURE = 'd'
	SENSOR_FAILURE_TOO_BRIGHT = 'x'
	SENSOR_FAILURE_TOO_DIM = 'y'

	def __init__(self):
		self.conn = serial.Serial(port='/dev/ttyACM0', timeout=120)

	def request(self, command):
		self.conn.write(command)
		received = self.conn.readline()
		return received.strip()

	def status(self):
		reply = self.request(feeder.STATUS_REQUEST)
		parts = reply.split(" ")
		if len(parts) == 4:
			data = {}
			data['condition'] = 0
			data['status'] = "Klar"
			data['served_portions'] = int(parts[0])
			data['portion_capacity'] = int(feeder.PORTIONS_CAPACITY)
			data['autofeeding'] = int(parts[1])
			data['autofeeding_next_event'] = long(parts[3])
			return data
		else:
			return error(self, "Ugyldige data fra maskin")

	# return codes: 0 = success, 1 = soft fail, 2 = fatal
	def feed(self, force=False):
		reply = self.request(feeder.FORCE_FEED if force else feeder.FEED)
		if reply == feeder.SUCCESS:
			return 0, "Mating ferdig"
		elif reply == feeder.NO_MORE_FOOD:
			return 1, "Ingen porsjoner er tilgjengelige"
		elif reply == feeder.DISPENSE_FAILURE:
			return 1, "Generell matefeil"
		elif reply == feeder.SENSOR_FAILURE_TOO_BRIGHT:
			return 1, "Sensorfeil (for mye lys)"
		elif reply == feeder.SENSOR_FAILURE_TOO_DIM:
			return 1, "Sensorfeil (ikke noe lys)"
		else:
			return 2, "Ugyldige data"

	def setAuto(self, state):
		if state:
			reply = self.request(feeder.ENABLE_AUTO)
		else:
			reply = self.request(feeder.DISABLE_AUTO)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ikke OK"

	def count(self):
		reply = self.request(feeder.INCREMENT_COUNTER)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ugyldige data"

	def checkin(self):
		reply = self.request(feeder.CHECKIN_AUTO)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ugyldige data"

	def reset(self):
		reply = self.request(feeder.RESET)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ugyldige data"

	def error(self, message):
		errordata = {}
		errordata['condition'] = 2
		errordata['status'] = message
		return errordata

class feeder_fake:

	def __init__(self):
		self.fakeportions = 0

	def status(self):
		reply = self.fakerequest(feeder.STATUS_REQUEST)
		parts = reply.split(" ")
		if len(parts) == 4:
			data = {}
			data['condition'] = 0
			data['status'] = "Klar"
			data['served_portions'] = int(parts[0])
			data['portion_capacity'] = int(feeder.PORTIONS_CAPACITY)
			data['autofeeding'] = int(parts[1])
			data['autofeeding_next_event'] = long(parts[3])
			return data
		else:
			return error(self, "Ugyldige data fra maskin")

	# return codes: 0 = success, 1 = soft fail, 2 = fatal
	def feed(self, force=False):
		reply = self.fakerequest(feeder.FEED)
		if reply == feeder.SUCCESS:
			return 0, "Mating ferdig"
		elif reply == feeder.NO_MORE_FOOD:
			return 1, "Ingen porsjoner er tilgjengelige"
		elif reply == feeder.DISPENSE_FAILURE:
			return 1, "Generell matefeil"
		else:
			return 2, "Ugyldige data"

	def setAuto(self, state):
		if state:
			reply = self.fakerequest(feeder.ENABLE_AUTO)
		else:
			reply = self.fakerequest(feeder.DISABLE_AUTO)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ikke OK"

	def checkin(self):
		reply = self.fakerequest(feeder.CHECKIN_AUTO)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ugyldige data"

	def reset(self):
		reply = self.fakerequest(feeder.RESET)
		if reply == "OK":
			return 0, "OK"
		else:
			return 2, "Ugyldige data"

	def error(self, message):
		errordata = {}
		errordata['condition'] = 2
		errordata['status'] = message
		return errordata

	def fakerequest(self, command):
		if command == feeder.STATUS_REQUEST:
			return "%d 1 0 7200000" % self.fakeportions
		elif command == feeder.FEED:
			time.sleep(6)
			self.fakeportions += 1
			return feeder.SUCCESS
		elif command == feeder.CHECKIN_AUTO:
			return "OK"
		elif command == feeder.RESET:
			self.fakeportions = 0
			return "OK"


def doReboot():
	subprocess.Popen(["sudo", "/sbin/reboot"])


##### main API interface ####
f = feeder()

def corsify(res):
	res.headers["Access-Control-Allow-Methods"]="POST, GET, OPTIONS, PUT, DELETE"
	res.headers["Access-Control-Allow-Headers"]="Content-Type"
	try:
		origin = request.headers['Origin'] # fail if no origin !
	except:
		origin = "*"
	res.headers["Access-Control-Allow-Origin"] = origin
	return res
		
def returnStatusOp(commandTuple):
	code = commandTuple[0]
	message = commandTuple[1]
	if code == 0 or code == 1:
		status = f.status()
		status['condition'] = code
		status['operation'] = message
		return corsify(jsonify(status))
	else:
		return corsify(jsonify(f.error(message)))
	

@app.route('/')
def hello_world():
	return 'Tode feeder private web interface'

# machine related
@app.route('/status', methods=['GET'])
def status():
	return corsify(jsonify(f.status()))

@app.route('/feed', methods=['POST'])
def feed():
	return returnStatusOp(f.feed())

@app.route('/feed-force', methods=['POST'])
def feed_force():
	return returnStatusOp(f.feed(True))

@app.route('/count', methods=['POST'])
def count():
	return returnStatusOp(f.count())

@app.route('/auto/<state>', methods=['POST'])
def setAuto(state):
	result = f.setAuto(True if state == "enable" else False)
	return returnStatusOp(result)

@app.route('/checkin', methods=['POST'])
def checkin():
	return returnStatusOp(f.checkin())

@app.route('/reset', methods=["POST"])
def reset():
	return returnStatusOp(f.reset())

# miscellaneous / management
@app.route('/server-status', methods=['GET'])
def server_status():
	return "OK" # trivial, for maint. script 

@app.route('/reboot', methods=["POST"])
def reboot():
	doReboot()
	return corsify(make_response(""))

if __name__ == '__main__':
	app.run(host='0.0.0.0', port=5001, threaded=True)

