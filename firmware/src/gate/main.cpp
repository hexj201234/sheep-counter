// 门口主机固件（ESP32-S3-DevKitC-1 N16R8）
//   - 4 道 × 2 束对射光电（经光耦进 GPIO），四道汇总为当前会话数
//   - TM1637 四位数码管显示当前选中一头的数字
//   - ESP-NOW 从机：接手持命令、约 10Hz 回传状态
//   - 断电保持：会话写入 NVS，来电恢复
//   - WiFi Station：结束时把当日出门/回圈排队上云（有网才发）
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "Protocol.h"
#include "SheepSession.h"

using namespace sheep;

// ---- 引脚（按 BOM 走线，可现场调整）----
static const uint8_t kLaneBeamA[4] = {4, 6, 8, 10};   // 各道 A 束
static const uint8_t kLaneBeamB[4] = {5, 7, 9, 11};   // 各道 B 束
static const uint8_t kPinTM1637Clk = 12;
static const uint8_t kPinTM1637Dio = 13;
static const uint8_t kPinLedOut = 15;   // 出门指示
static const uint8_t kPinLedBack = 16;  // 回圈指示
static const uint8_t kPinLedRun = 17;   // 进行中指示

static Session g_session;
static LaneCounter g_lanes[4];
static Preferences g_prefs;
static uint32_t g_lastBroadcastMs = 0;
static uint8_t g_broadcastPeer[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ---------- TM1637 极简位驱动（CLK/DIO 两线）----------
static const uint8_t kSeg[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

static void tmStart() {
  digitalWrite(kPinTM1637Dio, HIGH);
  digitalWrite(kPinTM1637Clk, HIGH);
  digitalWrite(kPinTM1637Dio, LOW);
  digitalWrite(kPinTM1637Clk, LOW);
}
static void tmStop() {
  digitalWrite(kPinTM1637Clk, LOW);
  digitalWrite(kPinTM1637Dio, LOW);
  digitalWrite(kPinTM1637Clk, HIGH);
  digitalWrite(kPinTM1637Dio, HIGH);
}
static void tmWrite(uint8_t b) {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(kPinTM1637Clk, LOW);
    digitalWrite(kPinTM1637Dio, (b & 0x01) ? HIGH : LOW);
    delayMicroseconds(3);
    digitalWrite(kPinTM1637Clk, HIGH);
    delayMicroseconds(3);
    b >>= 1;
  }
  // ACK
  digitalWrite(kPinTM1637Clk, LOW);
  pinMode(kPinTM1637Dio, INPUT_PULLUP);
  digitalWrite(kPinTM1637Clk, HIGH);
  pinMode(kPinTM1637Dio, OUTPUT);
}

static void tmShow(int value, bool colon) {
  if (value < 0) value = 0;
  if (value > 9999) value = 9999;
  uint8_t d[4] = {kSeg[(value / 1000) % 10], kSeg[(value / 100) % 10],
                  kSeg[(value / 10) % 10], kSeg[value % 10]};
  if (colon) d[1] |= 0x80;  // 冒号当“进行中”
  tmStart();
  tmWrite(0x40);  // 自动地址
  tmStop();
  tmStart();
  tmWrite(0xC0);  // 地址 0
  for (uint8_t i = 0; i < 4; i++) tmWrite(d[i]);
  tmStop();
  tmStart();
  tmWrite(0x88 | 0x07);  // 开显示, 最亮
  tmStop();
}

// ---------- 持久化 ----------
static void persist() {
  g_prefs.putInt("out", g_session.count(Mode::Out));
  g_prefs.putInt("back", g_session.count(Mode::Back));
  g_prefs.putUChar("mode", (uint8_t)g_session.mode());
  g_prefs.putUChar("state", (uint8_t)g_session.state());
}

static void restore() {
  int out = g_prefs.getInt("out", 0);
  int back = g_prefs.getInt("back", 0);
  Mode m = (Mode)g_prefs.getUChar("mode", 0);
  g_session.selectMode(m);
  for (int i = 0; i < out; i++) {
    g_session.start();
    g_session.onSheepDetected();
  }
  g_session.selectMode(Mode::Back);
  for (int i = 0; i < back; i++) {
    g_session.start();
    g_session.onSheepDetected();
  }
  g_session.selectMode(m);
  g_session.end();
}

// ---------- ESP-NOW ----------
static void broadcastStatus() {
  StatusMsg s{};
  s.magic = kStatusMagic;
  s.mode = (uint8_t)g_session.mode();
  s.state = (uint8_t)g_session.state();
  s.battery = 100;
  s.outCount = (int16_t)g_session.count(Mode::Out);
  s.backCount = (int16_t)g_session.count(Mode::Back);
  esp_now_send(g_broadcastPeer, (uint8_t*)&s, sizeof(s));
}

static void onCmd(const uint8_t*, const uint8_t* data, int len) {
  if (len < (int)sizeof(CommandMsg)) return;
  const CommandMsg* c = (const CommandMsg*)data;
  if (c->magic != kCmdMagic) return;
  switch ((Cmd)c->cmd) {
    case Cmd::SelectOut: g_session.selectMode(Mode::Out); break;
    case Cmd::SelectBack: g_session.selectMode(Mode::Back); break;
    case Cmd::Start: g_session.start(); break;
    case Cmd::End: g_session.end(); break;
    case Cmd::Inc: g_session.increment(); break;
    case Cmd::Dec: g_session.decrement(); break;
    case Cmd::Reset: g_session.resetCurrent(); break;
  }
  persist();
  broadcastStatus();
}

static void setupEspNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onCmd);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, g_broadcastPeer, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(kLaneBeamA[i], INPUT_PULLUP);
    pinMode(kLaneBeamB[i], INPUT_PULLUP);
  }
  pinMode(kPinTM1637Clk, OUTPUT);
  pinMode(kPinTM1637Dio, OUTPUT);
  pinMode(kPinLedOut, OUTPUT);
  pinMode(kPinLedBack, OUTPUT);
  pinMode(kPinLedRun, OUTPUT);

  g_prefs.begin("sheep", false);
  restore();
  setupEspNow();
  tmShow(g_session.count(), g_session.state() == RunState::Running);
  Serial.println("gate ready");
}

void loop() {
  uint32_t now = millis();
  bool changed = false;

  // 四道独立判定，任一道计到即当前会话 +1（进行中才生效）
  for (uint8_t i = 0; i < 4; i++) {
    bool a = digitalRead(kLaneBeamA[i]) == LOW;  // 光耦拉低=被挡
    bool b = digitalRead(kLaneBeamB[i]) == LOW;
    if (g_lanes[i].update(a, b, now)) {
      if (g_session.onSheepDetected()) changed = true;
    }
  }

  if (changed) persist();

  digitalWrite(kPinLedOut, g_session.mode() == Mode::Out);
  digitalWrite(kPinLedBack, g_session.mode() == Mode::Back);
  digitalWrite(kPinLedRun, g_session.state() == RunState::Running);
  tmShow(g_session.count(), g_session.state() == RunState::Running);

  if (now - g_lastBroadcastMs >= 100) {  // ~10Hz
    g_lastBroadcastMs = now;
    broadcastStatus();
  }
}
