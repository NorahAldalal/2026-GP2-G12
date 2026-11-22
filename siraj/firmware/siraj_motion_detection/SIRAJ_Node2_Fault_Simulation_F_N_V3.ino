/* ======================================================================
   SIRAJ  —  NODE 2   (compensation node)        FINAL SEALED FIRMWARE
   ----------------------------------------------------------------------
   Hardware in this box:
       ESP32-S3-N16R8  +  LM2596 buck (5V)  +  IRLZ44N  +  12V 5W LED lamp
       no sensors

   Wiring:
       GPIO 11 --[100R]-- GATE
       GATE  --[10k]-- SOURCE
       SOURCE -- GND
       DRAIN  -- lamp negative
       lamp positive -- +12V

   Behaviour (protocol V2, must flash BOTH nodes):
       Node 1 sends requested PWM and estimated delivered PWM based on INA219.
       This lamp: 40 + max(0, requested - estimated), capped at 100.
       Ex: requested=70, INA estimate=40 -> node2=70.
       Full battery shutdown -> estimated=0, still use requested setpoint.
       When Node 1 receives F, it uses a separately flagged reduced PWM
       command for testing (-20 percentage points); N restores normal PWM.
       This simulation is NOT a hardware or sensor fault measurement.
       On missing/stale INA data it assumes no physical shortfall (requested)
       and marks FALLBACK; it CANNOT detect partial faults without INA data.
       If a previously connected Node 1 becomes unreachable for 10 seconds,
       Node 2 compensates conservatively using last requested level.

   Radio:
       ESP-NOW broadcast. No MAC addresses, no router needed here.
       Node 1 uses channel 1 when it has no router, or its router's channel
       when it is on WiFi. When nothing is heard, this node steps through
       channels 1..13 until it finds node 1, then stays there.

   Updating after the box is glued shut:
       ONLY if OTA_ENABLED=1, for 5 minutes after every power-up this node
       opens a WiFi access point for over-the-air sketch upload.

           SSID      Siraj-Node2
           password  siraj1234
           port      Arduino IDE -> Tools -> Port -> siraj-node2 (network)

       After 5 minutes the AP shuts down to save power and normal
       operation begins. Power-cycle the box to get another window.
       A node 1 broadcast with flag bit 0 set also reopens the window.
   ====================================================================== */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoOTA.h>

// ----------------------------------------------------------------------
// hardware
// ----------------------------------------------------------------------
#define LAMP_PIN        11
#define PWM_FREQ        1000      // Hz - above flicker, clean on camera
#define PWM_RES         8         // bits -> duty 0..255
#define PWM_CH          0         // core 2.x only

// What "100%" means for THIS lamp:
//   255  -> node runs on a 12V supply
//   110  -> node runs on a 16V battery (keeps the 12V lamp near 5W)
#define LAMP_FULL_DUTY  255

// 1 = open the 5-minute OTA update hotspot at every power-up
// 0 = no hotspot (upload by USB only)
#define OTA_ENABLED     0    // USB upload/testing: no 5-min channel-1 startup delay
// Set to 1 again before sealing the enclosure if OTA-on-boot is required.

// prints a boot step and waits until it is sent, so if the board
// crashes the last printed step shows where
#define BOOT_STEP(txt)  do { Serial.println("# boot: " txt); Serial.flush(); delay(50); } while (0)

// ----------------------------------------------------------------------
// behaviour  (the numbers you may want to change)
// ----------------------------------------------------------------------
const uint8_t  BASELINE_PCT    = 10;     // resting level of THIS lamp (node 2)
// Node 1 target is sent dynamically by the model (not a fixed 70%).
const uint8_t  FLOOR_PCT       = 10;     // node 2 never goes below this
const uint8_t  CEILING_PCT     = 100;    // ...nor above this
const uint32_t LINK_TIMEOUT_MS = 10000;  // prolonged silence -> link-loss behavior
#define COMPENSATE_ON_LINK_LOSS 1     // after first successful link only
const uint16_t RAMP_STEP_MS    = 12;     // 1% per this many ms

