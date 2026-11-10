/*
  1996 Civic automatic-transmission signal emulator.

  Purpose:
    Keep an automatic-transmission Civic PCM/gauge harness happy after the
    original transmission is removed, using MPH received from the EV/motor CAN
    bus as the speed source.

  Target wiring model:
    1996 Civic non-HX automatic (S4PA/S4XA-style 4 A/T). The sketch emulates
    the transmission-side signals that normally go into the PCM/gauge harness:

      - VSS pulse train
      - mainshaft speed sensor pulse train
      - countershaft speed sensor pulse train
      - transmission range switch closures: P/N, R, D4, D3, 2, optional 1/P

  Important hardware boundary:
    Shift solenoid, lock-up solenoid, and linear solenoid wires are PCM outputs,
    not sensor inputs. Firmware cannot make those electrical load faults go away
    by "sending" a signal. Use properly rated dummy loads, the original solenoids
    mounted remotely, or an automotive load emulator, and keep the heat/fusing
    design conservative.

  Electrical interface:
    Do not connect microcontroller pins directly to 12 V vehicle wiring. Use
    optocouplers, open-drain transistor stages, or protected automotive I/O.
    The default outputs below assume a HIGH from the MCU turns on a transistor
    that pulls the Civic ECU/sensor line to ground.

  CAN backend:
    By default the sketch builds without a CAN library and can be bench-tested
    with Serial commands. To use an MCP2515 module, install the MCP_CAN_lib
    library and set USE_MCP2515_CAN to 1.
*/

#define USE_MCP2515_CAN 0

#if USE_MCP2515_CAN
#include <SPI.h>
#include <mcp_can.h>
#endif

// -------------------------- Pin assignments --------------------------
// Pulse outputs. These drive open-drain transistor/opto stages.
const int PIN_VSS_OUT = 2;
const int PIN_MAINSHAFT_OUT = 3;
const int PIN_COUNTERSHAFT_OUT = 4;

// Range switch emulator outputs. Active means "pull the PCM line low".
const int PIN_ATP_NP_OUT = 5;   // P/N range signal, used by many OBD2A Hondas
const int PIN_ATP_R_OUT = 6;
const int PIN_ATP_D4_OUT = 7;
const int PIN_ATP_D3_OUT = 8;
const int PIN_ATP_2_OUT = 9;

// Optional extra range lines. Leave wired only if your pinout actually has them.
const int PIN_ATP_1_OUT = A0;
const int PIN_ATP_P_OUT = A1;

// MCP2515 pins. D10-D13 are reserved by SPI on Uno/Nano-style boards.
const int PIN_CAN_CS = 10;
const int PIN_CAN_INT = A2;

// Emulator enable should come from ignition/run or a service enable contact.
// Active low by default, matching the other sketches in this project.
const int PIN_EMULATOR_ENABLE = A3;
const int PIN_FAULT_OUT = A4;       // optional dashboard/VCU fault input
const int PIN_STATUS_LED = A5;      // do not use LED_BUILTIN when SPI uses D13

// -------------------------- Electrical behavior --------------------------
const uint8_t ACTIVE_LOW = LOW;
const uint8_t SINK_ON_LEVEL = HIGH;   // MCU HIGH turns on external pull-down
const uint8_t SINK_OFF_LEVEL = LOW;

// If true, the optional P line is pulled low only in Park. The normal NP line
// is still pulled low in both Park and Neutral.
const bool DRIVE_OPTIONAL_P_OUTPUT = true;
const bool DRIVE_OPTIONAL_1_OUTPUT = true;

// -------------------------- CAN configuration --------------------------
// Expected default frame:
//   ID 0x201, byte 0-1 little-endian unsigned speed in 0.01 mph units,
//   optional byte 2 gear enum: 0=P, 1=R, 2=N, 3=D4, 4=D3, 5=2, 6=1.
const uint32_t CAN_SPEED_ID = 0x201;
const uint8_t CAN_MPH_BYTE_L = 0;
const uint8_t CAN_MPH_BYTE_H = 1;
const float CAN_MPH_SCALE = 0.01;
const float CAN_MPH_OFFSET = 0.0;
const bool ACCEPT_GEAR_FROM_CAN = true;
const uint8_t CAN_GEAR_BYTE = 2;
const uint32_t CAN_TIMEOUT_MS = 500;

