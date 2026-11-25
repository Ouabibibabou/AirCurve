#include "control.h"
#include <math.h>
#include <string.h>

// Each row describes one stage above the 30% base duty.
static const struct { uint8_t enter_c, leave_c, duty_pct; } stages[] = {
  { 3, 2,  50 },
  { 6, 5,  75 },
  {10, 8, 100 }
};
enum { STAGE_COUNT = sizeof stages / sizeof stages[0], BASE_DUTY_PCT = 30 };

static int plausible(float c) { return c >= -10 && c <= 85; }

float ac_probe_c(float adc) {
  // Equal nominal divider and NTC resistances cancel in R/R25.
  if (!(adc >= 3 && adc <= 1020)) return NAN;
  float ratio = adc / (1023.0f - adc);
  float c = 1.0f / (1.0f / 298.15f + (float)log(ratio) / 3950.0f) - 273.15f;
  return plausible(c) ? c : NAN;
}

void ac_init(Controller *s, uint32_t now) {
  memset(s, 0, sizeof *s);
  s->cabinet = s->room = NAN;
  s->boot = s->last_sample = now;
  s->probe_fault = s->starting = 1;
}

static void invalidate(Controller *s) {
  s->qualifying = 0;
  s->probe_fault = 1;
  s->cabinet = s->room = NAN;
}

void ac_temperature(Controller *s, uint32_t now, float c, float r) {
  float d;
  if (now - s->last_sample > AC_MAX_GAP_MS) invalidate(s);
  s->last_sample = now;
  // Only a valid cabinet sample may change the absolute-temperature latch.
  if (plausible(c)) {
    if (c >= 40) s->hot = 1;
    else if (c <= 37) s->hot = 0;
  }
  if (!plausible(c) || !plausible(r)) {
    invalidate(s);
    return;
  }
  if (!s->qualifying) {
    s->valid_since = now;
    s->qualifying = 1;
    s->cabinet = c; s->room = r;
  } else {
    s->cabinet += 0.25f * (c - s->cabinet);
    s->room += 0.25f * (r - s->room);
  }
  if (now - s->valid_since >= AC_PROBE_RECOVERY_MS) s->probe_fault = 0;
  d = s->cabinet - s->room;
  while (s->level < STAGE_COUNT && d >= stages[s->level].enter_c) ++s->level;
  while (s->level > 0 && d <= stages[s->level - 1].leave_c) --s->level;
}

void ac_tach(Controller *s, uint32_t window_start_ms, uint32_t rpm) {
  if (!s->tach_armed) {
    if (window_start_ms - s->boot < AC_STARTUP_MS) return;
    s->tach_armed = 1; // A later millis() wrap must not re-open the startup exemption.
  }
  if (rpm >= 120 && rpm <= 2400) { s->bad_windows = 0; return; }
  if (s->bad_windows < 3) ++s->bad_windows;
  if (s->bad_windows == 3) s->tach_fault = 1; // Repair and reset to clear.
}

AcCommand ac_command(Controller *s, uint32_t now, uint8_t boost) {
  AcCommand cmd;
  if (s->starting && now - s->boot >= AC_STARTUP_MS) s->starting = 0;
  if (now - s->last_sample > AC_MAX_GAP_MS) invalidate(s);
  cmd.duty_pct = 100;
  if (s->tach_fault) cmd.mode = AC_TACH;
  else if (s->starting) cmd.mode = AC_START;
  else if (s->probe_fault) cmd.mode = AC_PROBE;
  else if (s->hot) cmd.mode = AC_HOT;
  else if (boost) cmd.mode = AC_BOOST;
  else {
    cmd.mode = AC_AUTO;
    cmd.duty_pct = s->level ? stages[s->level - 1].duty_pct : BASE_DUTY_PCT;
  }
  return cmd;
}