// ----------------------------------------------------------------------
// radio
// ----------------------------------------------------------------------
const uint8_t  ESPNOW_CHANNEL  = 1;
#define MSG_MAGIC     0x5349524AUL       // "SIRJ"
#define MSG_VERSION   2
#define NODE_ID_MAIN  1
#define FLAG_OPEN_OTA 0x01
#define FLAG_FAULT    0x02               // node 1 lamp is faulty
#define FLAG_LOWBAT       0x04
#define FLAG_MEASURE_OK   0x08
#define FLAG_MANUAL       0x10
#define FLAG_SIM_FAULT    0x20  // Node 1 F/N simulation flag (not INA measurement)

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t  version;
  uint8_t  nodeId;
  uint8_t  requested;    // dynamically sent model/manual desired percentage
  uint8_t  commanded;    // Node 1 applied software PWM
  uint8_t  estimated;    // Node 1 current-based PWM estimate
  uint8_t  flags;
  uint32_t seq;
} SirajMsg;

static uint8_t BROADCAST[6] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

// ----------------------------------------------------------------------
// OTA access point
// ----------------------------------------------------------------------
const char*    OTA_AP_SSID   = "Siraj-Node2";
const char*    OTA_AP_PASS   = "siraj1234";          // must be >= 8 chars
const char*    OTA_HOSTNAME  = "siraj-node2";
const uint32_t OTA_WINDOW_MS = 5UL * 60UL * 1000UL;  // 5 minutes

// ----------------------------------------------------------------------
// state
// ----------------------------------------------------------------------
// ESP-NOW callbacks run in a WiFi task, not in loop(): copy state atomically.
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t  rxRequested = 0;
uint8_t  rxCommanded = 0;
uint8_t  rxEstimated = 0;
uint8_t  rxFlags     = 0;
uint32_t rxLastMs    = 0;
uint32_t rxCount     = 0;
uint32_t rxSeq       = 0;
bool     rxOtaRequest = false;

uint8_t  currentPct = 0;      // what the lamp is actually at now
uint8_t  targetPct  = BASELINE_PCT;
uint32_t lastRampMs = 0;
uint32_t lastLogMs  = 0;
uint32_t lastCompEventMs = 0;
uint8_t  lastReportedTarget = 255;

uint8_t  curChannel = ESPNOW_CHANNEL;   // channel the radio listens on now
uint32_t lastHopMs  = 0;
const uint16_t HOP_DWELL_MS = 700;     // node 1 sends every 500 ms, so 700 ms catches at least one

bool     otaActive    = false;
uint32_t otaStartedMs = 0;

// ======================================================================
// lamp
// ======================================================================
static inline uint8_t pctToDuty(uint8_t pct) {
  if (pct > 100) pct = 100;
  return (uint8_t)(((uint16_t)pct * LAMP_FULL_DUTY + 50U) / 100U);
}

void lampWritePct(uint8_t pct) {
  uint8_t duty = pctToDuty(pct);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LAMP_PIN, duty);
#else
  ledcWrite(PWM_CH, duty);
#endif
  currentPct = pct;
}

void lampBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LAMP_PIN, PWM_FREQ, PWM_RES);
#else
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(LAMP_PIN, PWM_CH);
#endif
  lampWritePct(0);
}

// move one step toward target - called often, never blocks
void lampService() {
  if (currentPct == targetPct) return;
  uint32_t now = millis();
  if (now - lastRampMs < RAMP_STEP_MS) return;
  lastRampMs = now;
  lampWritePct(currentPct < targetPct ? currentPct + 1 : currentPct - 1);
}

// ======================================================================
// compensation rule
// ======================================================================
uint8_t compensationFor(uint8_t requested, uint8_t actualEstimate) {
  int16_t gap = (int16_t)requested - (int16_t)actualEstimate;
  if (gap < 0) gap = 0;
  int16_t v = (int16_t)BASELINE_PCT + gap;
  if (v < FLOOR_PCT)   v = FLOOR_PCT;
  if (v > CEILING_PCT) v = CEILING_PCT;
  return (uint8_t)v;
}

