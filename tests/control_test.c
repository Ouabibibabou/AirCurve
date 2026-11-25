#include "../firmware/AirCurve/control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#ifdef NDEBUG
#error Tests require assertions enabled.
#endif

static Controller s;
static uint32_t t;
static uint8_t sample(float c, float r) {
  t += 250;
  ac_temperature(&s,t,c,r);
  return ac_command(&s,t,0).duty_pct;
}
static void start(uint32_t at) {
  int i;
  t = at;
  ac_init(&s,t); ac_temperature(&s,t,22,22);
  assert(ac_command(&s,t,0).duty_pct == 100);
  for (i=1;i<8;i++) assert(sample(22,22) == 100);
  assert(sample(22,22) == 30);
}
static uint8_t settle(float c) {
  int i;
  for (i=0;i<80;i++) sample(c,22);
  return ac_command(&s,t,0).duty_pct;
}
static void test_adc(void) {
  assert(isnan(ac_probe_c(0)) && isnan(ac_probe_c(1023)) && isnan(ac_probe_c(NAN)));
  assert(fabs(ac_probe_c(511.5f)-25) < 0.01);
  assert(ac_probe_c(600) < 25 && ac_probe_c(400) > 25);
  puts("PASS ADC polarity, nominal divider midpoint and rail faults");
}

static void test_concurrent_faults(void) {
  int i;
  start(0); assert(sample(40,NAN)==100 && s.hot && s.probe_fault);
  for(i=0;i<10;i++) assert(sample(38,38)==100);
  assert(!s.probe_fault && s.hot);
  assert(sample(37,37)==30 && !s.hot);
  puts("PASS cabinet trip survives concurrent ambient failure and requalification");
}

static void test_startup_wrap(void) {
  start(UINT32_MAX-999); assert(!s.starting); puts("PASS startup across millis wrap");
}

static void test_curve(void) {
  start(0);
  assert(settle(25.5f)==50 && settle(24.5f)==50 && settle(23.5f)==30);
  assert(settle(28.5f)==75 && settle(27.5f)==75 && settle(26.5f)==50);
  assert(settle(32.5f)==100 && settle(30.5f)==100 && settle(29.5f)==75);
  puts("PASS rising and falling thresholds on all three stages");
}

static void test_hot_latch(void) {
  start(0); assert(sample(40,22)==100 && s.hot);
  assert(sample(38,22)==100 && s.hot); sample(37,22); assert(!s.hot);
  puts("PASS unfiltered absolute trip and hysteretic release");
}

static void test_probe_recovery(void) {
  int i;
  start(0); assert(ac_command(&s,t,1).duty_pct==100 && ac_command(&s,t,0).duty_pct==30);
  assert(sample(NAN,22)==100 && s.probe_fault); sample(22,22);
  for(i=0;i<7;i++) assert(sample(22,22)==100);
  assert(sample(22,100)==100 && s.probe_fault); sample(22,22);
  for(i=0;i<7;i++) assert(sample(22,22)==100);
  assert(sample(22,22)==30 && !s.probe_fault);
  puts("PASS boost, either probe invalid, uninterrupted two-second recovery");
}

static void test_sample_gap(void) {
  start(0); assert(ac_command(&s,t+400,0).duty_pct==30 && !s.probe_fault);
  start(0); t+=401; assert(ac_command(&s,t,0).duty_pct==100 && s.probe_fault);
  start(0); t+=401; ac_temperature(&s,t,22,22);
  assert(ac_command(&s,t,0).duty_pct==100 && s.probe_fault);
  puts("PASS stale data caught before or during next sample");
}

static void tach_window(uint32_t rpm) {
  uint32_t opened = t;
  for (int i = 0; i < 4; ++i) sample(22,22);
  ac_tach(&s, opened, rpm);
}

static void test_tach_latch(void) {
  start(0);
  tach_window(0); tach_window(119); tach_window(120);
  assert(s.bad_windows==0 && !s.tach_fault);
  tach_window(2400); assert(!s.bad_windows);
  tach_window(2401); tach_window(2401); assert(!s.tach_fault);
  tach_window(0); assert(s.tach_fault && ac_command(&s,t,0).duty_pct==100);
  for(int i=0;i<10;i++) tach_window(1600);
  assert(s.tach_fault && ac_command(&s,t,0).duty_pct==100);
  start(0); assert(!s.tach_fault); puts("PASS tach bounds, consecutive failures, reset-only latch");
}

static void test_tach_startup_boundary(void) {
  uint32_t boot = UINT32_MAX - 500;
  start(boot);
  // A window opened 1 ms before startup ended, but was submitted in AUTO.
  ac_tach(&s, boot + 1999u, 0);
  assert(s.bad_windows == 0 && !s.tach_fault);
  ac_tach(&s, boot + 2000u, 0);
  assert(s.bad_windows == 1);
  // On a later timer wrap, the numerical proximity to boot is coincidental.
  ac_tach(&s, boot, 0);
  ac_tach(&s, boot + 1000u, 0);
  assert(s.tach_fault);
  puts("PASS tach window start boundary and no repeat exemption on timer wrap");
}

static void expect_command(uint8_t boost, AcMode mode, uint8_t pct) {
  AcCommand cmd = ac_command(&s, t, boost);
  assert(cmd.mode == mode && cmd.duty_pct == pct);
}

static void test_command_reason(void) {
  t = 0;
  ac_init(&s, t);
  expect_command(1, AC_START, 100);
  start(0);
  settle(33);
  expect_command(0, AC_AUTO, 100); // Top curve stage, not an override.
  sample(40, NAN);
  expect_command(1, AC_PROBE, 100); // Cabinet trip and boost are both masked.
  for (int i = 0; i < 9; ++i) sample(38, 38);
  expect_command(1, AC_HOT, 100);
  sample(37, 37);
  expect_command(1, AC_BOOST, 100);
  expect_command(0, AC_AUTO, 30);
  for (int i = 0; i < 3; ++i) tach_window(0);
  sample(40, NAN);
  expect_command(1, AC_TACH, 100);
  puts("PASS reported mode and duty agree through concurrent overrides and recovery");
}

int main(void) {
  test_adc();
  test_concurrent_faults();
  test_startup_wrap();
  test_curve();
  test_hot_latch();
  test_probe_recovery();
  test_sample_gap();
  test_tach_latch();
  test_tach_startup_boundary();
  test_command_reason();
  return 0;
}
