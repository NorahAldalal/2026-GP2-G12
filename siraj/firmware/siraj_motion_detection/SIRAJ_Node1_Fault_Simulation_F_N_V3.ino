/* ======================================================================
   SIRAJ  —  NODE 1  —  DATA COLLECTION FIRMWARE
   ----------------------------------------------------------------------
   Reads every sensor on node 1, drives the lamp by PWM, and sends one
   reading every SEND_INTERVAL_MS to:
       1) update_lamp.php on the laptop, which writes BOTH
            - the sheet siraj_readings.csv (all readings, opens in Excel)
            - the MySQL database (same tables as before)
       2) the USB serial port, as CSV lines

   Compensation link (to node 2, protocol V2):
       Every 500 ms Node 1 broadcasts requested PWM and an estimated
       delivered brightness based on INA219 lamp current (if valid).
       Node 2 adds the measured shortfall to its own 40% baseline.
       NOTE: INA219 measures electrical current, NOT optical lux.
   Demo mode: type F in Node 1 Serial Monitor to reduce applied PWM by 20
   percentage points without changing the model target. Type N to restore.
   This tests compensation logic but does NOT verify fault detection by INA.
       Calibrate COMP_CALIBRATED_FULL_MA using the actual lamp before
       interpreting the estimate as an accurate light percentage.

   Lamp control for experiments:
       - set any brightness 0..100 %
       - "boost": raise the lamp by BOOST_PCT for BOOST_SECONDS, then return
     from the serial monitor or from a web page served by the device.

   Board:  Tools -> Board -> "ESP32S3 Dev Module"   (NOT "ESP32 Dev Module")

   Pin map (node 1)
       I2C bus A   SDA 8, SCL 9    INA219 (0x40), DS3231 (0x68)
       Ambient BH1750              SDA 41, SCL 40  (its own two pins, see AMBIENT_ON_OWN_PINS)
       I2C bus B   SDA 12, SCL 13  road BH1750 (0x23)
       TSL237 OUT  GPIO 5          sky brightness (frequency)
       DS18B20     GPIO 6          optional, see USE_DS18B20
       LD2410C     OUT -> GPIO 7,  TX -> GPIO 18,  RX <- GPIO 17   (human)
       LD2451      OT1 -> GPIO 10, TX -> GPIO 16 (RX on GPIO 15, not used: listen only)
       Lamp MOSFET GPIO 11         gate through 100R, 10k gate-to-source
   ====================================================================== */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>  // ArduinoJson 7
#define APPLY_MODEL_PWM 1  // 0 = serial verification, 1 = drive the lamp
#include <WebServer.h>
#include <Wire.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Set to 1 only if the OneWire and DallasTemperature libraries are installed.
#define USE_DS18B20 0
#if USE_DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#endif

// ======================================================================
//  SETTINGS TO EDIT
// ======================================================================

// ---- WiFi (2.4 GHz router) ----
const char* WIFI_SSID     = "E5785_71C2";
const char* WIFI_PASSWORD = "bYATLin2DHg";

// ---- server ----
const char* SERVER_URL = "http://192.168.8.116/Siraj/api/update_lamp.php";
#define LAMP_ID 1

// ---- fixed position of this node (there is no GPS). 0 = do not send ----
const float FIXED_LAT = 0.0;      // example: 24.7136
const float FIXED_LNG = 0.0;      // example: 46.6753

// ---- how often one reading is sent ----
const uint32_t SEND_INTERVAL_MS = 5000;

// ---- lamp behaviour ----
const uint8_t  DEFAULT_PCT   = 70;   // brightness after power-up
const uint8_t  BOOST_PCT     = 10;   // how much a boost adds
const uint16_t BOOST_SECONDS = 5;    // how long a boost lasts
#define AUTO_BOOST_ON_MOTION 0       // 1 = boost by itself whenever motion is detected

// ---- compensation link to node 2 (ESP-NOW broadcast, V2) ----
#define COMP_LINK_ENABLED 1
const uint8_t COMP_CHANNEL_NO_WIFI = 1;
// Set to measured healthy-lamp CURRENT (mA) at a 100% command.
// 0 = estimate from lamp electrical model; MUST calibrate for quantitative tests.
const float COMP_CALIBRATED_FULL_MA = 0.0f;
const uint8_t COMP_MIN_MEASURE_PCT = 10;  // avoid treating near-off sensor noise as a fault
const uint32_t COMP_RAMP_SETTLE_MS = 650; // delay estimates while normal PWM ramp is underway
const uint32_t COMP_SENSOR_MAX_AGE_MS = 900;
// F: simulate losing 20 PWM percentage points; N: restore normal model output.
const uint8_t SIMULATED_FAULT_LOSS_PCT = 20;

// ---- battery protection ----
// Running from the 4-cell lithium pack: set this to 13.0. Below that voltage the lamp is
// switched off so the cells are not drained too far (it comes back above 14.0 V).
// Running from a 12 V supply or adapter: leave it at 0 (off).
const float LOW_BATTERY_V = 13.0f;

// ---- lamp supply ----
// 12.0 = the lamp has its own step-down set to this voltage, wired like this:
//            INA219 VIN-  ->  step-down IN+ ... OUT+  ->  lamp red wire
//        The lamp then always sees this voltage, whatever the battery voltage is.
// 0    = no lamp step-down: the lamp is fed straight from the supply / battery and the
//        code shortens the pulses to protect it.
const float LAMP_REG_V     = 12.0f;
const float LAMP_REG_EFF   = 0.88f;   // efficiency of that step-down (LM2596 is about 0.85-0.90)
const float LAMP_REG_IQ_MA = 6.0f;    // what the step-down draws by itself with the lamp off

// ======================================================================
//  PINS
// ======================================================================
// Ambient light sensor: 1 = on its own two pins (SDA 40, SCL 41), so a fault on it cannot
// disturb the INA219 and the clock.  0 = shares pins 8 / 9 with them (the old wiring).
#define AMBIENT_ON_OWN_PINS 1
#define PIN_AMB_SDA    41
#define PIN_AMB_SCL    40

#define PIN_SDA_A      8
#define PIN_SCL_A      9
#define PIN_SDA_B      12
#define PIN_SCL_B      13
#define PIN_TSL        5
#define PIN_DS18       6
#define PIN_HUMAN_OUT  7
#define PIN_VEH_OT1    10
#define PIN_LAMP       11
#define PIN_R1_RX      18     // LD2410C TX -> here
#define PIN_R1_TX      17     // LD2410C RX <- here
// LD2451 (vehicle radar): its TX wire is on GPIO 16. The code only LISTENS to this radar
// (it never sends it anything); its settings are changed with the HLKRadarTool app.
#define PIN_R2_RX      16     // LD2451 TX -> here
#define PIN_R2_UNUSED  15     // LD2451 RX is soldered here; left as an input, never driven