// ======================================================================
// ESP-NOW
// ======================================================================
void handlePacket(const uint8_t* data, int len) {
  if (len != (int)sizeof(SirajMsg)) return;

  SirajMsg m;
  memcpy(&m, data, sizeof(m));

  if (m.magic   != MSG_MAGIC)    return;   // not ours
  if (m.version != MSG_VERSION)  return;
  if (m.nodeId  != NODE_ID_MAIN) return;   // only listen to node 1
  if (m.requested > 100 || m.commanded > 100 || m.estimated > 100) return;

  portENTER_CRITICAL(&rxMux);
  rxRequested = m.requested;
  rxCommanded = m.commanded;
  rxEstimated = m.estimated;
  rxFlags     = m.flags;
  rxSeq       = m.seq;
  rxLastMs    = millis();
  rxCount++;
  if (m.flags & FLAG_OPEN_OTA) rxOtaRequest = true;
  portEXIT_CRITICAL(&rxMux);
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  (void)info;
  handlePacket(data, len);
}
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
  (void)mac;
  handlePacket(data, len);
}
#endif

bool espNowBegin() {
  esp_now_deinit();                       // harmless if not running

  if (esp_now_init() != ESP_OK) {
    Serial.println("[radio] esp_now_init FAILED");
    return false;
  }
  esp_now_register_recv_cb(onEspNowRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  if (!esp_now_is_peer_exist(BROADCAST)) {
    if (esp_now_add_peer(&peer) != ESP_OK) {
      Serial.println("[radio] add_peer FAILED");
      return false;
    }
  }
  Serial.printf("[radio] ESP-NOW listening on channel %u\n", ESPNOW_CHANNEL);
  return true;
}

// ======================================================================
// OTA window
// ======================================================================
void otaBegin() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(OTA_AP_SSID, OTA_AP_PASS, ESPNOW_CHANNEL);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  ArduinoOTA.setHostname(OTA_HOSTNAME);

  ArduinoOTA.onStart([]() {
    lampWritePct(BASELINE_PCT);          // predictable state during flashing
    Serial.println("[ota] upload starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[ota] done, rebooting");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    Serial.printf("\r[ota] %u%%", (done * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("\n[ota] error %u\n", e);
  });

  ArduinoOTA.begin();

  otaActive    = true;
  otaStartedMs = millis();

  Serial.println();
  Serial.println("==========================================");
  Serial.printf ("  OTA WINDOW OPEN for %lu seconds\n", OTA_WINDOW_MS / 1000UL);
  Serial.printf ("  SSID     : %s\n", OTA_AP_SSID);
  Serial.printf ("  password : %s\n", OTA_AP_PASS);
  Serial.printf ("  IP       : %s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("==========================================");
  Serial.println();

  espNowBegin();     // ESP-NOW keeps working during the window
}

void otaEnd() {
  if (!otaActive) return;
  ArduinoOTA.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  curChannel = ESPNOW_CHANNEL;
  otaActive = false;
  Serial.println("[ota] window closed - normal operation");
  espNowBegin();
}

// ======================================================================
// setup
// ======================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  BOOT_STEP("1 lamp PWM on GPIO 11");
  lampBegin();                       // lamp off first, before anything else

  Serial.println();
  Serial.println("======================================");
  Serial.println("  SIRAJ  -  NODE 2  (compensation)");
  Serial.printf ("  rest %u%%   max %u%%   protocol v%u\n",
                 BASELINE_PCT, CEILING_PCT, MSG_VERSION);
  Serial.println("  rule: node2 = 40 + max(0, requested - measured or simulated PWM)");
  Serial.println("======================================");

  BOOT_STEP("2 radio on");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();                 // ESP-NOW needs the interface up, not joined
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

#if OTA_ENABLED
  BOOT_STEP("3 OTA hotspot");
  otaBegin();                        // 5 minute update window on every boot
#else
  BOOT_STEP("3 OTA off - ESP-NOW only");
  espNowBegin();
#endif
  BOOT_STEP("4 setup done");

  targetPct = BASELINE_PCT;          // come up at the resting level
  Serial.println("# COMP_CSV header: ms,seq,link,requested,commanded,estimated,measurement_ok,flags,node2_target,node2_applied");
  Serial.printf("[lamp] rising to baseline %u%%\n", BASELINE_PCT);
}

// ======================================================================
// loop
// ======================================================================
void loop() {
  // ---- OTA window management -----------------------------------------
  bool otaRequested = false;
  portENTER_CRITICAL(&rxMux);
  otaRequested = rxOtaRequest;
  rxOtaRequest = false;
  portEXIT_CRITICAL(&rxMux);
  if (otaActive) {
    ArduinoOTA.handle();
    if (millis() - otaStartedMs > OTA_WINDOW_MS) otaEnd();
  } else if (otaRequested) {
#if OTA_ENABLED
    Serial.println("[ota] reopen requested by node 1");
    otaBegin();
#endif
  }

  // Snapshot a COMPLETE packet, never mixed fields from different packets.
  uint32_t now = millis();
  uint8_t requested, commanded, estimated, flags;
  uint32_t lastRx, count, seq;
  portENTER_CRITICAL(&rxMux);
  requested = rxRequested;
  commanded = rxCommanded;
  estimated = rxEstimated;
  flags = rxFlags;
  lastRx = rxLastMs;
  count = rxCount;
  seq = rxSeq;
  portEXIT_CRITICAL(&rxMux);

  bool everLinked = count > 0;
  bool linkUp = everLinked && (now - lastRx < LINK_TIMEOUT_MS);
  bool measurementOK = (flags & FLAG_MEASURE_OK) != 0;
  bool simulatedFault = (flags & FLAG_SIM_FAULT) != 0;
  // Node 1 F mode sends its intentionally reduced *software* PWM.
  // It is valid for exercising the compensation path, but it is NOT
  // an INA219 measurement and must not be presented as physical fault detection.
  uint8_t actualEstimate = (flags & FLAG_LOWBAT) ? 0
                            : simulatedFault ? estimated
                            : measurementOK ? estimated : requested;
  // Otherwise, no INA data: do not mistake PWM ramping for physical failure.

  if (linkUp) {
    targetPct = compensationFor(requested, actualEstimate);
  } else if (everLinked && COMPENSATE_ON_LINK_LOSS) {
    // Loss of radio is not proof the lamp failed, but reserve some coverage.
    targetPct = compensationFor(requested, 0);
  } else {
    targetPct = BASELINE_PCT;  // don't boost until Node 1 has ever been heard
  }

  // Find Node 1's WiFi channel. This requires OTA hotspot to be closed.
  if (!otaActive && !linkUp && now - lastHopMs >= HOP_DWELL_MS) {
    lastHopMs  = now;
    curChannel = (curChannel % 13) + 1;
    esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
  }

  lampService();

  // CSV-ready diagnostic lines: do not claim estimated==optical lux.
  if (targetPct != lastReportedTarget || now - lastCompEventMs >= 1000) {
    lastReportedTarget = targetPct;
    lastCompEventMs = now;
    Serial.printf("# COMP_CSV,%lu,%lu,%u,%u,%u,%u,%u,%u,%u,%u\n",
                  (unsigned long)now, (unsigned long)seq, linkUp ? 1 : 0,
                  requested, commanded, actualEstimate, measurementOK ? 1 : 0,
                  flags, targetPct, currentPct);
  }

  if (now - lastLogMs >= 2000) {
    lastLogMs = now;
    Serial.printf("link %-4s | ch %2u | req %3u%% | cmd %3u%% | %s %3u%%%s | node2 %3u%%/%3u%% | rx %lu%s%s\n",
                  linkUp ? "UP" : "DOWN", curChannel,
                  requested, commanded,
                  simulatedFault ? "SIM PWM" : "INA estimate", actualEstimate,
                  simulatedFault ? " [F TEST - NOT SENSOR]" :
                  (measurementOK ? "" : " [FALLBACK: NO VALID INA]"),
                  targetPct, currentPct, (unsigned long)count,
                  (flags & FLAG_LOWBAT) ? " [LOW BATTERY]" : "",
                  otaActive ? " [OTA]" : "");
  }
  delay(2);
}
