/*
  Gen 2 Prius parking pawl / shift control actuator bench controller.

  This sketch is for bench characterization of the 2004-2009 Prius S1
  shift control actuator. It drives the three parking-lock motor phase
  leads with external low-side MOSFETs and reads the three Hall angle
  sensor outputs.

  Do not connect Arduino pins directly to the actuator motor phases.
  Use fused 12 V power, current sensing, flyback clamps, and an emergency
  stop. Do not use this as a road-going vehicle safety controller without
  independent validation and proper vehicle-speed/brake interlocks.
*/

// -------------------------- Pin assignments --------------------------
// Motor phase driver inputs. These go to MOSFET/gate-driver inputs.
// Actuator connector: S1-7 MUA = U, S1-6 MVA = V, S1-1 MWA = W.
const uint8_t PIN_PHASE_U = 9;   // PWM
const uint8_t PIN_PHASE_V = 10;  // PWM
const uint8_t PIN_PHASE_W = 11;  // PWM

// Hall / rotation-angle sensor outputs. Use external 4.7k-10k pullups to 5 V
// if the sensor outputs do not already pull high cleanly.
// Actuator connector: S1-5 RA, S1-3 RB, S1-4 RZ1.
const uint8_t PIN_HALL_RA = 2;
const uint8_t PIN_HALL_RB = 3;
const uint8_t PIN_HALL_RZ1 = 4;

// Optional position/interlock inputs. Wired active-low to GND by default.
const uint8_t PIN_LOCK_LIMIT = 5;
const uint8_t PIN_UNLOCK_LIMIT = 6;
const uint8_t PIN_ARM_SWITCH = 7;
const uint8_t PIN_BRAKE_OR_SAFE = 8;

// Optional relay/high-side enable for fused 12 V feed to S1-2 MGNA.
// Drive a relay transistor or high-side switch enable, not a relay coil directly.
const uint8_t PIN_MOTOR_POWER_ENABLE = 12;

// Total motor current sensor output, e.g. ACS758/ACS770/ACS724 module.
const uint8_t PIN_CURRENT_SENSOR = A0;
const uint8_t PIN_STATUS_LED = 13;

// -------------------------- Safety configuration --------------------------
// Publication builds cannot energize the actuator until its interface is reviewed.
const bool ENABLE_ACTUATOR_OUTPUTS = false;
const bool REQUIRE_ARM_SWITCH = true;
const bool REQUIRE_BRAKE_FOR_UNLOCK = true;
const bool USE_LOCK_LIMIT_SWITCH = true;
const bool USE_UNLOCK_LIMIT_SWITCH = true;
const bool USE_INTERNAL_PULLUPS = true;

const uint8_t ACTIVE_LOW = LOW;
const uint8_t MOTOR_POWER_ON_LEVEL = HIGH;
const uint8_t MOTOR_POWER_OFF_LEVEL = LOW;

// Start gently. Increase only after verifying current, temperature, and motion.
uint8_t runPwm = 70;  // 0-255, roughly 27% duty on Arduino Uno/Nano.

const uint16_t STEP_DWELL_MS = 12;
const uint16_t PHASE_DEADTIME_US = 600;
const uint16_t JOG_STEPS = 1;
const uint32_t MAX_RUN_MS = 2500;
const uint32_t NO_HALL_PROGRESS_MS = 800;
const uint32_t OVERCURRENT_TRIP_MS = 20;

// If you do not have limit switches, set the relevant USE_*_LIMIT_SWITCH false
// and set these targets from measured Hall edge counts. Zero means "no target".
const uint16_t PARK_EDGE_TARGET = 0;
const uint16_t UNLOCK_EDGE_TARGET = 0;

// Direction is empirical. If PARK moves the shaft the wrong way, swap these.
const int8_t PARK_DIRECTION = +1;
const int8_t UNLOCK_DIRECTION = -1;

// Current sensor calibration. Defaults match a bidirectional ACS758-050B-ish
// sensor at 5 V (about 40 mV/A, zero near 2.5 V). Use the 'z' command to zero.
const float ADC_REF_MV = 5000.0;
const float ADC_COUNTS = 1023.0;
const float CURRENT_MV_PER_AMP = 40.0;
float currentZeroMv = 2500.0;
float currentTripAmps = 12.0;

// -------------------------- Runtime state --------------------------
const uint8_t PHASE_PINS[3] = {PIN_PHASE_U, PIN_PHASE_V, PIN_PHASE_W};
const char PHASE_NAMES[3] = {'U', 'V', 'W'};