// ======================================================================
//  LAMP MODEL  (measured on this lamp: LED string + internal resistor)
//  The duty is corrected for the supply voltage so that "100 %" is the
//  same light on a 12 V supply and on the 16 V battery, and the lamp is
//  never driven harder than it is at 12 V.
// ======================================================================
const float LAMP_VF     = 8.7f;     // V
const float LAMP_R      = 11.0f;    // ohm
const float LAMP_I_FULL = 0.30f;    // A average at 100 %
#define PWM_FREQ   1000
#define PWM_RES    10
#define PWM_MAX    1023
#define PWM_CH     0                // core 2.x only
const uint16_t SAFE_FALLBACK_DUTY = 440;   // 43 %: safe at 16.8 V, used when the voltage is unknown

// ======================================================================
//  I2C DEVICES
// ======================================================================
#define ADDR_BH      0x23
#define ADDR_INA219  0x40
#define ADDR_DS3231  0x68
TwoWire busA = TwoWire(0);
TwoWire busB = TwoWire(1);
bool okAmbient = false, okRoad = false, okIna = false, okRtc = false;

#if USE_DS18B20
OneWire oneWire(PIN_DS18);
DallasTemperature ds18(&oneWire);
#endif

// ======================================================================
//  ONE RECORD  =  ONE ROW OF THE DATASET
// ======================================================================
struct Record {
  uint32_t seq;
  uint32_t uptimeS;
  char     rtc[20];        // YYYY-MM-DD HH:MM:SS from the DS3231
  float    luxAmbient;
  float    luxRoad;
  float    skyHz;          // TSL237 frequency (higher = brighter sky)
  uint8_t  human;          // seen at any moment since the previous record
  uint8_t  vehicle;
  uint8_t  motion;         // human OR vehicle
  uint8_t  humanState;     // LD2410C: 0 none, 1 moving, 2 still, 3 both, 255 unknown
  int16_t  humanDistCm;    // -1 unknown
  uint8_t  vehCount;       // most targets seen at once since previous record
  int16_t  vehDistM;       // nearest, -1 none
  int16_t  vehSpeedKmh;    // fastest, -1 none
  uint8_t  vehApproaching; // 1 if any target was approaching
  uint8_t  lampPct;        // commanded brightness now
  float    lampDutyPct;    // real PWM duty after voltage correction
  uint8_t  boost;          // 1 while a boost is active
  float    busV;           // supply / battery voltage
  float    lampMa;         // average lamp current
  uint8_t  lampFault;      // 1 = lamp draws far less than it should
  float    tempC;
  char     status[4];      // "on" / "off"
};

QueueHandle_t     recQueue;
SemaphoreHandle_t lastMutex;
Record            lastRec;
bool              haveLast = false;

volatile uint32_t sentOk = 0, sentFail = 0;
volatile int      lastHttpCode = 0;

// ======================================================================
//  LAMP STATE
// ======================================================================
struct ModelDecision { uint8_t percent; uint32_t sampledAt; };
QueueHandle_t modelQueue;
volatile uint8_t  basePct      = DEFAULT_PCT;
volatile uint32_t boostEndMs   = 0;
volatile bool     boostActive  = false;
// Manual tests: send 0..100 via Serial or /set?pct=N, then 'm' or /model
// to resume the model. Does not change APPLY_MODEL_PWM or HTTP integration.
volatile bool     manualMode   = false;
bool              simulatedFaultActive = false;  // Serial F/N; software fault demo only
volatile uint8_t  desiredPct  = DEFAULT_PCT;  // latest desired level BEFORE low-battery override
uint8_t  currentPct   = 0;
uint16_t currentDuty  = 0;
uint32_t lastRampMs   = 0;
uint32_t lastLampPctChangeMs = 0;
float    vbusFilt     = 0;      // filtered supply voltage, 0 = unknown
float    lampMaFilt   = 0;
bool     inaHasValidSample = false;
uint32_t lastInaGoodMs = 0;
bool     lampFault    = false;
uint32_t faultSinceMs = 0, okSinceMs = 0;
bool     lowBattery   = false;
uint32_t lowWarnMs    = 0;

// ======================================================================
//  RADAR STATE
// ======================================================================
struct FrameReader {
  uint8_t  buf[96];
  uint8_t  stage;       // 0..3 header, 4..5 length, 6 payload, 7..10 tail
  uint16_t len, got;
};
FrameReader fr1, fr2;

uint8_t  humanState   = 255;
int16_t  humanDistCm  = -1;
uint32_t humanFrameMs = 0;

uint32_t humFrames = 0, vehFrames = 0, vehBytes = 0;   // for the radar check (command r)
uint32_t radarRepMs = 0;
void vehBegin() {
  pinMode(PIN_R2_UNUSED, INPUT);                  // never drive the radar's RX line
  Serial2.setRxBufferSize(1024);
  Serial2.begin(115200, SERIAL_8N1, PIN_R2_RX, -1);  // receive only
}
uint8_t  vehCountNow  = 0;
uint32_t vehSeenMs    = 0;
bool     vehEverFrame = false;

// latched between two records
bool     latHuman = false, latVehicle = false, latVehAppr = false;
uint8_t  latVehCount = 0;
int16_t  latVehDist = -1, latVehSpeed = -1;

// ======================================================================
//  LIGHT SENSOR STATE
// ======================================================================
float    luxAmbient = -1, luxRoad = -1;
uint32_t bhLastMs = 0;
bool     bhTriggered = false;

volatile uint32_t tslEdges = 0;
bool     tslIrqOn  = false;
uint32_t tslLastMs = 0;
float    skyHz     = -1;

float    tempC = -127;
uint32_t slowLastMs = 0, inaLastMs = 0, recLastMs = 0;
uint32_t recSeq = 0;

WebServer web(80);

// ======================================================================
//  LAMP
// ======================================================================
void pwmWrite(uint16_t duty) {
  if (duty > PWM_MAX) duty = PWM_MAX;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_LAMP, duty);
#else
  ledcWrite(PWM_CH, duty);
#endif
  currentDuty = duty;
}

// duty that gives 100 % light at the present supply voltage
uint16_t fullDutyNow() {
  float vLamp = vbusFilt;
  if (LAMP_REG_V > 0) {
    vLamp = LAMP_REG_V;                               // lamp has its own regulator: voltage is known
  } else if (vbusFilt < 9.5f || vbusFilt > 20.0f) {
    return SAFE_FALLBACK_DUTY;                        // voltage unknown
  }
  float iInst = (vLamp - LAMP_VF) / LAMP_R;           // current while the MOSFET is on
  if (iInst <= LAMP_I_FULL) return PWM_MAX;           // 12 V or lower: full on is safe
  float d = (float)PWM_MAX * LAMP_I_FULL / iInst;
  if (d > PWM_MAX) d = PWM_MAX;
  return (uint16_t)d;
}

