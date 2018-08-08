#include <AFMotor.h>
#include <EEPROM.h>

const int SWITCH_PIN = 4, IRSENS_APIN = A0;

AF_DCMotor motor(1);
AF_DCMotor irled(2);

// Protocol
const char STATUS_REQUEST = 's', FEED = 'f', FORCE_FEED = 'F', RESET = 'r';
const char ENABLE_AUTO = 'a', DISABLE_AUTO = 'd', CHECKIN_AUTO = 'c';
const char INCREMENT_COUNTER = 'i';
// Status codes from feed routine
const int SUCCESS = 's', NO_MORE_FOOD = 'o', DISPENSE_FAILURE = 'd', SENSOR_FAILURE = 'x';

const long COMMAND_TIMEOUT = 60000;

// How much hardware
const int N_PORTIONS = 14;

// Config
const unsigned long AUTO_FEED_INITIATE_TIMEOUT = 2ul*24*3600*1000;
const unsigned long AUTO_FEED_REPEAT = 24ul*3600*1000;

// Operational consts
const int BELT_OP_TIMEOUT = 5000, FIRST_TIMEOUT = 10000;

// Operation globals
boolean switchState;
// Auto feed
unsigned long lastInteraction, lastAutoFeed;
boolean autoFeedModeActive, autoFeedEnabled;

void setup() {
  pinMode(SWITCH_PIN, INPUT);
  digitalWrite(SWITCH_PIN, HIGH); // enable pull-up  
  Serial.begin(9600);
  switchState = digitalRead(SWITCH_PIN);
  lastInteraction = millis();
  autoFeedModeActive = false;
  autoFeedEnabled = getAutoFeed(); // cache
}

void loop() {
  // Check for new command
  
  if (Serial.available() > 0) {
    byte action = Serial.read();
    byte result;
    switch(action) {
      case STATUS_REQUEST:
        Serial.print(getUsedPortions());
        Serial.print(" ");
        Serial.print(getAutoFeed());
        Serial.print(" ");
        Serial.print(autoFeedModeActive);
        Serial.print(" ");
        if (autoFeedModeActive) {
          Serial.println(AUTO_FEED_REPEAT + lastAutoFeed - millis());
        }
        else {
          Serial.println(AUTO_FEED_INITIATE_TIMEOUT + lastInteraction - millis());
        }
        break;
      case FEED:
        result = feed(true);
        Serial.println((char)result);
        break;
      case FORCE_FEED:
        result = feed(false);
        Serial.println((char)result);
        break;
      case INCREMENT_COUNTER:
        setPortions(getUsedPortions() + 1);
        Serial.println("OK");
        break;
      case RESET:
        reset();
        Serial.println("OK");
        break;
      case ENABLE_AUTO:
        setAutoFeed(true);
        autoFeedEnabled = true;
        Serial.println("OK");
        break;
      case DISABLE_AUTO:
        setAutoFeed(false);
        autoFeedEnabled = false;
        Serial.println("OK");
        break;
      case CHECKIN_AUTO:
        Serial.println("OK");
        break;
      default:
        break;
    }
    if (action != STATUS_REQUEST) {
      // status request does count as a "check-in" for postponing auto
      // feeder, as status req. is issued without user intervention
      lastInteraction = millis();
      autoFeedModeActive = false;
    }
  }
  if (autoFeedEnabled) {
    if (autoFeedModeActive) {
      if ((millis() - lastAutoFeed) > AUTO_FEED_REPEAT) {
        feed(true);
        lastAutoFeed += AUTO_FEED_REPEAT;
      }
    }
    else {
      if ((millis() - lastInteraction) > AUTO_FEED_INITIATE_TIMEOUT) {
        feed(true);
        lastAutoFeed = millis();
        autoFeedModeActive = true;
      }
    }
  }
  if (digitalRead(SWITCH_PIN) == LOW) {
    motor.run(BACKWARD);
    while(digitalRead(SWITCH_PIN) == LOW) {
      delay(10);
    }
    motor.run(RELEASE);
    lastInteraction = millis();
    autoFeedModeActive = false;
  }
}


byte feed(boolean checks) {
  
  char ret = SUCCESS;
  
  int n_portions = getUsedPortions();
  
  if (checks) {
    if (n_portions >= N_PORTIONS) {
      ret = NO_MORE_FOOD;
    }
  }
  if (ret == SUCCESS) {
    boolean first = n_portions == 0;
    ret = dispenseNextPortion(first, checks);
    if (ret == SUCCESS) {
      setPortions(n_portions + 1);
    }
  }
  return ret;
}

byte getUsedPortions() {
  byte sig1 = EEPROM.read(0);
  if (sig1 == 0xFA) {
    return EEPROM.read(1);
  }
  else {
    return 0;
  }
}

void setPortions(byte n) {
  EEPROM.write(0, 0xFA);
  EEPROM.write(1, n);
}

void reset() {
  setPortions(0);
}

byte getAutoFeed() {
  byte sig1 = EEPROM.read(2);
  if (sig1 == 0xFA) {
    return EEPROM.read(3);
  }
  else {
    return 1;
  }
}

void setAutoFeed(boolean enable) {
  EEPROM.write(2, 0xFA);
  EEPROM.write(3, (byte)enable);
}

int dispenseNextPortion(boolean first, boolean checks) {
  int high_level = 900, half_hyst = 50;
  int i;
  
  irled.run(RELEASE); // should be low already
  if (checks) {
    int base_level = analogRead(IRSENS_APIN);
    if (base_level > high_level - 2*half_hyst) {
      return SENSOR_FAILURE;
    }
  }
  
  
  irled.run(FORWARD);
  delay(1);
  // Expect a high level of light here, as the protocol is to move the basket
  // past the light sensor when finishing the feeding.
 
  if (checks) {
    int high_test = analogRead(IRSENS_APIN);
    if (high_test < high_level - 2*half_hyst) {
      return SENSOR_FAILURE;
    }
  }

  long start = millis();
  long timeout;
  if (first) {
    timeout = start + FIRST_TIMEOUT;
  }
  else {
    timeout = start + BELT_OP_TIMEOUT;
  }
  
  // case 1 (expected): HIGH -> LOW -> HIGH (stop)
  // case 2: LOW -> HIGH -> LOW -> HIGH (stop) --> Should give an error if not force
  
  //analogWrite(MOTOR_PIN, MOTOR_PWM);
  motor.run(FORWARD);
  
  const boolean states[] = {false, true, false};
  
  boolean ok = true;
  for (i=0; i<sizeof(states)/sizeof(boolean) && ok; ++i) {
    int level;
    if (states[i]) {
      // while high, require low level to continue
      level = high_level - half_hyst;
    }
    else
    {
      level = high_level + half_hyst;
    }
    ok = ok && waitPhotoState(states[i], level, timeout);
  }
  
  motor.run(RELEASE);
  irled.run(RELEASE);
  if (ok)
    return SUCCESS;
  else
    return DISPENSE_FAILURE;
}

boolean waitPhotoState(boolean whileHigh, int conditionLevel, long timeout) {
  boolean finished;
  do {
    if (millis() > timeout) 
      return false;
    delay(1);
    int val = analogRead(IRSENS_APIN);
    if (whileHigh) {
      finished = val < conditionLevel;
    }
    else {
      finished =  val > conditionLevel;
    }
  } while (!finished);
  return true;
}