uint8_t phaseIndex = 0;
uint8_t lastHall = 0;
uint32_t hallEdges = 0;
uint32_t lastHallChangeMs = 0;
bool faulted = false;
char faultReason[80] = "none";

void allPhasesOff() {
  analogWrite(PIN_PHASE_U, 0);
  analogWrite(PIN_PHASE_V, 0);
  analogWrite(PIN_PHASE_W, 0);
}

void setMotorPower(bool on) {
  if (!ENABLE_ACTUATOR_OUTPUTS) on = false;  // Keep the common feed disabled.
  digitalWrite(PIN_MOTOR_POWER_ENABLE, on ? MOTOR_POWER_ON_LEVEL : MOTOR_POWER_OFF_LEVEL);
  digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
}

bool activeLowInput(uint8_t pin) {
  return digitalRead(pin) == ACTIVE_LOW;
}

bool armIsActive() {
  return !REQUIRE_ARM_SWITCH || activeLowInput(PIN_ARM_SWITCH);
}

bool brakeOrSafeIsActive() {
  return activeLowInput(PIN_BRAKE_OR_SAFE);
}

bool lockLimitActive() {
  return USE_LOCK_LIMIT_SWITCH && activeLowInput(PIN_LOCK_LIMIT);
}

bool unlockLimitActive() {
  return USE_UNLOCK_LIMIT_SWITCH && activeLowInput(PIN_UNLOCK_LIMIT);
}

uint8_t readHallState() {
  uint8_t state = 0;
  if (digitalRead(PIN_HALL_RA)) state |= 0x01;
  if (digitalRead(PIN_HALL_RB)) state |= 0x02;
  if (digitalRead(PIN_HALL_RZ1)) state |= 0x04;
  return state;
}

void updateHallEdges() {
  uint8_t nowHall = readHallState();
  if (nowHall != lastHall) {
    lastHall = nowHall;
    hallEdges++;
    lastHallChangeMs = millis();
  }
}

float readCurrentAmps() {
  uint32_t sum = 0;
  const uint8_t samples = 8;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(PIN_CURRENT_SENSOR);
  }
  float adc = (float)sum / samples;
  float mv = adc * ADC_REF_MV / ADC_COUNTS;
  float amps = (mv - currentZeroMv) / CURRENT_MV_PER_AMP;
  if (amps < 0) amps = -amps;
  return amps;
}

void setFault(const char *reason) {
  allPhasesOff();
  setMotorPower(false);
  faulted = true;
  strncpy(faultReason, reason, sizeof(faultReason) - 1);
  faultReason[sizeof(faultReason) - 1] = '\0';
  Serial.print(F("FAULT: "));
  Serial.println(faultReason);
}

void clearFault() {
  faulted = false;
  strncpy(faultReason, "none", sizeof(faultReason));
  Serial.println(F("Fault cleared."));
}

void energizePhase(uint8_t idx, uint8_t pwm) {
  allPhasesOff();
  if (!ENABLE_ACTUATOR_OUTPUTS) return;  // No phase PWM in the review build.
  delayMicroseconds(PHASE_DEADTIME_US);
  analogWrite(PHASE_PINS[idx], pwm);
}

bool serviceDuringDwell(uint32_t startMs, uint32_t edgeStart, uint32_t *overCurrentStart) {
  (void)edgeStart;  // Travel targets are checked by the calling motion routine.
  updateHallEdges();

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'x' || c == 'X') {
      setFault("serial stop");
      return false;
    }
  }

  if (!armIsActive()) {
    setFault("arm switch open");
    return false;
  }

  float amps = readCurrentAmps();
  if (amps > currentTripAmps) {
    if (*overCurrentStart == 0) *overCurrentStart = millis();
    if (millis() - *overCurrentStart >= OVERCURRENT_TRIP_MS) {
      setFault("overcurrent");
      return false;
    }
  } else {
    *overCurrentStart = 0;
  }

  if ((millis() - startMs) > MAX_RUN_MS) {
    setFault("max runtime");
    return false;
  }

  if (((millis() - startMs) > NO_HALL_PROGRESS_MS) &&
      ((millis() - lastHallChangeMs) > NO_HALL_PROGRESS_MS)) {
    setFault("no Hall progress");
    return false;
  }

  return true;
}

bool dwellWithSafety(uint16_t dwellMs, uint32_t runStartMs, uint32_t edgeStart, uint32_t *overCurrentStart) {
  uint32_t dwellStart = millis();
  while (millis() - dwellStart < dwellMs) {
    if (!serviceDuringDwell(runStartMs, edgeStart, overCurrentStart)) {
      return false;
    }
    delay(1);
  }
  return true;
}