void lampApply() {
  uint32_t d = (uint32_t)fullDutyNow() * currentPct / 100U;
  pwmWrite((uint16_t)d);
}

void lampService(uint32_t now) {
  if (boostActive && (int32_t)(now - boostEndMs) >= 0) boostActive = false;

  uint16_t target = basePct + (boostActive ? BOOST_PCT : 0);
#if APPLY_MODEL_PWM
  static ModelDecision decision{};
  static bool received = false;
  if (modelQueue && xQueueReceive(modelQueue, &decision, 0) == pdTRUE) received = true;
  if (received && now - decision.sampledAt < 2 * SEND_INTERVAL_MS) {
    target = decision.percent;
    if (target && boostActive) target += BOOST_PCT;
  } else {
    received = false;
  }
#endif
  if (manualMode) {
    target = basePct;
    if (target && boostActive) target += BOOST_PCT;
  }
  if (target > 100) target = 100;
  desiredPct = (uint8_t)target; // original model/manual intent before fault simulation

  // Simulate a partial lamp malfunction: the PWM output is intentionally
  // reduced while the model continues requesting its normal brightness.
  // Example: model wants 70%, F delivers 50% PWM.
  if (simulatedFaultActive) {
    target = (target > SIMULATED_FAULT_LOSS_PCT) ? (target - SIMULATED_FAULT_LOSS_PCT) : 0;
  }

  // battery protection (only when LOW_BATTERY_V is set and the voltage is being read)
  if (LOW_BATTERY_V > 0 && vbusFilt > 5.0f) {
    if (vbusFilt < LOW_BATTERY_V)             lowBattery = true;
    else if (vbusFilt > LOW_BATTERY_V + 1.0f) lowBattery = false;
  } else {
    lowBattery = false;
  }
  if (lowBattery) {
    target = 0;
    if (now - lowWarnMs > 30000) {
      lowWarnMs = now;
      Serial.printf("# LOW BATTERY %.2f V: lamp is off. Switch the device off and recharge.\n", vbusFilt);
    }
  }

  if (now - lastRampMs >= 10) {
    lastRampMs = now;
    if (currentPct < target) { currentPct++; lastLampPctChangeMs = now; }
    else if (currentPct > target) { currentPct--; lastLampPctChangeMs = now; }
    lampApply();                       // also follows slow battery voltage changes
  }
}

void startBoost() {
  boostEndMs  = millis() + (uint32_t)BOOST_SECONDS * 1000UL;
  boostActive = true;
}

// ======================================================================
//  INA219  (supply voltage + average lamp current)
// ======================================================================
bool inaWrite16(uint8_t reg, uint16_t v) {
  busA.beginTransmission(ADDR_INA219);
  busA.write(reg); busA.write(v >> 8); busA.write(v & 0xFF);
  return busA.endTransmission() == 0;
}
bool inaRead16(uint8_t reg, int16_t &out) {
  busA.beginTransmission(ADDR_INA219);
  busA.write(reg);
  if (busA.endTransmission() != 0) return false;
  if (busA.requestFrom((int)ADDR_INA219, 2) != 2) return false;
  out = (int16_t)((busA.read() << 8) | busA.read());
  return true;
}

void inaService(uint32_t now) {
  if (!okIna || now - inaLastMs < 200) return;
  inaLastMs = now;

  int16_t rawBus, rawShunt;
  if (!inaRead16(0x02, rawBus) || !inaRead16(0x01, rawShunt)) return;

  float v  = ((uint16_t)rawBus >> 3) * 0.004f;       // volts
  float ma = rawShunt * 0.1f;                        // 10 uV per bit across 0.1 ohm
  if (ma < 0) ma = -ma;
  if (v <= 5.0f || v >= 26.0f) return;  // do not declare invalid electrical data "valid"
  lastInaGoodMs = now;
  inaHasValidSample = true;
  vbusFilt = (vbusFilt < 1.0f) ? v : (vbusFilt * 0.9f + v * 0.1f);

  // With a lamp step-down the INA219 sees the current going INTO the step-down, at battery
  // voltage. Convert it to the (approximate) current in the lamp itself.
  if (LAMP_REG_V > 0) {
    ma -= LAMP_REG_IQ_MA;
    if (ma < 0) ma = 0;
    if (vbusFilt > 5.0f) ma = ma * vbusFilt * LAMP_REG_EFF / LAMP_REG_V;
  }
  lampMaFilt = lampMaFilt * 0.7f + ma * 0.3f;

  // ---- lamp fault: drawing far less current than the PWM should give ----
  float vLamp    = (LAMP_REG_V > 0) ? LAMP_REG_V : vbusFilt;
  float iInst    = (vLamp - LAMP_VF) / LAMP_R;
  float expectMa = (iInst > 0) ? iInst * 1000.0f * currentDuty / PWM_MAX : 0;
  if (expectMa >= 30.0f) {
    bool low = lampMaFilt < 0.4f * expectMa;
    bool ok  = lampMaFilt > 0.7f * expectMa;
    if (low) { okSinceMs = 0; if (!faultSinceMs) faultSinceMs = now; if (now - faultSinceMs >= 3000) lampFault = true; }
    else if (ok) { faultSinceMs = 0; if (!okSinceMs) okSinceMs = now; if (now - okSinceMs >= 3000) lampFault = false; }
  } else {
    faultSinceMs = 0; okSinceMs = 0;                 // lamp off or too dim to judge
  }
}

// ======================================================================
//  BH1750 x2   (one-shot high resolution, no blocking delays)
// ======================================================================
bool bhCommand(TwoWire &bus, uint8_t cmd) {
  bus.beginTransmission(ADDR_BH);
  bus.write(cmd);
  return bus.endTransmission() == 0;
}
float bhFetch(TwoWire &bus) {
  if (bus.requestFrom((int)ADDR_BH, 2) != 2) return -1;
  uint16_t raw = (bus.read() << 8) | bus.read();
  return raw / 1.2f;
}

// ---- ambient sensor on its own pins: a small software I2C (the ESP32 has only two hardware ones) ----
static inline void swHi(uint8_t p) { pinMode(p, INPUT_PULLUP); }                 // release the line
static inline void swLo(uint8_t p) { digitalWrite(p, LOW); pinMode(p, OUTPUT); } // pull it low
static inline void swT()           { delayMicroseconds(8); }

