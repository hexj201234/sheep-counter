#include <unity.h>

#include "SheepSession.h"

using namespace sheep;

void setUp() {}
void tearDown() {}

// 待开始时光电不计数，开始后才累加。
void test_counts_only_when_running() {
  Session s;
  TEST_ASSERT_FALSE(s.onSheepDetected());
  TEST_ASSERT_EQUAL(0, s.count());
  s.start();
  TEST_ASSERT_TRUE(s.onSheepDetected());
  TEST_ASSERT_TRUE(s.onSheepDetected());
  TEST_ASSERT_EQUAL(2, s.count());
}

// 进行中禁止切换出门/回圈；结束后可切换。
void test_mode_switch_blocked_while_running() {
  Session s;
  s.start();
  TEST_ASSERT_FALSE(s.selectMode(Mode::Back));
  TEST_ASSERT_EQUAL(Mode::Out, s.mode());
  s.end();
  TEST_ASSERT_TRUE(s.selectMode(Mode::Back));
  TEST_ASSERT_EQUAL(Mode::Back, s.mode());
}

// 出门与回圈分开计数，未归 = 出门 - 回圈。
void test_out_and_back_independent() {
  Session s;
  s.start();
  s.onSheepDetected();
  s.onSheepDetected();
  s.onSheepDetected();  // 出门=3
  s.end();
  s.selectMode(Mode::Back);
  s.start();
  s.onSheepDetected();  // 回圈=1
  TEST_ASSERT_EQUAL(3, s.count(Mode::Out));
  TEST_ASSERT_EQUAL(1, s.count(Mode::Back));
  TEST_ASSERT_EQUAL(2, s.notReturned());
}

// -1 不出现负数；结束后仍可 +1/-1 修正冻结值。
void test_manual_adjust_no_negative() {
  Session s;
  TEST_ASSERT_FALSE(s.decrement());  // 待开始且为 0，-1 忽略
  s.start();
  s.onSheepDetected();
  s.end();
  TEST_ASSERT_TRUE(s.increment());   // 已结束仍可 +1
  TEST_ASSERT_EQUAL(2, s.count());
  TEST_ASSERT_TRUE(s.decrement());
  TEST_ASSERT_TRUE(s.decrement());
  TEST_ASSERT_FALSE(s.decrement());  // 到 0 不再减
  TEST_ASSERT_EQUAL(0, s.count());
}

// 结束后再按开始 = 同一会话继续累加，不清零。
void test_restart_keeps_count() {
  Session s;
  s.start();
  s.onSheepDetected();
  s.end();
  s.start();  // 晚到几只
  s.onSheepDetected();
  TEST_ASSERT_EQUAL(2, s.count());
}

// 长按结束清零当前一头，另一头不受影响。
void test_reset_current_only() {
  Session s;
  s.start();
  s.onSheepDetected();
  s.end();
  s.selectMode(Mode::Back);
  s.start();
  s.onSheepDetected();
  s.resetCurrent();  // 清回圈
  TEST_ASSERT_EQUAL(0, s.count(Mode::Back));
  TEST_ASSERT_EQUAL(1, s.count(Mode::Out));
}

// 正常一只：A 先挡、B 后挡且重叠、时间合理 -> 计一只。
void test_lane_valid_pass() {
  LaneCounter lane;  // 80..1500ms, lockout 300ms
  TEST_ASSERT_FALSE(lane.update(false, false, 0));
  TEST_ASSERT_FALSE(lane.update(true, false, 100));   // A 挡
  TEST_ASSERT_TRUE(lane.update(true, true, 300));      // B 挡, dt=200ms, 重叠
}

// 探头又退：只挡 A 又松开，不计。
void test_lane_retreat_not_counted() {
  LaneCounter lane;
  TEST_ASSERT_FALSE(lane.update(true, false, 100));   // A 挡
  TEST_ASSERT_FALSE(lane.update(false, false, 200));  // A 退, B 从未挡
  TEST_ASSERT_FALSE(lane.update(false, false, 400));
}

// 闭锁：一只之后的短时二次沿（蹄子）不重复计。
void test_lane_lockout() {
  LaneCounter lane;
  TEST_ASSERT_FALSE(lane.update(true, false, 100));
  TEST_ASSERT_TRUE(lane.update(true, true, 300));     // 计一只, 闭锁至 600ms
  TEST_ASSERT_FALSE(lane.update(true, true, 350));    // 闭锁中
  TEST_ASSERT_FALSE(lane.update(true, false, 700));   // 闭锁结束, 重新等 A->B
  TEST_ASSERT_TRUE(lane.update(true, true, 850));      // 第二只
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_counts_only_when_running);
  RUN_TEST(test_mode_switch_blocked_while_running);
  RUN_TEST(test_out_and_back_independent);
  RUN_TEST(test_manual_adjust_no_negative);
  RUN_TEST(test_restart_keeps_count);
  RUN_TEST(test_reset_current_only);
  RUN_TEST(test_lane_valid_pass);
  RUN_TEST(test_lane_retreat_not_counted);
  RUN_TEST(test_lane_lockout);
  return UNITY_END();
}
