/*
  Gen 2 Prius parking pawl / shift control actuator vehicle controller draft.
  Tailored for a swap such as a Prius transaxle in a 1996 Civic using the
  Civic shifter/interlock as driver intent.

  This is a more conservative hardwired-input state machine for a real project.
  It is still a prototype: validate it on stands, instrument current and motion,
  and do not rely on it as the only device preventing vehicle movement.

  The actuator motor must be driven through external automotive-rated power
  electronics. Arduino pins must never connect directly to motor phase wiring.
*/

// -------------------------- Actuator / driver pins --------------------------
const uint8_t PIN_PHASE_U = 9;   // S1-7 MUA -> low-side U gate-driver input
const uint8_t PIN_PHASE_V = 10;  // S1-6 MVA -> low-side V gate-driver input
const uint8_t PIN_PHASE_W = 11;  // S1-1 MWA -> low-side W gate-driver input

const uint8_t PIN_HALL_RA = 2;   // S1-5 RA
const uint8_t PIN_HALL_RB = 3;   // S1-3 RB
const uint8_t PIN_HALL_RZ1 = 4;  // S1-4 RZ1

const uint8_t PIN_LOCK_LIMIT = 5;    // independent PARK confirmation, active low
const uint8_t PIN_UNLOCK_LIMIT = 6;  // independent NOT-P confirmation, active low
const uint8_t PIN_ARM_ENABLE = 7;    // ignition/controller-enable, active low
const uint8_t PIN_BRAKE = 8;         // brake switch or brake-ok relay, active low

const uint8_t PIN_MOTOR_POWER_ENABLE = 12;  // relay/smart high-side enable
const uint8_t PIN_STATUS_LED = 13;

const uint8_t PIN_CURRENT_SENSOR = A0;
const uint8_t PIN_SPEED_ZERO_1 = A1;    // external zero-speed relay/comparator, active low
const uint8_t PIN_SPEED_ZERO_2 = A2;    // redundant zero-speed relay/comparator, active low
const uint8_t PIN_CIVIC_PARK_SELECTED = A3;    // Civic shifter says P selected
const uint8_t PIN_CIVIC_NOT_P_SELECTED = A4;   // Civic shifter says not-P selected
const uint8_t PIN_EBRAKE = A5;                 // parking brake switch, active low

// -------------------------- Configuration --------------------------
// Publication builds cannot energize the actuator until its interface is reviewed.
const bool ENABLE_ACTUATOR_OUTPUTS = false;
const uint8_t ACTIVE_LOW = LOW;
const uint8_t MOTOR_POWER_ON_LEVEL = HIGH;
const uint8_t MOTOR_POWER_OFF_LEVEL = LOW;

const bool REQUIRE_TWO_SPEED_ZERO_INPUTS = true;
const bool REQUIRE_BRAKE_FOR_PARK = true;
const bool REQUIRE_BRAKE_FOR_UNPARK = true;
const bool REQUIRE_LIMIT_SWITCHES = true;
const bool REQUIRE_EBRAKE_FOR_PARK = false;
const bool REQUIRE_EBRAKE_FOR_UNPARK = false;

// Direction is empirical. If PARK moves the mechanism away from park, swap signs.
const int8_t PARK_DIRECTION = +1;
const int8_t UNPARK_DIRECTION = -1;

// Start gentle. Raise only after measured bench/car-on-stands validation.
const uint8_t RUN_PWM = 75;
const uint16_t STEP_DWELL_MS = 10;
const uint16_t PHASE_DEADTIME_US = 700;
const uint32_t MAX_MOVE_MS = 2500;
const uint32_t NO_HALL_PROGRESS_MS = 600;
const uint32_t OVERCURRENT_TRIP_MS = 20;
const uint16_t MAX_HALL_EDGES_PER_MOVE = 800;  // sanity stop, not position authority

// Current sensor calibration. Defaults fit a bidirectional 5 V Hall sensor
// with output centered near 2.5 V. Update CURRENT_MV_PER_AMP for your module.
const float ADC_REF_MV = 5000.0;
const float ADC_COUNTS = 1023.0;
const float CURRENT_MV_PER_AMP = 40.0;
const float CURRENT_TRIP_AMPS = 12.0;
float currentZeroMv = 2500.0;

// -------------------------- State --------------------------
enum PawlState {
  STATE_UNKNOWN,
  STATE_PARKED,
  STATE_UNPARKED,
  STATE_MOVING_TO_PARK,
  STATE_MOVING_TO_UNPARK,
  STATE_FAULT
};