void swStart() { swHi(PIN_AMB_SDA); swHi(PIN_AMB_SCL); swT(); swLo(PIN_AMB_SDA); swT(); swLo(PIN_AMB_SCL); swT(); }
void swStop()  { swLo(PIN_AMB_SDA); swT(); swHi(PIN_AMB_SCL); swT(); swHi(PIN_AMB_SDA); swT(); }
bool swWrite(uint8_t b) {                       // true = the sensor acknowledged
  for (uint8_t i = 0; i < 8; i++) {
    if (b & 0x80) swHi(PIN_AMB_SDA); else swLo(PIN_AMB_SDA);
    b <<= 1;
    swT(); swHi(PIN_AMB_SCL); swT(); swLo(PIN_AMB_SCL);
  }
  swHi(PIN_AMB_SDA); swT(); swHi(PIN_AMB_SCL); swT();
  bool ack = (digitalRead(PIN_AMB_SDA) == LOW);
  swLo(PIN_AMB_SCL); swT();
  return ack;
}
uint8_t swRead(bool ack) {
  uint8_t b = 0;
  swHi(PIN_AMB_SDA);
  for (uint8_t i = 0; i < 8; i++) {
    swT(); swHi(PIN_AMB_SCL); swT();
    b = (uint8_t)((b << 1) | (digitalRead(PIN_AMB_SDA) ? 1 : 0));
    swLo(PIN_AMB_SCL);
  }
  if (ack) swLo(PIN_AMB_SDA); else swHi(PIN_AMB_SDA);
  swT(); swHi(PIN_AMB_SCL); swT(); swLo(PIN_AMB_SCL); swHi(PIN_AMB_SDA); swT();
  return b;
}

bool ambPing() {
#if AMBIENT_ON_OWN_PINS
  swStart(); bool ok = swWrite(ADDR_BH << 1); swStop(); return ok;
#else
  busA.beginTransmission(ADDR_BH); return busA.endTransmission() == 0;
#endif
}
bool ambCommand(uint8_t cmd) {
#if AMBIENT_ON_OWN_PINS
  swStart(); bool ok = swWrite(ADDR_BH << 1) && swWrite(cmd); swStop(); return ok;
#else
  return bhCommand(busA, cmd);
#endif
}
float ambFetch() {
#if AMBIENT_ON_OWN_PINS
  swStart();
  if (!swWrite((ADDR_BH << 1) | 1)) { swStop(); return -1; }
  uint8_t hi = swRead(true), lo = swRead(false);
  swStop();
  return (uint16_t)((hi << 8) | lo) / 1.2f;
#else
  return bhFetch(busA);
#endif
}

void bhService(uint32_t now) {
  if (now - bhLastMs < 250) return;
  bhLastMs = now;
  if (bhTriggered) {
    if (okAmbient) luxAmbient = ambFetch();
    if (okRoad)    luxRoad    = bhFetch(busB);
  }
  if (okAmbient) ambCommand(0x20);
  if (okRoad)    bhCommand(busB, 0x20);
  bhTriggered = true;
}

// ======================================================================
//  TSL237  (sky brightness as a frequency)
//  Dark: edges are counted with an interrupt.  Bright (daylight): the
//  interrupt is switched off and one pulse is timed instead, so a very
//  fast signal cannot overload the processor.
// ======================================================================
void IRAM_ATTR tslIsr() { tslEdges++; }

void tslService(uint32_t now) {
  if (now - tslLastMs < 1000) return;
  uint32_t elapsed = now - tslLastMs;
  tslLastMs = now;

  if (tslIrqOn) {
    noInterrupts(); uint32_t n = tslEdges; tslEdges = 0; interrupts();
    skyHz = n * 1000.0f / elapsed;
    if (skyHz > 20000.0f) { detachInterrupt(digitalPinToInterrupt(PIN_TSL)); tslIrqOn = false; }
  } else {
    unsigned long hi = pulseIn(PIN_TSL, HIGH, 3000);          // microseconds
    float hz = (hi > 0) ? 500000.0f / hi : 0;
    if (hi > 0) skyHz = hz;
    if (hi == 0 || hz < 10000.0f) {
      noInterrupts(); tslEdges = 0; interrupts();
      attachInterrupt(digitalPinToInterrupt(PIN_TSL), tslIsr, RISING);
      tslIrqOn = true;
    }
  }
}

// ======================================================================
//  DS3231
// ======================================================================
uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