#if USE_MCP2515_CAN
const uint8_t MCP_CAN_SPEED = CAN_500KBPS;
const uint8_t MCP_CAN_CLOCK = MCP_16MHZ;  // many cheap modules are 8 MHz; verify yours
MCP_CAN CAN0(PIN_CAN_CS);
#endif

// -------------------------- Signal calibration --------------------------
// Honda VSS is commonly treated as a 4,000 pulse/mile open-collector style
// signal. Confirm against your exact cluster/PCM by comparing GPS and cluster.
const float VSS_PULSES_PER_MILE = 4000.0;

// These shaft defaults are intentionally easy to recalibrate. They only matter
// if your PCM performs ratio/slip plausibility checks using mainshaft and
// countershaft speed sensors.
const float TIRE_DIAMETER_IN = 23.7;
const float FINAL_DRIVE_RATIO = 4.357;
const uint8_t MAINSHAFT_PULSES_PER_REV = 4;
const uint8_t COUNTERSHAFT_PULSES_PER_REV = 4;

// Approximate 4 A/T ratios. Replace with the ratios for your transmission code.
const float GEAR_RATIO_1 = 2.722;
const float GEAR_RATIO_2 = 1.468;
const float GEAR_RATIO_3 = 0.975;
const float GEAR_RATIO_4 = 0.638;
const float GEAR_RATIO_R = 1.954;

// Virtual shift map used only to generate believable shaft speeds while the
// selector is in D4/D3. It does not command the vehicle.
const float SHIFT_1_TO_2_MPH = 12.0;
const float SHIFT_2_TO_3_MPH = 24.0;
const float SHIFT_3_TO_4_MPH = 43.0;

const float MAX_REASONABLE_MPH = 130.0;
const float MIN_PULSE_HZ = 0.10;
const uint32_t MIN_HALF_PERIOD_US = 250;  // caps pulse output near 2 kHz

// -------------------------- Runtime state --------------------------
enum GearPosition {
  GEAR_P,
  GEAR_R,
  GEAR_N,
  GEAR_D4,
  GEAR_D3,
  GEAR_2,
  GEAR_1,
  GEAR_INVALID
};

struct PulseOutput {
  int pin;
  bool running;
  bool level;
  uint32_t halfPeriodUs;
  uint32_t nextToggleUs;
};

PulseOutput vssPulse = {PIN_VSS_OUT, false, false, 0, 0};
PulseOutput mainshaftPulse = {PIN_MAINSHAFT_OUT, false, false, 0, 0};
PulseOutput countershaftPulse = {PIN_COUNTERSHAFT_OUT, false, false, 0, 0};

GearPosition selectedRange = GEAR_N;
float canMph = 0.0;
float manualMph = 0.0;
bool manualSpeedOverride = false;
bool canBackendReady = false;
uint32_t lastCanSpeedMs = 0;
uint32_t lastSignalUpdateMs = 0;
uint32_t lastStatusMs = 0;
char faultReason[80] = "none";

bool pinPresent(int pin) {
  return pin >= 0;
}

void pinWriteIfPresent(int pin, uint8_t level) {
  if (pinPresent(pin)) {
    digitalWrite((uint8_t)pin, level);
  }
}

void pinModeIfPresent(int pin, uint8_t mode) {
  if (pinPresent(pin)) {
    pinMode((uint8_t)pin, mode);
  }
}

bool activeLowInput(int pin) {
  if (!pinPresent(pin)) return true;
  return digitalRead((uint8_t)pin) == ACTIVE_LOW;
}

bool emulatorEnabled() {
  return activeLowInput(PIN_EMULATOR_ENABLE);
}

void setSink(int pin, bool active) {
  pinWriteIfPresent(pin, active ? SINK_ON_LEVEL : SINK_OFF_LEVEL);
}

void stopPulse(PulseOutput &pulse) {
  pulse.running = false;
  pulse.level = false;
  setSink(pulse.pin, false);
}