PawlState state = STATE_UNKNOWN;
const uint8_t PHASE_PINS[3] = {PIN_PHASE_U, PIN_PHASE_V, PIN_PHASE_W};
uint8_t phaseIndex = 0;
uint8_t lastHall = 0;
uint32_t hallEdges = 0;
uint32_t lastHallChangeMs = 0;
char faultReason[80] = "none";

void inferStaticState();

bool activeLow(uint8_t pin) {
  return digitalRead(pin) == ACTIVE_LOW;
}

bool armed() {
  return activeLow(PIN_ARM_ENABLE);
}

bool brakeOk() {
  return activeLow(PIN_BRAKE);
}

bool ebrakeSet() {
  return activeLow(PIN_EBRAKE);
}

bool speedZeroOk() {
  bool s1 = activeLow(PIN_SPEED_ZERO_1);
  bool s2 = activeLow(PIN_SPEED_ZERO_2);
  return REQUIRE_TWO_SPEED_ZERO_INPUTS ? (s1 && s2) : (s1 || s2);
}

bool lockLimit() {
  return activeLow(PIN_LOCK_LIMIT);
}

bool unlockLimit() {
  return activeLow(PIN_UNLOCK_LIMIT);
}

bool civicParkSelected() {
  return activeLow(PIN_CIVIC_PARK_SELECTED);
}

bool civicNotParkSelected() {
  return activeLow(PIN_CIVIC_NOT_P_SELECTED);
}

bool civicShifterValid() {
  return civicParkSelected() != civicNotParkSelected();
}

bool parkRequested() {
  return civicParkSelected();
}

bool unparkRequested() {
  return civicNotParkSelected();
}

uint8_t readHallState() {
  uint8_t h = 0;
  if (digitalRead(PIN_HALL_RA)) h |= 0x01;
  if (digitalRead(PIN_HALL_RB)) h |= 0x02;
  if (digitalRead(PIN_HALL_RZ1)) h |= 0x04;
  return h;
}