bool rtcRead(uint8_t reg, uint8_t *dst, uint8_t n) {
  busA.beginTransmission(ADDR_DS3231);
  busA.write(reg);
  if (busA.endTransmission() != 0) return false;
  if (busA.requestFrom((int)ADDR_DS3231, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) dst[i] = busA.read();
  return true;
}

// if the clock was never set, set it to the time this sketch was compiled
void rtcSetFromCompileTimeIfNeeded() {
  uint8_t t[7];
  if (!rtcRead(0x00, t, 7)) return;
  if (bcd2dec(t[6]) >= 25) return;                    // year looks valid

  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = { __DATE__[0], __DATE__[1], __DATE__[2], 0 };
  const char* p = strstr(months, mon);
  uint8_t month = p ? (uint8_t)((p - months) / 3 + 1) : 1;
  uint8_t day   = (uint8_t)atoi(__DATE__ + 4);
  uint8_t year  = (uint8_t)(atoi(__DATE__ + 7) - 2000);
  uint8_t hh = (uint8_t)atoi(__TIME__), mm = (uint8_t)atoi(__TIME__ + 3), ss = (uint8_t)atoi(__TIME__ + 6);

  busA.beginTransmission(ADDR_DS3231);
  busA.write(0x00);
  busA.write(dec2bcd(ss)); busA.write(dec2bcd(mm)); busA.write(dec2bcd(hh));
  busA.write(1);
  busA.write(dec2bcd(day)); busA.write(dec2bcd(month)); busA.write(dec2bcd(year));
  busA.endTransmission();
}

void rtcString(char *out, size_t n) {
  uint8_t t[7];
  if (!okRtc || !rtcRead(0x00, t, 7)) { snprintf(out, n, "0000-00-00 00:00:00"); return; }
  snprintf(out, n, "20%02u-%02u-%02u %02u:%02u:%02u",
           bcd2dec(t[6]), bcd2dec(t[5] & 0x1F), bcd2dec(t[4]),
           bcd2dec(t[2] & 0x3F), bcd2dec(t[1]), bcd2dec(t[0] & 0x7F));
}

void slowService(uint32_t now) {             // temperature, every 2 s
  if (now - slowLastMs < 2000) return;
  slowLastMs = now;
#if USE_DS18B20
  float t = ds18.getTempCByIndex(0);
  ds18.requestTemperatures();
  if (t > -100) { tempC = t; return; }
#endif
  uint8_t r[2];
  if (okRtc && rtcRead(0x11, r, 2)) tempC = (int8_t)r[0] + (r[1] >> 6) * 0.25f;
}

// ======================================================================
//  RADARS   (both use frames  F4 F3 F2 F1 | len lo hi | payload | F8 F7 F6 F5)
// ======================================================================
bool frameFeed(FrameReader &f, uint8_t b) {
  static const uint8_t HDR[4]  = { 0xF4, 0xF3, 0xF2, 0xF1 };
  static const uint8_t TAIL[4] = { 0xF8, 0xF7, 0xF6, 0xF5 };

  if (f.stage < 4) {
    if (b == HDR[f.stage]) f.stage++;
    else f.stage = (b == HDR[0]) ? 1 : 0;
    return false;
  }
  if (f.stage == 4) { f.len = b; f.stage = 5; return false; }
  if (f.stage == 5) {
    f.len |= (uint16_t)b << 8;
    f.got = 0;
    if (f.len > sizeof(f.buf)) { f.stage = 0; return false; }
    f.stage = (f.len == 0) ? 7 : 6;
    return false;
  }
  if (f.stage == 6) {
    f.buf[f.got++] = b;
    if (f.got >= f.len) f.stage = 7;
    return false;
  }
  // tail
  if (b != TAIL[f.stage - 7]) { f.stage = (b == HDR[0]) ? 1 : 0; return false; }
  if (++f.stage > 10) { f.stage = 0; return true; }
  return false;
}

void radarService(uint32_t now) {
  // ---- LD2410C : human ----
  while (Serial1.available()) {
    if (frameFeed(fr1, (uint8_t)Serial1.read())) {
      humFrames++;
      // payload: type, 0xAA, state, moveDist(2), moveEnergy, stillDist(2), stillEnergy, detectDist(2) ...
      if (fr1.len >= 11 && fr1.buf[1] == 0xAA) {
        humanState   = fr1.buf[2];
        int16_t dMove   = (int16_t)(fr1.buf[3] | (fr1.buf[4] << 8));
        int16_t dStill  = (int16_t)(fr1.buf[6] | (fr1.buf[7] << 8));
        int16_t dDetect = (int16_t)(fr1.buf[9] | (fr1.buf[10] << 8));
        if (humanState == 0)        humanDistCm = -1;
        else if (dDetect > 0)       humanDistCm = dDetect;
        else if (humanState & 0x01) humanDistCm = dMove;
        else                        humanDistCm = dStill;
        humanFrameMs = now;
      }
    }
  }
  if (now - humanFrameMs > 2000) { humanState = 255; humanDistCm = -1; }

  bool humanNow = (digitalRead(PIN_HUMAN_OUT) == HIGH);
  if (humanNow) latHuman = true;

  // ---- LD2451 : vehicles ----
  while (Serial2.available()) {
    vehBytes++;
    if (frameFeed(fr2, (uint8_t)Serial2.read())) {
      vehFrames++;
      vehEverFrame = true;
      uint8_t n = 0;
      if (fr2.len >= 2) {
        n = fr2.buf[0];                                   // target count
        uint8_t fit = (uint8_t)((fr2.len - 2) / 5);
        if (n > fit) n = fit;
      }
      vehCountNow = n;
      if (n > 0) {
        vehSeenMs  = now;
        latVehicle = true;
        if (n > latVehCount) latVehCount = n;
        for (uint8_t i = 0; i < n; i++) {
          const uint8_t *t = &fr2.buf[2 + i * 5];        // angle, distance, direction, speed, snr
          int16_t dist = t[1], speed = t[3];
          if (latVehDist < 0 || dist < latVehDist) latVehDist = dist;
          if (speed > latVehSpeed) latVehSpeed = speed;
          if (t[2] == 0x00) latVehAppr = true;            // 00 = approaching
        }
      }
    }
  }
  if (now - vehSeenMs > 1000) vehCountNow = 0;

  if (digitalRead(PIN_VEH_OT1) == HIGH && !vehEverFrame) latVehicle = true;   // fallback if no serial data

#if AUTO_BOOST_ON_MOTION
  if (humanNow || vehCountNow > 0) startBoost();
#endif
}

// ======================================================================
//  BUILD ONE RECORD
// ======================================================================
void makeRecord(uint32_t now) {
  Record r;
  memset(&r, 0, sizeof(r));
  r.seq      = ++recSeq;
  r.uptimeS  = now / 1000;
  rtcString(r.rtc, sizeof(r.rtc));
  r.luxAmbient = luxAmbient;
  r.luxRoad    = luxRoad;
  r.skyHz      = skyHz;
  r.human      = latHuman ? 1 : 0;
  r.vehicle    = latVehicle ? 1 : 0;
  r.motion     = (latHuman || latVehicle) ? 1 : 0;
  r.humanState = humanState;
  r.humanDistCm = humanDistCm;
  r.vehCount   = latVehCount;
  r.vehDistM   = latVehDist;
  r.vehSpeedKmh = latVehSpeed;
  r.vehApproaching = latVehAppr ? 1 : 0;
  r.lampPct    = currentPct;
  r.lampDutyPct = currentDuty * 100.0f / PWM_MAX;
  r.boost      = boostActive ? 1 : 0;
  r.busV       = vbusFilt;
  r.lampMa     = lampMaFilt;
  r.lampFault  = lampFault ? 1 : 0;
  r.tempC      = tempC;
  strcpy(r.status, (currentPct > 0 && !lampFault) ? "on" : "off");

  latHuman = latVehicle = latVehAppr = false;
  latVehCount = 0; latVehDist = -1; latVehSpeed = -1;

  Serial.printf("%lu,%s,%lu,%.1f,%.1f,%.1f,%u,%u,%u,%u,%d,%u,%d,%d,%u,%u,%.1f,%u,%.2f,%.1f,%u,%.2f,%s\n",
                (unsigned long)r.seq, r.rtc, (unsigned long)r.uptimeS,
                r.luxAmbient, r.luxRoad, r.skyHz,
                r.human, r.vehicle, r.motion, r.humanState, r.humanDistCm,
                r.vehCount, r.vehDistM, r.vehSpeedKmh, r.vehApproaching,
                r.lampPct, r.lampDutyPct, r.boost, r.busV, r.lampMa, r.lampFault,
                r.tempC, r.status);

  if (xSemaphoreTake(lastMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    lastRec = r; haveLast = true;
    xSemaphoreGive(lastMutex);
  }
  xQueueSend(recQueue, &r, 0);        // dropped silently if WiFi is down and the queue is full
}

// ======================================================================
//  NETWORK TASK   (runs beside the main loop, so a slow server can never
//                  delay the sensors or the lamp)
// ======================================================================
bool httpPost(const char* url, const String &body, const Record &r) {
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.setConnectTimeout(2000);
  http.setTimeout(3000);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(body);
  Serial.printf("# HTTP seq=%lu code=%d\n",
              (unsigned long)r.seq, code);
  if (code == 200) {
    JsonDocument reply;
    DeserializationError error = deserializeJson(reply, http.getString());
    const uint32_t sampledAt = r.uptimeS * 1000UL;
    if (!error && reply["prediction_status"] == "ok" && reply["lamp_id"] == LAMP_ID &&
        reply["seq"].is<uint32_t>() && reply["seq"].as<uint32_t>() == r.seq &&
        reply["pwm"].is<float>() && millis() - sampledAt < SEND_INTERVAL_MS) {
      float pwm = reply["pwm"].as<float>();
      if (isfinite(pwm) && pwm >= 0 && pwm <= 100) {
        Serial.printf("# MODEL seq=%lu pwm=%.2f%% mode=%s\n", (unsigned long)r.seq, pwm,
                      APPLY_MODEL_PWM ? "control" : "monitor");
#if APPLY_MODEL_PWM
        ModelDecision decision = {(uint8_t)lroundf(pwm), sampledAt};
        if (modelQueue) xQueueOverwrite(modelQueue, &decision);
#endif
      }
    } else {
      Serial.printf("# MODEL no valid fresh decision for seq=%lu\n", (unsigned long)r.seq);
    }
  }
  http.end();
  lastHttpCode = code;
  return code >= 200 && code < 300;
}

void sendRecord(const Record &r) {
  // one message with everything -> update_lamp.php writes the sheet AND the database
  String c;
  c.reserve(480);
  c  = "lamp_id=";          c += LAMP_ID;
  c += "&seq=";             c += r.seq;
  { String t = r.rtc; t.replace(" ", "%20"); c += "&rtc_time="; c += t; }
  c += "&uptime_s=";        c += r.uptimeS;
  c += "&lux=";             c += String(r.luxAmbient < 0 ? 0 : r.luxAmbient, 2);
  c += "&lux_ambient=";     c += String(r.luxAmbient, 1);
  c += "&lux_road=";        c += String(r.luxRoad, 1);
  c += "&sky_hz=";          c += String(r.skyHz, 1);
  c += "&human=";           c += r.human;
  c += "&vehicle=";         c += r.vehicle;
  c += "&motion=";          c += r.motion;
  c += "&human_state=";     c += r.humanState;
  c += "&human_dist_cm=";   c += r.humanDistCm;
  c += "&veh_count=";       c += r.vehCount;
  c += "&veh_dist_m=";      c += r.vehDistM;
  c += "&veh_speed_kmh=";   c += r.vehSpeedKmh;
  c += "&veh_approaching="; c += r.vehApproaching;
  c += "&lamp_pct=";        c += r.lampPct;
  c += "&lamp_duty_pct=";   c += String(r.lampDutyPct, 1);
  c += "&boost=";           c += r.boost;
  c += "&battery_v=";       c += String(r.busV, 2);
  c += "&lamp_ma=";         c += String(r.lampMa, 1);
  c += "&lamp_fault=";      c += r.lampFault;
  c += "&temp_c=";          c += String(r.tempC, 2);
  c += "&status=";          c += r.status;
  c += "&lat=";             c += String(FIXED_LAT, 7);
  c += "&lng=";             c += String(FIXED_LNG, 7);
  if (httpPost(SERVER_URL, c, r)) sentOk++; else sentFail++;
}

// ---- small web page for control from the laptop ----
void webRoot() {
  Record r; bool ok = false;
  if (xSemaphoreTake(lastMutex, pdMS_TO_TICKS(50)) == pdTRUE) { r = lastRec; ok = haveLast; xSemaphoreGive(lastMutex); }

  String h;
  h.reserve(1800);
  h += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
         "<meta http-equiv='refresh' content='5'><title>Siraj node 1</title>"
         "<style>body{font-family:sans-serif;max-width:560px;margin:20px auto;padding:0 16px}"
         "td{padding:4px 12px 4px 0}a.b,button{display:inline-block;padding:10px 16px;margin:4px 4px 4px 0;"
         "border:1px solid #888;border-radius:6px;text-decoration:none;color:#000;background:#eee;font-size:16px}"
         "input{font-size:16px;padding:8px;width:80px}</style></head><body><h2>Siraj node 1</h2>");
  h += F("<p><a class='b' href='/boost'>Boost +"); h += BOOST_PCT; h += F("% for "); h += BOOST_SECONDS; h += F(" s</a></p>");
  h += F("<form action='/set'>Brightness % <input name='pct' type='number' min='0' max='100' value='"); h += basePct;
  h += F("'> <button>Set</button></form><p>");
  for (int p = 0; p <= 100; p += 10) { h += F("<a class='b' href='/set?pct="); h += p; h += F("'>"); h += p; h += F("</a>"); }
  h += F("</p><p><a href='/model'>Resume model/auto mode</a></p><table>");
  if (ok) {
    h += F("<tr><td>Lamp</td><td>");            h += r.lampPct; h += F(" % (base "); h += basePct; h += r.boost ? F(", boost on)") : F(")"); h += F("</td></tr>");
    h += F("<tr><td>Ambient light</td><td>");   h += String(r.luxAmbient, 1); h += F(" lx</td></tr>");
    h += F("<tr><td>Road light</td><td>");      h += String(r.luxRoad, 1);    h += F(" lx</td></tr>");
    h += F("<tr><td>Sky sensor</td><td>");      h += String(r.skyHz, 1);      h += F(" Hz</td></tr>");
    h += F("<tr><td>Human</td><td>");           h += r.human;   h += F("</td></tr>");
    h += F("<tr><td>Vehicle</td><td>");         h += r.vehicle; h += F("</td></tr>");
    h += F("<tr><td>Supply voltage</td><td>");  h += String(r.busV, 2);   h += F(" V</td></tr>");
    h += F("<tr><td>Lamp current</td><td>");    h += String(r.lampMa, 0); h += F(" mA</td></tr>");
    h += F("<tr><td>Lamp fault</td><td>");      h += r.lampFault; h += F("</td></tr>");
    if (lowBattery) h += F("<tr><td><b>LOW BATTERY</b></td><td><b>lamp off, switch off and recharge</b></td></tr>");
    h += F("<tr><td>Temperature</td><td>");     h += String(r.tempC, 1);  h += F(" C</td></tr>");
  }
  h += F("<tr><td>Sent to server</td><td>"); h += sentOk; h += F(" ok, "); h += sentFail; h += F(" failed (last code "); h += lastHttpCode; h += F(")</td></tr>");
  h += F("</table><p><a href='/data'>/data (JSON)</a></p></body></html>");
  web.send(200, "text/html; charset=utf-8", h);
}

void webRedirectHome() { web.sendHeader("Location", "/"); web.send(303, "text/plain", ""); }

void webSet() {
  if (web.hasArg("pct")) {
    int v = web.arg("pct").toInt();
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    basePct = (uint8_t)v;
    manualMode = true;   // the model cannot overwrite the manual test
  }
  webRedirectHome();
}
void webModel() { manualMode = false; webRedirectHome(); }
void webBoost() { startBoost(); webRedirectHome(); }

void webData() {
  Record r; bool ok = false;
  if (xSemaphoreTake(lastMutex, pdMS_TO_TICKS(50)) == pdTRUE) { r = lastRec; ok = haveLast; xSemaphoreGive(lastMutex); }
  if (!ok) { web.send(503, "application/json", "{\"status\":\"no data yet\"}"); return; }
  char j[420];
  snprintf(j, sizeof(j),
    "{\"seq\":%lu,\"rtc_time\":\"%s\",\"lux_ambient\":%.1f,\"lux_road\":%.1f,\"sky_hz\":%.1f,"
    "\"human\":%u,\"vehicle\":%u,\"motion\":%u,\"lamp_pct\":%u,\"base_pct\":%u,\"boost\":%u,"
    "\"battery_v\":%.2f,\"lamp_ma\":%.1f,\"lamp_fault\":%u,\"temp_c\":%.2f,\"status\":\"%s\"}",
    (unsigned long)r.seq, r.rtc, r.luxAmbient, r.luxRoad, r.skyHz, r.human, r.vehicle, r.motion,
    r.lampPct, (unsigned)basePct, r.boost, r.busV, r.lampMa, r.lampFault, r.tempC, r.status);
  web.send(200, "application/json", j);
}

void netTask(void *) {
  uint32_t lastTry = 0;
  bool webStarted = false, announced = false;

  for (;;) {
    uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED) {
      announced = false;
      if (WIFI_SSID[0] != 0 && (lastTry == 0 || now - lastTry > 10000)) {
        lastTry = now;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
      Record drop;
      while (uxQueueMessagesWaiting(recQueue) > 6) xQueueReceive(recQueue, &drop, 0);   // keep only recent rows
    } else {
      if (!announced) {
        announced = true;
        Serial.printf("# WiFi connected. Control page: http://%s/\n", WiFi.localIP().toString().c_str());
      }
      if (!webStarted) {
        web.on("/", webRoot);
        web.on("/set", webSet);
        web.on("/model", webModel);
        web.on("/boost", webBoost);
        web.on("/data", webData);
        web.begin();
        webStarted = true;
      }
      web.handleClient();

      Record r;
      if (xQueueReceive(recQueue, &r, 0) == pdTRUE) sendRecord(r);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ======================================================================
//  COMPENSATION LINK V2 (MUST match Node 2 struct and message version)
// ======================================================================
#define MSG_MAGIC     0x5349524AUL       // "SIRJ"
#define MSG_VERSION   2
#define NODE_ID_MAIN  1
#define COMP_FLAG_FAULT        0x02
#define COMP_FLAG_LOWBAT       0x04
#define COMP_FLAG_MEASURE_OK   0x08
#define COMP_FLAG_MANUAL       0x10
#define COMP_FLAG_SIM_FAULT    0x20  // Test mode only; not a measured physical failure

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t  version;
  uint8_t  nodeId;
  uint8_t  requested;    // model/manual target BEFORE battery protection (0..100)
  uint8_t  commanded;    // software PWM setting AFTER ramp + battery override (0..100)
  uint8_t  estimated;    // INA219-estimated effective PWM, or commanded if invalid
  uint8_t  flags;        // includes MEASURE_OK when estimate is based on valid current
  uint32_t seq;
} SirajMsg;

static uint8_t BROADCAST_ADDR[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
bool     compReady  = false;
uint32_t compLastMs = 0, compSeq = 0;

void compBegin() {
#if COMP_LINK_ENABLED
  if (WIFI_SSID[0] == 0) esp_wifi_set_channel(COMP_CHANNEL_NO_WIFI, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("# compensation link: ESP-NOW start FAILED"); return; }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
  peer.channel = 0;                    // current WiFi/router channel
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("# compensation link: add peer FAILED"); return; }
  compReady = true;
#endif
}

// Electrical-current estimate: assumes comparable LED light per unit of
// current. Not a lux measurement. Calibrate using known healthy lamp output.
// Returns false if INA data are missing/stale or command is too dim to judge.
bool compEstimate(uint8_t &estimated) {
  estimated = currentPct;
  if (lowBattery) { estimated = 0; return true; }
  if (!okIna || !inaHasValidSample || millis() - lastInaGoodMs > COMP_SENSOR_MAX_AGE_MS)
    return false;
  if (currentPct < COMP_MIN_MEASURE_PCT) return false;
  if (currentPct != desiredPct || millis() - lastLampPctChangeMs < COMP_RAMP_SETTLE_MS)
    return false; // not a measured lamp failure: commanded PWM is still ramping

  float vLamp = (LAMP_REG_V > 0) ? LAMP_REG_V : vbusFilt;
  float instA = (vLamp - LAMP_VF) / LAMP_R;
  float expectedFullMa = instA * 1000.0f * fullDutyNow() / PWM_MAX;
  float fullMa = (COMP_CALIBRATED_FULL_MA > 0) ? COMP_CALIBRATED_FULL_MA : expectedFullMa;
  if (!isfinite(fullMa) || fullMa < 30.0f || !isfinite(lampMaFilt)) return false;

  float fraction = lampMaFilt * 100.0f / fullMa;
  if (!isfinite(fraction)) return false;
  if (fraction < 0) fraction = 0;
  if (fraction > 100) fraction = 100;
  estimated = (uint8_t)lroundf(fraction);
  return true;
}

void compService(uint32_t now) {
  if (!compReady || now - compLastMs < 500) return;
  compLastMs = now;
  SirajMsg m = {};
  m.magic     = MSG_MAGIC;
  m.version   = MSG_VERSION;
  m.nodeId    = NODE_ID_MAIN;
  m.requested = desiredPct;
  m.commanded = lowBattery ? 0 : currentPct;
  // In F mode report the intentionally applied software PWM, not a made-up
  // INA219 measurement. Node 2 distinguishes it using SIM_FAULT flag.
  bool measurementOK = false;
  if (simulatedFaultActive) {
    m.estimated = m.commanded;
  } else {
    measurementOK = compEstimate(m.estimated);
  }
  m.flags = (lampFault ? COMP_FLAG_FAULT : 0)
          | (lowBattery ? COMP_FLAG_LOWBAT : 0)
          | (measurementOK ? COMP_FLAG_MEASURE_OK : 0)
          | (manualMode ? COMP_FLAG_MANUAL : 0)
          | (simulatedFaultActive ? COMP_FLAG_SIM_FAULT : 0);
  m.seq = ++compSeq;
  esp_err_t result = esp_now_send(BROADCAST_ADDR, (uint8_t *)&m, sizeof(m));
  if (result != ESP_OK && (m.seq % 10) == 1)
    Serial.printf("# COMP send failed err=%d\n", (int)result);
  if ((m.seq % 4) == 0) {
    Serial.printf("# COMP desired=%u cmd=%u %s=%u%s fault=%u lowbat=%u mode=%s\n",
                  m.requested, m.commanded,
                  simulatedFaultActive ? "SIM_PWM" : "INA_est", m.estimated,
                  simulatedFaultActive ? " [F TEST: -20 PWM points]" :
                  (measurementOK ? "" : " (NO_VALID_MEASUREMENT)"),
                  lampFault ? 1 : 0, lowBattery ? 1 : 0,
                  manualMode ? "manual" : "model/auto");
  }
}

// ======================================================================
//  RADAR CHECK  (printed every 10 s during the first minute, and on command "r")
// ======================================================================
void radarReport() {
  static uint32_t lastH = 0, lastVF = 0, lastVB = 0;
  Serial.printf("# radar check: human frames %lu | vehicle bytes %lu, vehicle frames %lu | OUT %d, OT1 %d\n",
                (unsigned long)(humFrames - lastH), (unsigned long)(vehBytes - lastVB),
                (unsigned long)(vehFrames - lastVF),
                digitalRead(PIN_HUMAN_OUT), digitalRead(PIN_VEH_OT1));
  lastH = humFrames; lastVF = vehFrames; lastVB = vehBytes;
}

// ======================================================================
//  SERIAL COMMANDS
// ======================================================================
void serialService() {
  if (!Serial.available()) return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (s.length() == 0) return;
  if (s.equalsIgnoreCase("f")) {
    simulatedFaultActive = true;
    Serial.printf("# TEST FAULT ON: physical PWM command reduced by %u percentage points; model stays active\n",
                  SIMULATED_FAULT_LOSS_PCT);
  } else if (s.equalsIgnoreCase("n")) {
    simulatedFaultActive = false;
    Serial.println("# TEST FAULT OFF: normal PWM restored; previous model/manual selection retained");
  } else if (s.equalsIgnoreCase("r")) {
    radarReport();
  } else if (s.equalsIgnoreCase("b")) {
    startBoost();
    Serial.printf("# boost +%u%% for %u s\n", BOOST_PCT, BOOST_SECONDS);
  } else if (isDigit(s[0])) {
    int v = s.toInt();
    if (v >= 0 && v <= 100) {
      basePct = (uint8_t)v; manualMode = true;
      Serial.printf("# manual desired PWM = %d%% (send m to resume model)\n", v);
    }
  } else if (s.equalsIgnoreCase("m")) {
    manualMode = false;
    Serial.println("# model/automatic mode resumed");
  } else {
    Serial.println("# commands: F = simulated -20 PWM-point fault; N = normal; 0..100 = manual; m = model; b = boost; r = radar");
  }
}

// ======================================================================
//  SETUP
// ======================================================================
bool i2cPing(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

void setup() {
  // lamp first, held off
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_LAMP, PWM_FREQ, PWM_RES);
#else
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_LAMP, PWM_CH);
#endif
  pwmWrite(0);

  modelQueue = xQueueCreate(1, sizeof(ModelDecision));
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(500);

  pinMode(PIN_HUMAN_OUT, INPUT);
  pinMode(PIN_VEH_OT1, INPUT);
  pinMode(PIN_TSL, INPUT);

  busA.begin(PIN_SDA_A, PIN_SCL_A, 100000);
  busB.begin(PIN_SDA_B, PIN_SCL_B, 100000);
  busA.setTimeOut(50);
  busB.setTimeOut(50);

#if AMBIENT_ON_OWN_PINS
  swHi(PIN_AMB_SDA); swHi(PIN_AMB_SCL); delay(2);
#endif
  okAmbient = ambPing();
  okIna     = i2cPing(busA, ADDR_INA219);
  okRtc     = i2cPing(busA, ADDR_DS3231);
  okRoad    = i2cPing(busB, ADDR_BH);

  if (okAmbient) ambCommand(0x01);
  if (okRoad)    bhCommand(busB, 0x01);
  // 32 V range, +-160 mV, 128-sample averaging on both channels (averages the PWM), continuous
  if (okIna)     inaWrite16(0x00, 0x37FF);
  if (okRtc)     rtcSetFromCompileTimeIfNeeded();

#if USE_DS18B20
  ds18.begin();
  ds18.setWaitForConversion(false);
  ds18.requestTemperatures();
#endif

  Serial1.setRxBufferSize(1024);
  Serial1.begin(256000, SERIAL_8N1, PIN_R1_RX, PIN_R1_TX);    // LD2410C
  vehBegin();                                                   // LD2451

  recQueue  = xQueueCreate(8, sizeof(Record));
  lastMutex = xSemaphoreCreateMutex();

  WiFi.mode(WIFI_STA);
  compBegin();
  WiFi.setAutoReconnect(true);
  xTaskCreatePinnedToCore(netTask, "net", 8192, NULL, 1, NULL, 0);

  Serial.println();
  Serial.println("# ================= SIRAJ NODE 1 - data collection =================");
  Serial.printf ("# ambient BH1750: %s   road BH1750: %s   INA219: %s   DS3231: %s\n",
                 okAmbient ? "OK" : "MISSING", okRoad ? "OK" : "MISSING",
                 okIna ? "OK" : "MISSING", okRtc ? "OK" : "MISSING");
  if (WIFI_SSID[0] == 0) Serial.println("# WiFi name is empty: readings go to the serial port only");
  if (LOW_BATTERY_V > 0) Serial.printf("# battery protection: lamp switches off below %.1f V\n", LOW_BATTERY_V);
  else                   Serial.println("# battery protection: OFF (12 V supply setting)");
  if (LAMP_REG_V > 0)    Serial.printf("# lamp supply: own step-down at %.1f V (lamp_ma is calculated from the battery side)\n", LAMP_REG_V);
  else                   Serial.println("# lamp supply: direct from the battery, pulses limited by voltage");
  if (compReady) {
    uint8_t ch = 0; wifi_second_chan_t sc;
    esp_wifi_get_channel(&ch, &sc);
    Serial.printf("# compensation link: broadcasting lamp level to node 2 (radio channel %u)\n", ch);
  }
  Serial.println("# commands: F = simulated -20 PWM-point fault; N = normal; 0..100 = manual; m = model; b = boost; r = radar");
  Serial.println("# compensation V2: desired, commanded, estimated via INA219 (not optical lux)");
  Serial.println("seq,rtc_time,uptime_s,lux_ambient,lux_road,sky_hz,human,vehicle,motion,human_state,"
                 "human_dist_cm,veh_count,veh_dist_m,veh_speed_kmh,veh_approaching,lamp_pct,lamp_duty_pct,"
                 "boost,battery_v,lamp_ma,lamp_fault,temp_c,status");

  uint32_t now = millis();
  tslLastMs = now; recLastMs = now;
}

// ======================================================================
//  LOOP
// ======================================================================
void loop() {
  uint32_t now = millis();

  radarService(now);
  inaService(now);
  lampService(now);
  bhService(now);
  tslService(now);
  slowService(now);
  compService(now);
  serialService();

  if (radarRepMs == 0 && now >= 10000) { radarRepMs = now; radarReport(); }   // once, 10 s after start

  if (now - recLastMs >= SEND_INTERVAL_MS) {
    recLastMs = now;
    makeRecord(now);
  }

  delay(2);
}
