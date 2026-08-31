// 手持器固件（ESP32-C3 SuperMini）
//   - 0.96" SSD1306 OLED（I2C）镜像门口数字
//   - 六键：出门 / 回圈 / 开始 / 结束 / +1 / -1
//   - ESP-NOW 主机：按键即发命令，接收门口状态镜像显示
//   - 长按“结束” ~2s：清零当前一头（屏上确认）
#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>

#include "Protocol.h"

using namespace sheep;

// ---- 引脚（ESP32-C3 SuperMini）----
static const uint8_t kPinSda = 8;
static const uint8_t kPinScl = 9;
static const uint8_t kBtnOut = 0;
static const uint8_t kBtnBack = 1;
static const uint8_t kBtnStart = 2;
static const uint8_t kBtnEnd = 3;
static const uint8_t kBtnInc = 4;
static const uint8_t kBtnDec = 5;

// 门口 MAC（首次配对写入；此处占位为广播）。
static uint8_t g_gateMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static StatusMsg g_mirror{};
static bool g_haveStatus = false;
static uint32_t g_endPressStart = 0;
static bool g_endHandled = false;

static void sendCmd(Cmd c) {
  CommandMsg m{kCmdMagic, (uint8_t)c};
  esp_now_send(g_gateMac, (uint8_t*)&m, sizeof(m));
}

static void onStatus(const uint8_t*, const uint8_t* data, int len) {
  if (len < (int)sizeof(StatusMsg)) return;
  const StatusMsg* s = (const StatusMsg*)data;
  if (s->magic != kStatusMagic) return;
  g_mirror = *s;
  g_haveStatus = true;
}

static const char* modeText(uint8_t m) { return m == 0 ? "chu men" : "hui quan"; }
static const char* stateText(uint8_t s) {
  return s == 0 ? "dai kaishi" : (s == 1 ? "jinxing zhong" : "yi jieshu");
}

static void draw(bool confirmReset) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  if (!g_haveStatus) {
    u8g2.drawStr(0, 12, "connecting gate...");
    u8g2.sendBuffer();
    return;
  }
  int cur = g_mirror.mode == 0 ? g_mirror.outCount : g_mirror.backCount;
  u8g2.drawStr(0, 10, modeText(g_mirror.mode));
  u8g2.drawStr(70, 10, stateText(g_mirror.state));
  char big[8];
  snprintf(big, sizeof(big), "%d", cur);
  u8g2.setFont(u8g2_font_logisoso28_tn);
  u8g2.drawStr(0, 48, big);
  u8g2.setFont(u8g2_font_6x12_tf);
  char foot[24];
  snprintf(foot, sizeof(foot), "batt %d%%", g_mirror.battery);
  u8g2.drawStr(0, 62, foot);
  if (confirmReset) u8g2.drawStr(64, 62, "reset?");
  u8g2.sendBuffer();
}

static bool pressed(uint8_t pin) { return digitalRead(pin) == LOW; }

void setup() {
  Serial.begin(115200);
  const uint8_t btns[] = {kBtnOut, kBtnBack, kBtnStart, kBtnEnd, kBtnInc, kBtnDec};
  for (uint8_t b : btns) pinMode(b, INPUT_PULLUP);

  Wire.begin(kPinSda, kPinScl);
  u8g2.begin();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
  }
  esp_now_register_recv_cb(onStatus);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, g_gateMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  Serial.println("handheld ready");
}

void loop() {
  static uint32_t lastEdge[6] = {0};
  const uint8_t btns[] = {kBtnOut, kBtnBack, kBtnStart, kBtnEnd, kBtnInc, kBtnDec};
  const Cmd cmds[] = {Cmd::SelectOut, Cmd::SelectBack, Cmd::Start,
                      Cmd::End, Cmd::Inc, Cmd::Dec};
  uint32_t now = millis();

  for (uint8_t i = 0; i < 6; i++) {
    if (pressed(btns[i]) && now - lastEdge[i] > 200) {  // 简单去抖
      lastEdge[i] = now;
      if (btns[i] == kBtnEnd) continue;  // 结束键单独处理长按
      sendCmd(cmds[i]);
    }
  }

  // 结束键：短按=结束；长按 ~2s=清零当前一头
  if (pressed(kBtnEnd)) {
    if (g_endPressStart == 0) g_endPressStart = now;
    if (!g_endHandled && now - g_endPressStart >= 2000) {
      sendCmd(Cmd::Reset);
      g_endHandled = true;
    }
  } else {
    if (g_endPressStart != 0 && !g_endHandled) sendCmd(Cmd::End);
    g_endPressStart = 0;
    g_endHandled = false;
  }

  draw(g_endPressStart != 0 && !g_endHandled && now - g_endPressStart > 600);
  delay(20);
}