bool nextPhaseAndDwell(int8_t direction, uint32_t runStartMs, uint32_t edgeStart, uint32_t *overCurrentStart) {
  if (direction >= 0) {
    phaseIndex = (phaseIndex + 1) % 3;
  } else {
    phaseIndex = (phaseIndex + 2) % 3;  // equivalent to -1 modulo 3
  }

  Serial.print(F("phase "));
  Serial.print(PHASE_NAMES[phaseIndex]);
  Serial.print(F(" hall="));
  Serial.print(readHallState(), BIN);
  Serial.print(F(" edges="));
  Serial.print(hallEdges - edgeStart);
  Serial.print(F(" current="));
  Serial.println(readCurrentAmps(), 1);

  energizePhase(phaseIndex, runPwm);
  return dwellWithSafety(STEP_DWELL_MS, runStartMs, edgeStart, overCurrentStart);
}

bool motionPrecheck(bool unlocking) {
  if (faulted) {
    Serial.print(F("Refusing to run while faulted: "));
    Serial.println(faultReason);
    return false;
  }

  if (!armIsActive()) {
    Serial.println(F("Arm switch is not active. Hold/close ARM to GND."));
    return false;
  }

  if (unlocking && REQUIRE_BRAKE_FOR_UNLOCK && !brakeOrSafeIsActive()) {
    Serial.println(F("Unlock requires BRAKE/SAFE input active."));
    return false;
  }

  return true;
}

void jog(int8_t direction, uint16_t steps) {
  if (!motionPrecheck(false)) return;

  Serial.println(F("Jog start. Send x to stop."));
  setMotorPower(true);
  delay(20);

  uint32_t runStart = millis();
  uint32_t edgeStart = hallEdges;
  uint32_t overCurrentStart = 0;
  lastHallChangeMs = millis();

  for (uint16_t i = 0; i < steps; i++) {
    if (!nextPhaseAndDwell(direction, runStart, edgeStart, &overCurrentStart)) break;
  }

  allPhasesOff();
  setMotorPower(false);
  Serial.println(F("Jog done."));
}

void runMotion(const char *name, int8_t direction, bool unlocking, bool useLimit, uint16_t edgeTarget) {
  if (!motionPrecheck(unlocking)) return;
  if (!useLimit && edgeTarget == 0) {
    Serial.println(F("No limit switch or edge target configured for this move."));
    return;
  }

  if ((direction == PARK_DIRECTION) && lockLimitActive()) {
    Serial.println(F("Already at lock limit."));
    return;
  }
  if ((direction == UNLOCK_DIRECTION) && unlockLimitActive()) {
    Serial.println(F("Already at unlock limit."));
    return;
  }

  Serial.print(F("Run "));
  Serial.print(name);
  Serial.println(F(". Send x to stop."));

  setMotorPower(true);
  delay(20);

  uint32_t runStart = millis();
  uint32_t edgeStart = hallEdges;
  uint32_t overCurrentStart = 0;
  lastHallChangeMs = millis();

  while (!faulted) {
    if (useLimit) {
      if ((direction == PARK_DIRECTION) && lockLimitActive()) {
        Serial.println(F("Lock limit reached."));
        break;
      }
      if ((direction == UNLOCK_DIRECTION) && unlockLimitActive()) {
        Serial.println(F("Unlock limit reached."));
        break;
      }
    }

    if (edgeTarget > 0 && (hallEdges - edgeStart) >= edgeTarget) {
      Serial.println(F("Hall edge target reached."));
      break;
    }

    if (!nextPhaseAndDwell(direction, runStart, edgeStart, &overCurrentStart)) {
      break;
    }
  }

  allPhasesOff();
  setMotorPower(false);
  Serial.println(F("Run stopped."));
}

void zeroCurrentSensor() {
  allPhasesOff();
  setMotorPower(false);
  delay(20);

  uint32_t sum = 0;
  const uint16_t samples = 80;
  for (uint16_t i = 0; i < samples; i++) {
    sum += analogRead(PIN_CURRENT_SENSOR);
    delay(2);
  }
  float adc = (float)sum / samples;
  currentZeroMv = adc * ADC_REF_MV / ADC_COUNTS;

  Serial.print(F("Current zero set to "));
  Serial.print(currentZeroMv, 1);
  Serial.println(F(" mV."));
}

