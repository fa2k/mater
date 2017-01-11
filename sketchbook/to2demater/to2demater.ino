
#include <EEPROM.h>

const int IRLED_PIN = 2, MOTOR_PIN = 3, SWITCH_PIN = 4;
const int IRSENS_APIN = A0;

const int FISH_A = 5, FISH_B = 6, FISH_C = 7, FISH_D = 8;

/* note: uses a 1-based indexing, index 0 ignored! */
const int FISH_MOTOR_HIGH[] = {
  FISH_A, FISH_B, FISH_A, FISH_C, FISH_C, FISH_D
};

const int FISH_MOTOR_LOW[] = {
  FISH_B, FISH_A, FISH_C, FISH_A, FISH_D, FISH_C
};


// Protocol
const char STATUS_REQUEST = 's', FEED = 'f', FORCE_FEED = 'F', RESET = 'r';
const char ENABLE_AUTO = 'a', DISABLE_AUTO = 'd', CHECKIN_AUTO = 'c';
const char INCREMENT_COUNTER = 'i', FISH_SCANS = 'S', PIN_CHECK = '?';
// Status codes from feed routine
const int SUCCESS = 's', NO_MORE_FOOD = 'o', DISPENSE_FAILURE = 'd', SENSOR_FAILURE = 'x';

const long COMMAND_TIMEOUT = 60000;

// How much hardware
const int N_PORTIONS = 14;

// Config
const unsigned long AUTO_FEED_INITIATE_TIMEOUT = 2ul*24*3600*1000;
const unsigned long AUTO_FEED_REPEAT = 24ul*3600*1000;

const unsigned long FISH_SCAN_COOLDOWN_MS = 1000;

// Operational consts
const int BELT_OP_TIMEOUT = 5000, FIRST_TIMEOUT = 10000;
const int MOTOR_PWM = 180; // 3/5*255 = 154 | 5/12*255=107

// Operation globals
boolean switchState;
// Auto feed
unsigned long lastInteraction, lastAutoFeed;
boolean autoFeedModeActive, autoFeedEnabled;

void setup() {
  pinMode(IRLED_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
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
    if (action > '0' && action < '9') {
      fishMotor(action - '0');
      Serial.println("OK");
    }
    else {
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
        case FISH_SCANS:
        case PIN_CHECK:
          fishApi(action);
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
  }
  else if (autoFeedEnabled) {
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
  
  if (digitalRead(SWITCH_PIN) != switchState) {
    feed(false);
    switchState = !switchState;
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
  
  digitalWrite(IRLED_PIN, LOW); // should be low already
  if (checks) {
    int base_level = analogRead(IRSENS_APIN);
    if (base_level > high_level - 2*half_hyst) {
      return SENSOR_FAILURE;
    }
  }
  
  
  digitalWrite(IRLED_PIN, HIGH);
  delay(1);
  // Expect a high level of light here, as the protocol is to move the basket
  // past the light sensor when finishing the feeding. Must also handle the 
  // case if a basket is in front of the sensor. Then assume that this is the 
  // previous basket.
 
  long start = millis();
  long timeout;
  if (first) {
    timeout = start + FIRST_TIMEOUT;
  }
  else {
    timeout = start + BELT_OP_TIMEOUT;
  }
  
  // case 1 (expected): HIGH -> LOW -> HIGH (stop)
  // case 2: LOW -> HIGH -> LOW -> HIGH (stop)
  
  //analogWrite(MOTOR_PIN, MOTOR_PWM);
  digitalWrite(MOTOR_PIN, HIGH);
  
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
  
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(IRLED_PIN, LOW);
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

void fishMotor(int imotor) {
  pinMode(FISH_MOTOR_HIGH[imotor], OUTPUT);
  pinMode(FISH_MOTOR_LOW[imotor], OUTPUT);
  
  digitalWrite(FISH_MOTOR_HIGH[imotor], HIGH);
  digitalWrite(FISH_MOTOR_LOW[imotor], LOW);
  
  delay(2000);
  
  digitalWrite(FISH_MOTOR_HIGH[imotor], LOW);
  
  pinMode(FISH_MOTOR_HIGH[imotor], INPUT);
  pinMode(FISH_MOTOR_LOW[imotor], INPUT);
}

void fishResonanceScan(int imotor) {
  double max_freq, min_freq;
  int nsteps;
  max_freq = 50;
  min_freq = 1;
  nsteps = 10;
  
  double fqstep = (max_freq - min_freq) / (nsteps - 1);
  
  for (int i=0; i<nsteps; ++i) {
    double freq = fqstep * nsteps + min_freq;
    fishRepeat(imotor, freq, 0.5, 1.0);
    delay(FISH_SCAN_COOLDOWN_MS);
  }
  
}

void fishRepeat(int imotor, double freq, double duty, double duration) {
  pinMode(FISH_MOTOR_HIGH[imotor], OUTPUT);
  pinMode(FISH_MOTOR_LOW[imotor], OUTPUT);
  
  
  digitalWrite(FISH_MOTOR_HIGH[imotor], LOW); 
  digitalWrite(FISH_MOTOR_LOW[imotor], LOW); 
  
  long period = 1000.0 / freq;
  long on_time = period * duty;
  long off_time = period - on_time;
  
  long n = duration*1000.0 / period;
  
  for (int i=0; i<n; ++i) {
    digitalWrite(FISH_MOTOR_HIGH[imotor], HIGH);
    delay(on_time);
    digitalWrite(FISH_MOTOR_HIGH[imotor], LOW); 
    delay(off_time);
  }
    
  delay(10); // swallow flyback
  
  // go to safe input "off" state
  pinMode(FISH_MOTOR_HIGH[imotor], INPUT);
  pinMode(FISH_MOTOR_LOW[imotor], INPUT);
}

// parse scan request
void fishApi(int action) {
  long start = millis();
  boolean avail = false;
  while (millis() < start+COMMAND_TIMEOUT && !avail) {
    avail = Serial.available() > 0;
  }
  if (avail) {
    int im = Serial.read() - '0';
    if (im > 0 && im < 9) {
      if (action == FISH_SCANS) {
        fishResonanceScan(im);
      }
      else if (action == PIN_CHECK) {
        checkConnection(im);
      }
    }
  }
}


void checkConnection(int im) {

    digitalWrite(FISH_MOTOR_HIGH[im], LOW);
    digitalWrite(FISH_MOTOR_LOW[im], LOW);  
    pinMode(FISH_MOTOR_LOW[im], OUTPUT);
    pinMode(FISH_MOTOR_HIGH[im], OUTPUT);
    
    pinMode(FISH_MOTOR_LOW[im], INPUT);
    digitalWrite(FISH_MOTOR_HIGH[im], HIGH);
    
    Serial.print("+:");
    Serial.print(digitalRead(FISH_MOTOR_LOW[im]));
    Serial.print(" ");

    pinMode(FISH_MOTOR_LOW[im], OUTPUT);
    digitalWrite(FISH_MOTOR_HIGH[im], HIGH);
    digitalWrite(FISH_MOTOR_LOW[im], HIGH);
    pinMode(FISH_MOTOR_HIGH[im], INPUT);
    digitalWrite(FISH_MOTOR_LOW[im], LOW);
    
    Serial.print("-:");
    Serial.print(!digitalRead(FISH_MOTOR_HIGH[im]));
    Serial.println(" ");
    
 
    pinMode(FISH_MOTOR_LOW[im], INPUT);
    pinMode(FISH_MOTOR_HIGH[im], INPUT);   
}