void setPulseFrequency(PulseOutput &pulse, float hz) {
  if (!pinPresent(pulse.pin)) return;

  if (hz < MIN_PULSE_HZ) {
    stopPulse(pulse);
    return;
  }

  uint32_t halfPeriod = (uint32_t)(500000.0 / hz);
  if (halfPeriod < MIN_HALF_PERIOD_US) {
    halfPeriod = MIN_HALF_PERIOD_US;
  }

  if (!pulse.running || pulse.halfPeriodUs != halfPeriod) {
    pulse.running = true;
    pulse.level = false;
    pulse.halfPeriodUs = halfPeriod;
    pulse.nextToggleUs = micros() + pulse.halfPeriodUs;
    setSink(pulse.pin, false);
  }
}

void servicePulse(PulseOutput &pulse, uint32_t nowUs) {
  if (!pulse.running) return;

  if ((int32_t)(nowUs - pulse.nextToggleUs) >= 0) {
    pulse.level = !pulse.level;
    setSink(pulse.pin, pulse.level);
    pulse.nextToggleUs += pulse.halfPeriodUs;

    // If the loop was delayed badly, resynchronize instead of burst-toggling.
    if ((int32_t)(nowUs - pulse.nextToggleUs) > (int32_t)pulse.halfPeriodUs) {
      pulse.nextToggleUs = nowUs + pulse.halfPeriodUs;
    }
  }
}

void stopAllPulses() {
  stopPulse(vssPulse);
  stopPulse(mainshaftPulse);
  stopPulse(countershaftPulse);
}

const char *gearName(GearPosition gear) {
  switch (gear) {
    case GEAR_P: return "P";
    case GEAR_R: return "R";
    case GEAR_N: return "N";
    case GEAR_D4: return "D4";
    case GEAR_D3: return "D3";
    case GEAR_2: return "2";
    case GEAR_1: return "1";
    case GEAR_INVALID: return "INVALID";
  }
  return "?";
}

GearPosition decodeCanGear(uint8_t raw) {
  switch (raw) {
    case 0: return GEAR_P;
    case 1: return GEAR_R;
    case 2: return GEAR_N;
    case 3: return GEAR_D4;
    case 4: return GEAR_D3;
    case 5: return GEAR_2;
    case 6: return GEAR_1;
    default: return GEAR_INVALID;
  }
}

float clampMph(float mph) {
  if (mph < 0.0) return 0.0;
  if (mph > MAX_REASONABLE_MPH) return MAX_REASONABLE_MPH;
  return mph;
}

bool canSpeedFresh() {
#if USE_MCP2515_CAN
  return canBackendReady && ((millis() - lastCanSpeedMs) <= CAN_TIMEOUT_MS);
#else
  return false;
#endif
}

float currentMph() {
  if (manualSpeedOverride) {
    return clampMph(manualMph);
  }

  if (canSpeedFresh()) {
    return clampMph(canMph);
  }

  return 0.0;
}

void setFaultText(const char *reason) {
  strncpy(faultReason, reason, sizeof(faultReason) - 1);
  faultReason[sizeof(faultReason) - 1] = '\0';
}

bool speedSourceValid() {
  return manualSpeedOverride || canSpeedFresh();
}

uint8_t virtualForwardGear(GearPosition range, float mph) {
  if (range == GEAR_1) return 1;
  if (range == GEAR_2) return 2;

  uint8_t gear = 1;
  if (mph >= SHIFT_1_TO_2_MPH) gear = 2;
  if (mph >= SHIFT_2_TO_3_MPH) gear = 3;
  if (range == GEAR_D4 && mph >= SHIFT_3_TO_4_MPH) gear = 4;
  if (range == GEAR_D3 && gear > 3) gear = 3;
  return gear;
}

float ratioForVirtualGear(uint8_t gear) {
  switch (gear) {
    case 1: return GEAR_RATIO_1;
    case 2: return GEAR_RATIO_2;
    case 3: return GEAR_RATIO_3;
    case 4: return GEAR_RATIO_4;
    default: return GEAR_RATIO_3;
  }
}