void printStatus() {
  updateHallEdges();
  Serial.println(F("--- status ---"));
  Serial.print(F("fault="));
  Serial.print(faulted ? F("YES ") : F("no "));
  Serial.println(faultReason);
  Serial.print(F("armed="));
  Serial.print(armIsActive());
  Serial.print(F(" brake/safe="));
  Serial.print(brakeOrSafeIsActive());
  Serial.print(F(" lockLimit="));
  Serial.print(lockLimitActive());
  Serial.print(F(" unlockLimit="));
  Serial.println(unlockLimitActive());
  Serial.print(F("hall RA/RB/RZ1 bits="));
  Serial.print(readHallState(), BIN);
  Serial.print(F(" totalEdges="));
  Serial.println(hallEdges);
  Serial.print(F("current="));
  Serial.print(readCurrentAmps(), 2);
  Serial.print(F(" A trip="));
  Serial.print(currentTripAmps, 1);
  Serial.print(F(" A pwm="));
  Serial.println(runPwm);
}

void printHelp() {
  Serial.println();
  Serial.println(F("Gen 2 Prius park actuator bench controller"));
  Serial.println(F("Commands:"));
  Serial.println(F("  s  status"));
  Serial.println(F("  +  jog one commutation step positive"));
  Serial.println(F("  -  jog one commutation step negative"));
  Serial.println(F("  P  run toward PARK/lock"));
  Serial.println(F("  N  run toward NOT-P/unlock"));
  Serial.println(F("  u  PWM up"));
  Serial.println(F("  d  PWM down"));
  Serial.println(F("  z  zero current sensor with motor off"));
  Serial.println(F("  c  clear fault"));
  Serial.println(F("  x  stop active motion / trip fault"));
  Serial.println(F("  h  help"));
  Serial.println();
  Serial.println(F("Default inputs are active-low. ARM must be grounded to run."));
}

void setup() {
  pinMode(PIN_PHASE_U, OUTPUT);
  pinMode(PIN_PHASE_V, OUTPUT);
  pinMode(PIN_PHASE_W, OUTPUT);
  pinMode(PIN_MOTOR_POWER_ENABLE, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);

  if (USE_INTERNAL_PULLUPS) {
    pinMode(PIN_HALL_RA, INPUT_PULLUP);
    pinMode(PIN_HALL_RB, INPUT_PULLUP);
    pinMode(PIN_HALL_RZ1, INPUT_PULLUP);
    pinMode(PIN_LOCK_LIMIT, INPUT_PULLUP);
    pinMode(PIN_UNLOCK_LIMIT, INPUT_PULLUP);
    pinMode(PIN_ARM_SWITCH, INPUT_PULLUP);
    pinMode(PIN_BRAKE_OR_SAFE, INPUT_PULLUP);
  } else {
    pinMode(PIN_HALL_RA, INPUT);
    pinMode(PIN_HALL_RB, INPUT);
    pinMode(PIN_HALL_RZ1, INPUT);
    pinMode(PIN_LOCK_LIMIT, INPUT);
    pinMode(PIN_UNLOCK_LIMIT, INPUT);
    pinMode(PIN_ARM_SWITCH, INPUT);
    pinMode(PIN_BRAKE_OR_SAFE, INPUT);
  }

  allPhasesOff();
  setMotorPower(false);

  Serial.begin(115200);
  while (!Serial) {
    ;  // Leonardo/Micro only; harmless on Uno/Nano.
  }

  lastHall = readHallState();
  lastHallChangeMs = millis();
  printHelp();
  printStatus();
}

void loop() {
  updateHallEdges();

  if (!Serial.available()) {
    delay(5);
    return;
  }

  char c = Serial.read();
  switch (c) {
    case 'h':
    case 'H':
      printHelp();
      break;
    case 's':
    case 'S':
      printStatus();
      break;
    case '+':
      jog(+1, JOG_STEPS);
      break;
    case '-':
      jog(-1, JOG_STEPS);
      break;
    case 'P':
      runMotion("PARK", PARK_DIRECTION, false, USE_LOCK_LIMIT_SWITCH, PARK_EDGE_TARGET);
      break;
    case 'N':
      runMotion("NOT-P", UNLOCK_DIRECTION, true, USE_UNLOCK_LIMIT_SWITCH, UNLOCK_EDGE_TARGET);
      break;
    case 'u':
    case 'U':
      if (runPwm < 245) runPwm += 10;
      printStatus();
      break;
    case 'd':
    case 'D':
      if (runPwm > 20) runPwm -= 10;
      printStatus();
      break;
    case 'z':
    case 'Z':
      zeroCurrentSensor();
      break;
    case 'c':
    case 'C':
      clearFault();
      break;
    case 'x':
    case 'X':
      setFault("manual stop");
      break;
    case '\r':
    case '\n':
      break;
    default:
      Serial.println(F("Unknown command. h for help."));
      break;
  }
}
