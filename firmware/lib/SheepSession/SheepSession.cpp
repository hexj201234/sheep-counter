#include "SheepSession.h"

namespace sheep {

Session::Session()
    : mode_(Mode::Out), state_(RunState::Idle), out_(0), back_(0) {}

int& Session::active() { return mode_ == Mode::Out ? out_ : back_; }
const int& Session::active() const { return mode_ == Mode::Out ? out_ : back_; }

int Session::count() const { return active(); }

int Session::count(Mode m) const { return m == Mode::Out ? out_ : back_; }

bool Session::selectMode(Mode m) {
  if (state_ == RunState::Running) return false;  // 进行中禁止切换
  mode_ = m;
  state_ = RunState::Idle;
  return true;
}

bool Session::start() {
  state_ = RunState::Running;  // 已结束后再按=继续累加，不清零
  return true;
}

bool Session::end() {
  if (state_ == RunState::Ended) return false;
  state_ = RunState::Ended;
  return true;
}

bool Session::onSheepDetected() {
  if (state_ != RunState::Running) return false;
  active() += 1;
  return true;
}

bool Session::increment() {
  if (state_ == RunState::Idle) return false;
  active() += 1;
  return true;
}

bool Session::decrement() {
  if (active() <= 0) return false;  // 不出现负数
  active() -= 1;
  return true;
}

void Session::resetCurrent() { active() = 0; }

LaneCounter::LaneCounter(uint32_t minMs, uint32_t maxMs, uint32_t lockoutMs)
    : st_(St::WaitA),
      tA_(0),
      lockStart_(0),
      minMs_(minMs),
      maxMs_(maxMs),
      lockoutMs_(lockoutMs) {}

void LaneCounter::reset() {
  st_ = St::WaitA;
  tA_ = 0;
  lockStart_ = 0;
}

bool LaneCounter::update(bool a, bool b, uint32_t now) {
  if (st_ == St::Lockout) {
    if (now - lockStart_ < lockoutMs_) return false;
    st_ = St::WaitA;
  }

  if (st_ == St::WaitA) {
    if (a && !b) {  // A 先挡、B 未挡
      tA_ = now;
      st_ = St::WaitB;
    }
    return false;
  }

  // St::WaitB
  if (b) {
    uint32_t dt = now - tA_;
    if (a && dt >= minMs_ && dt <= maxMs_) {  // 躯干够长（重叠）且时间合理
      st_ = St::Lockout;
      lockStart_ = now;
      return true;
    }
    if (dt > maxMs_) st_ = St::WaitA;  // 太慢，作废
    return false;
  }

  if (!a) st_ = St::WaitA;  // A 在 B 之前退掉=探头又退，不计
  return false;
}

}  // namespace sheep