void updateRangeOutputs() {
  if (!emulatorEnabled() || selectedRange == GEAR_INVALID) {
    setSink(PIN_ATP_NP_OUT, false);
    setSink(PIN_ATP_R_OUT, false);
    setSink(PIN_ATP_D4_OUT, false);
    setSink(PIN_ATP_D3_OUT, false);
    setSink(PIN_ATP_2_OUT, false);
    setSink(PIN_ATP_1_OUT, false);
    setSink(PIN_ATP_P_OUT, false);
    return;
  }

  setSink(PIN_ATP_NP_OUT, selectedRange == GEAR_P || selectedRange == GEAR_N);
  setSink(PIN_ATP_R_OUT, selectedRange == GEAR_R);
  setSink(PIN_ATP_D4_OUT, selectedRange == GEAR_D4);
  setSink(PIN_ATP_D3_OUT, selectedRange == GEAR_D3);
  setSink(PIN_ATP_2_OUT, selectedRange == GEAR_2);
  setSink(PIN_ATP_1_OUT, DRIVE_OPTIONAL_1_OUTPUT && selectedRange == GEAR_1);
  setSink(PIN_ATP_P_OUT, DRIVE_OPTIONAL_P_OUTPUT && selectedRange == GEAR_P);
}

void updatePulseTargets() {
  float mph = currentMph();

  if (!emulatorEnabled()) {
    setFaultText("disabled");
    stopAllPulses();
    return;
  }

  if (!speedSourceValid()) {
    setFaultText("speed source timeout");
    stopAllPulses();
    return;
  }

  setFaultText("none");

  float vssHz = mph * VSS_PULSES_PER_MILE / 3600.0;
  setPulseFrequency(vssPulse, vssHz);

  float wheelRpm = mph * 336.13 / TIRE_DIAMETER_IN;
  float countershaftRpm = wheelRpm * FINAL_DRIVE_RATIO;
  float mainshaftRpm = 0.0;

  if (selectedRange == GEAR_R) {
    mainshaftRpm = countershaftRpm * GEAR_RATIO_R;
  } else if (selectedRange == GEAR_D4 || selectedRange == GEAR_D3 ||
             selectedRange == GEAR_2 || selectedRange == GEAR_1) {
    mainshaftRpm = countershaftRpm * ratioForVirtualGear(virtualForwardGear(selectedRange, mph));
  }

  float countershaftHz = countershaftRpm * COUNTERSHAFT_PULSES_PER_REV / 60.0;
  float mainshaftHz = mainshaftRpm * MAINSHAFT_PULSES_PER_REV / 60.0;

  setPulseFrequency(countershaftPulse, countershaftHz);
  setPulseFrequency(mainshaftPulse, mainshaftHz);
}

void serviceCan() {
#if USE_MCP2515_CAN
  while (CAN0.checkReceive() == CAN_MSGAVAIL) {
    unsigned long rxId = 0;
    byte len = 0;
    byte buf[8];
    CAN0.readMsgBuf(&rxId, &len, buf);

    uint32_t normalizedId = rxId & 0x1FFFFFFFUL;
    if (normalizedId != CAN_SPEED_ID) {
      continue;
    }

    uint8_t needed = CAN_MPH_BYTE_H + 1;
    if (ACCEPT_GEAR_FROM_CAN && CAN_GEAR_BYTE >= needed) {
      needed = CAN_GEAR_BYTE + 1;
    }
    if (len < needed) {
      continue;
    }

    uint16_t rawMph = ((uint16_t)buf[CAN_MPH_BYTE_H] << 8) | buf[CAN_MPH_BYTE_L];
    canMph = clampMph(rawMph * CAN_MPH_SCALE + CAN_MPH_OFFSET);
    lastCanSpeedMs = millis();

    if (ACCEPT_GEAR_FROM_CAN) {
      GearPosition decoded = decodeCanGear(buf[CAN_GEAR_BYTE]);
      if (decoded != GEAR_INVALID) {
        selectedRange = decoded;
      }
    }
  }
#endif
}

void printHelp() {
  Serial.println();
  Serial.println(F("Civic automatic-transmission signal emulator"));
  Serial.println(F("Serial bench commands:"));
  Serial.println(F("  s or ?   status"));
  Serial.println(F("  M42.5    set manual MPH override"));
  Serial.println(F("  C        clear manual MPH override and use CAN"));
  Serial.println(F("  P R N D  set range to Park/Reverse/Neutral/D4"));
  Serial.println(F("  3 2 1    set range to D3/2/1"));
  Serial.println(F("  h        help"));
  Serial.println();
}

