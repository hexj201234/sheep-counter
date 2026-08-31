#pragma once
#include <stdint.h>

// 数羊会话与分道光电判定核心逻辑。
// 该文件不依赖 Arduino，可在 PC 上直接编译做单元测试（见 test/test_session）。

namespace sheep {

// 当前正在看哪一头：出门 / 回圈。
enum class Mode : uint8_t { Out = 0, Back = 1 };

// 会话状态：待开始 / 进行中 / 已结束。
enum class RunState : uint8_t { Idle = 0, Running = 1, Ended = 2 };

// 会话状态机：门口盒子里的“计数真相”。
// 出门与回圈各有独立计数；只有进行中才接受光电自动 +1。
class Session {
 public:
  Session();

  Mode mode() const { return mode_; }
  RunState state() const { return state_; }

  // 当前选中一头的计数。
  int count() const;
  // 指定一头的计数。
  int count(Mode m) const;
  // 未归 = 出门 - 回圈（可为负，仅参考）。
  int notReturned() const { return out_ - back_; }

  // 切换出门/回圈；进行中禁止切换，返回是否成功。
  bool selectMode(Mode m);
  // 当前一头进入进行中；已结束后再按 = 同一会话继续累加。
  bool start();
  // 停止自动加并冻结当前一头，返回是否发生状态变化（可据此排队上云）。
  bool end();
  // 光电检测到一只通过；仅进行中才计入，返回是否真正 +1。
  bool onSheepDetected();
  // 手动 +1 当前一头（进行中或已结束均可）。
  bool increment();
  // 手动 -1 当前一头；不出现负数，返回是否真正 -1。
  bool decrement();
  // 清零当前一头（长按结束确认后调用），另一头不变。
  void resetCurrent();

 private:
  int& active();
  const int& active() const;

  Mode mode_;
  RunState state_;
  int out_;
  int back_;
};

// 单道对射光电：A、B 两束，判定“同一只羊 A 先挡、B 后挡且躯干够长”。
// 蹄子二次沿用闭锁时间过滤；探头又退（只挡 A）不计。
class LaneCounter {
 public:
  // minMs/maxMs：A→B 的合理时间窗；lockoutMs：计一只后闭锁时长。
  LaneCounter(uint32_t minMs = 80, uint32_t maxMs = 1500, uint32_t lockoutMs = 300);

  // 输入两束当前遮挡状态（true=被挡）与当前毫秒时间。
  // 恰好在判定到一只完整通过时返回 true（每只仅一次）。
  bool update(bool aBlocked, bool bBlocked, uint32_t nowMs);

  void reset();

 private:
  enum class St : uint8_t { WaitA, WaitB, Lockout };
  St st_;
  uint32_t tA_;
  uint32_t lockStart_;
  uint32_t minMs_;
  uint32_t maxMs_;
  uint32_t lockoutMs_;
};

}  // namespace sheep
