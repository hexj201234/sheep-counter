#pragma once
#include <stdint.h>

// 门口主机与手持器之间的 ESP-NOW 报文（点对点，预共享密钥）。
// 纯 POD 结构，两端共用；不依赖 Arduino。

namespace sheep {

// 手持 -> 门口 的命令码。
enum class Cmd : uint8_t {
  SelectOut = 1,  // 切到出门
  SelectBack = 2, // 切到回圈
  Start = 3,      // 开始
  End = 4,        // 结束
  Inc = 5,        // +1
  Dec = 6,        // -1
  Reset = 7,      // 清零当前一头（长按确认后）
};

// 手持 -> 门口。
struct CommandMsg {
  uint8_t magic;  // 协议标识, 固定 0xA5
  uint8_t cmd;    // Cmd
};

// 门口 -> 手持，约 10Hz 广播，手持镜像显示。
struct StatusMsg {
  uint8_t magic;   // 0x5A
  uint8_t mode;    // Mode: 0=出门 1=回圈
  uint8_t state;   // RunState: 0=待开始 1=进行中 2=已结束
  uint8_t battery; // 门口占位, 0-100
  int16_t outCount;
  int16_t backCount;
};

static const uint8_t kCmdMagic = 0xA5;
static const uint8_t kStatusMagic = 0x5A;

}  // namespace sheep