void printStatus() {
  Serial.print(F("enabled="));
  Serial.print(emulatorEnabled());
  Serial.print(F(" range="));
  Serial.print(gearName(selectedRange));
  Serial.print(F(" mph="));
  Serial.print(currentMph(), 2);
  Serial.print(F(" source="));
  Serial.print(manualSpeedOverride ? F("serial") : F("CAN"));
  Serial.print(F(" valid="));
  Serial.print(speedSourceValid());
  Serial.print(F(" fault="));
  Serial.print(faultReason);
  Serial.print(F(" vssHalfUs="));
  Serial.print(vssPulse.halfPeriodUs);
  Serial.print(F(" mainHalfUs="));
  Serial.print(mainshaftPulse.halfPeriodUs);
  Serial.print(F(" counterHalfUs="));
  Serial.println(countershaftPulse.halfPeriodUs);
}

void handleSerial() {
  if (!Serial.available()) return;

  char c = Serial.read();
  switch (c) {
    case 'h':
    case 'H':
      printHelp();
      break;
    case 's':
    case 'S':
    case '?':
      printStatus();
      break;
    case 'M':
    case 'm':
      manualMph = clampMph(Serial.parseFloat());
      manualSpeedOverride = true;
      printStatus();
      break;
    case 'C':
    case 'c':
      manualSpeedOverride = false;
      printStatus();
      break;
    case 'P':
    case 'p':
      selectedRange = GEAR_P;
      printStatus();
      break;
    case 'R':
    case 'r':
      selectedRange = GEAR_R;
      printStatus();
      break;
    case 'N':
    case 'n':
      selectedRange = GEAR_N;
      printStatus();
      break;
    case 'D':
    case 'd':
      selectedRange = GEAR_D4;
      printStatus();
      break;
    case '3':
      selectedRange = GEAR_D3;
      printStatus();
      break;
    case '2':
      selectedRange = GEAR_2;
      printStatus();
      break;
    case '1':
      selectedRange = GEAR_1;
      printStatus();
      break;
    case '\r':
    case '\n':
      break;
    default:
      Serial.println(F("Unknown command. h for help."));
      break;
  }
}

void setupPins() {
  pinModeIfPresent(PIN_VSS_OUT, OUTPUT);
  pinModeIfPresent(PIN_MAINSHAFT_OUT, OUTPUT);
  pinModeIfPresent(PIN_COUNTERSHAFT_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_NP_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_R_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_D4_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_D3_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_2_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_1_OUT, OUTPUT);
  pinModeIfPresent(PIN_ATP_P_OUT, OUTPUT);
  pinModeIfPresent(PIN_FAULT_OUT, OUTPUT);
  pinModeIfPresent(PIN_STATUS_LED, OUTPUT);

  pinModeIfPresent(PIN_EMULATOR_ENABLE, INPUT_PULLUP);
  pinModeIfPresent(PIN_CAN_INT, INPUT_PULLUP);

  stopAllPulses();
  updateRangeOutputs();
  setSink(PIN_FAULT_OUT, false);
  pinWriteIfPresent(PIN_STATUS_LED, LOW);
}

void setupCan() {
#if USE_MCP2515_CAN
  if (CAN0.begin(MCP_ANY, MCP_CAN_SPEED, MCP_CAN_CLOCK) == CAN_OK) {
    CAN0.setMode(MCP_NORMAL);
    canBackendReady = true;
    Serial.println(F("MCP2515 CAN ready."));
  } else {
    canBackendReady = false;
    Serial.println(F("MCP2515 CAN init failed; serial override still available."));
  }
#else
  canBackendReady = false;
  Serial.println(F("CAN backend disabled at compile time; use serial M command for bench test."));
#endif
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);

  setupPins();
  setupCan();
  printHelp();
  printStatus();
}

void loop() {
  serviceCan();
  handleSerial();

  uint32_t nowMs = millis();
  if (nowMs - lastSignalUpdateMs >= 20) {
    lastSignalUpdateMs = nowMs;
    updateRangeOutputs();
    updatePulseTargets();
    setSink(PIN_FAULT_OUT, strcmp(faultReason, "none") != 0);
    pinWriteIfPresent(PIN_STATUS_LED, strcmp(faultReason, "none") == 0 ? HIGH : LOW);
  }

  uint32_t nowUs = micros();
  servicePulse(vssPulse, nowUs);
  servicePulse(mainshaftPulse, nowUs);
  servicePulse(countershaftPulse, nowUs);

  if (nowMs - lastStatusMs >= 1000) {
    lastStatusMs = nowMs;
    printStatus();
  }
}