void updateHallEdges() {
  uint8_t h = readHallState();
  if (h != lastHall) {
    lastHall = h;
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
  float mv = ((float)sum / samples) * ADC_REF_MV / ADC_COUNTS;
  float amps = (mv - currentZeroMv) / CURRENT_MV_PER_AMP;
  return amps < 0 ? -amps : amps;
}

void allPhasesOff() {
  analogWrite(PIN_PHASE_U, 0);
  analogWrite(PIN_PHASE_V, 0);
  analogWrite(PIN_PHASE_W, 0);
}

void motorPower(bool on) {
  if (!ENABLE_ACTUATOR_OUTPUTS) on = false;  // Keep the common feed disabled.
  digitalWrite(PIN_MOTOR_POWER_ENABLE, on ? MOTOR_POWER_ON_LEVEL : MOTOR_POWER_OFF_LEVEL);
}

void setState(PawlState next) {
  state = next;
  digitalWrite(PIN_STATUS_LED, state == STATE_FAULT ? HIGH : LOW);
}

void fault(const char *reason) {
  allPhasesOff();
  motorPower(false);
  strncpy(faultReason, reason, sizeof(faultReason) - 1);
  faultReason[sizeof(faultReason) - 1] = '\0';
  setState(STATE_FAULT);
  Serial.print(F("FAULT: "));
  Serial.println(faultReason);
}

void clearFaultIfSafe() {
  if (state != STATE_FAULT) return;
  if (!armed()) {
    Serial.println(F("Fault reset refused: ARM is off."));
    return;
  }
  strncpy(faultReason, "none", sizeof(faultReason));
  inferStaticState();
  Serial.println(F("Fault cleared."));
}

void energizePhase(uint8_t idx) {
  allPhasesOff();
  if (!ENABLE_ACTUATOR_OUTPUTS) return;  // No phase PWM in the review build.
  delayMicroseconds(PHASE_DEADTIME_US);
  analogWrite(PHASE_PINS[idx], RUN_PWM);
}

void nextPhase(int8_t direction) {
  if (direction >= 0) {
    phaseIndex = (phaseIndex + 1) % 3;
  } else {
    phaseIndex = (phaseIndex + 2) % 3;
  }
  energizePhase(phaseIndex);
}

void zeroCurrentSensor() {
  allPhasesOff();
  motorPower(false);
  delay(50);

  uint32_t sum = 0;
  const uint16_t samples = 100;
  for (uint16_t i = 0; i < samples; i++) {
    sum += analogRead(PIN_CURRENT_SENSOR);
    delay(2);
  }
  currentZeroMv = ((float)sum / samples) * ADC_REF_MV / ADC_COUNTS;
}

void inferStaticState() {
  if (lockLimit() && unlockLimit()) {
    fault("both position limits active");
    return;
  }

  if (lockLimit()) {
    setState(STATE_PARKED);
  } else if (unlockLimit()) {
    setState(STATE_UNPARKED);
  } else {
    setState(STATE_UNKNOWN);
  }
}

bool commonMovePrecheck(bool goingToPark) {
  if (state == STATE_FAULT) return false;
  if (!armed()) {
    Serial.println(F("Move refused: ARM disabled."));
    return false;
  }
  if (!speedZeroOk()) {
    Serial.println(F("Move refused: zero-speed inputs not both OK."));
    return false;
  }
  if (!civicShifterValid()) {
    fault("invalid Civic shifter inputs");
    return false;
  }
  if (goingToPark && REQUIRE_BRAKE_FOR_PARK && !brakeOk()) {
    Serial.println(F("Park refused: brake not active."));
    return false;
  }
  if (!goingToPark && REQUIRE_BRAKE_FOR_UNPARK && !brakeOk()) {
    Serial.println(F("Unpark refused: brake not active."));
    return false;
  }
  if (goingToPark && REQUIRE_EBRAKE_FOR_PARK && !ebrakeSet()) {
    Serial.println(F("Park refused: ebrake not set."));
    return false;
  }
  if (!goingToPark && REQUIRE_EBRAKE_FOR_UNPARK && !ebrakeSet()) {
    Serial.println(F("Unpark refused: ebrake not set."));
    return false;
  }
  if (parkRequested() && unparkRequested()) {
    fault("conflicting park/unpark requests");
    return false;
  }
  if (REQUIRE_LIMIT_SWITCHES && lockLimit() && unlockLimit()) {
    fault("both position limits active");
    return false;
  }
  return true;
}

bool moveSafetyService(uint32_t moveStartMs, uint32_t edgeStart, uint32_t *overCurrentStart) {
  updateHallEdges();

  if (!armed()) {
    fault("ARM disabled during move");
    return false;
  }
  if (!speedZeroOk()) {
    fault("speed-zero lost during move");
    return false;
  }
  if (!brakeOk() && (REQUIRE_BRAKE_FOR_PARK || REQUIRE_BRAKE_FOR_UNPARK)) {
    fault("brake lost during move");
    return false;
  }
  if (!civicShifterValid()) {
    fault("Civic shifter invalid during move");
    return false;
  }
  if ((state == STATE_MOVING_TO_PARK) && REQUIRE_EBRAKE_FOR_PARK && !ebrakeSet()) {
    fault("ebrake lost during park move");
    return false;
  }
  if ((state == STATE_MOVING_TO_UNPARK) && REQUIRE_EBRAKE_FOR_UNPARK && !ebrakeSet()) {
    fault("ebrake lost during unpark move");
    return false;
  }

  float amps = readCurrentAmps();
  if (amps > CURRENT_TRIP_AMPS) {
    if (*overCurrentStart == 0) *overCurrentStart = millis();
    if (millis() - *overCurrentStart >= OVERCURRENT_TRIP_MS) {
      fault("overcurrent");
      return false;
    }
  } else {
    *overCurrentStart = 0;
  }

  if (millis() - moveStartMs > MAX_MOVE_MS) {
    fault("move timeout");
    return false;
  }
  if (millis() - lastHallChangeMs > NO_HALL_PROGRESS_MS) {
    fault("no Hall progress");
    return false;
  }
  if (hallEdges - edgeStart > MAX_HALL_EDGES_PER_MOVE) {
    fault("Hall edge sanity limit");
    return false;
  }
  return true;
}

bool dwellWithSafety(uint32_t moveStartMs, uint32_t edgeStart, uint32_t *overCurrentStart) {
  uint32_t dwellStart = millis();
  while (millis() - dwellStart < STEP_DWELL_MS) {
    if (!moveSafetyService(moveStartMs, edgeStart, overCurrentStart)) return false;
    delay(1);
  }
  return true;
}

void moveTo(bool goingToPark) {
  if (!commonMovePrecheck(goingToPark)) return;

  if (goingToPark && lockLimit()) {
    setState(STATE_PARKED);
    return;
  }
  if (!goingToPark && unlockLimit()) {
    setState(STATE_UNPARKED);
    return;
  }

  setState(goingToPark ? STATE_MOVING_TO_PARK : STATE_MOVING_TO_UNPARK);
  Serial.println(goingToPark ? F("Moving to PARK") : F("Moving to NOT-P"));

  uint32_t moveStart = millis();
  uint32_t edgeStart = hallEdges;
  uint32_t overCurrentStart = 0;
  lastHallChangeMs = millis();

  motorPower(true);
  delay(25);

  while (state == STATE_MOVING_TO_PARK || state == STATE_MOVING_TO_UNPARK) {
    if (goingToPark && lockLimit()) {
      allPhasesOff();
      motorPower(false);
      setState(STATE_PARKED);
      Serial.println(F("PARK confirmed."));
      return;
    }
    if (!goingToPark && unlockLimit()) {
      allPhasesOff();
      motorPower(false);
      setState(STATE_UNPARKED);
      Serial.println(F("NOT-P confirmed."));
      return;
    }

    nextPhase(goingToPark ? PARK_DIRECTION : UNPARK_DIRECTION);
    if (!dwellWithSafety(moveStart, edgeStart, &overCurrentStart)) return;
  }
}

const char *stateName() {
  switch (state) {
    case STATE_UNKNOWN: return "UNKNOWN";
    case STATE_PARKED: return "PARKED";
    case STATE_UNPARKED: return "UNPARKED";
    case STATE_MOVING_TO_PARK: return "MOVING_TO_PARK";
    case STATE_MOVING_TO_UNPARK: return "MOVING_TO_UNPARK";
    case STATE_FAULT: return "FAULT";
  }
  return "?";
}

void printStatus() {
  updateHallEdges();
  Serial.print(F("state="));
  Serial.print(stateName());
  Serial.print(F(" fault="));
  Serial.print(faultReason);
  Serial.print(F(" arm="));
  Serial.print(armed());
  Serial.print(F(" brake="));
  Serial.print(brakeOk());
  Serial.print(F(" ebrake="));
  Serial.print(ebrakeSet());
  Serial.print(F(" speedZero="));
  Serial.print(speedZeroOk());
  Serial.print(F(" civicP="));
  Serial.print(civicParkSelected());
  Serial.print(F(" civicNotP="));
  Serial.print(civicNotParkSelected());
  Serial.print(F(" lock="));
  Serial.print(lockLimit());
  Serial.print(F(" unlock="));
  Serial.print(unlockLimit());
  Serial.print(F(" hall="));
  Serial.print(readHallState(), BIN);
  Serial.print(F(" edges="));
  Serial.print(hallEdges);
  Serial.print(F(" current="));
  Serial.println(readCurrentAmps(), 1);
}

void setup() {
  pinMode(PIN_PHASE_U, OUTPUT);
  pinMode(PIN_PHASE_V, OUTPUT);
  pinMode(PIN_PHASE_W, OUTPUT);
  pinMode(PIN_MOTOR_POWER_ENABLE, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);

  pinMode(PIN_HALL_RA, INPUT_PULLUP);
  pinMode(PIN_HALL_RB, INPUT_PULLUP);
  pinMode(PIN_HALL_RZ1, INPUT_PULLUP);
  pinMode(PIN_LOCK_LIMIT, INPUT_PULLUP);
  pinMode(PIN_UNLOCK_LIMIT, INPUT_PULLUP);
  pinMode(PIN_ARM_ENABLE, INPUT_PULLUP);
  pinMode(PIN_BRAKE, INPUT_PULLUP);
  pinMode(PIN_SPEED_ZERO_1, INPUT_PULLUP);
  pinMode(PIN_SPEED_ZERO_2, INPUT_PULLUP);
  pinMode(PIN_CIVIC_PARK_SELECTED, INPUT_PULLUP);
  pinMode(PIN_CIVIC_NOT_P_SELECTED, INPUT_PULLUP);
  pinMode(PIN_EBRAKE, INPUT_PULLUP);

  allPhasesOff();
  motorPower(false);

  Serial.begin(115200);

  lastHall = readHallState();
  lastHallChangeMs = millis();
  zeroCurrentSensor();
  inferStaticState();
  printStatus();
}

void loop() {
  updateHallEdges();

  static uint32_t lastStatusMs = 0;
  if (millis() - lastStatusMs > 1000) {
    lastStatusMs = millis();
    printStatus();
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      clearFaultIfSafe();
    }
  }

  if (state == STATE_FAULT) {
    delay(10);
    return;
  }

  if (lockLimit() && unlockLimit()) {
    fault("both position limits active");
    return;
  }

  if (!armed()) {
    allPhasesOff();
    motorPower(false);
    inferStaticState();
    delay(20);
    return;
  }

  if (!civicShifterValid()) {
    fault("invalid Civic shifter inputs");
    return;
  }

  if (parkRequested()) {
    moveTo(true);
  } else if (unparkRequested()) {
    moveTo(false);
  }

  delay(10);
}
