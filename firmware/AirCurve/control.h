#ifndef AIRCURVE_CONTROL_H
#define AIRCURVE_CONTROL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { AC_STARTUP_MS = 2000, AC_PROBE_RECOVERY_MS = 2000,
       AC_SAMPLE_MS = 250, AC_MAX_GAP_MS = 400 };
typedef struct {
  float cabinet, room;
  uint32_t last_sample, valid_since, boot;
  uint8_t level, hot, qualifying, probe_fault, tach_fault, bad_windows, starting, tach_armed;
} Controller;
void ac_init(Controller *s, uint32_t now);
float ac_probe_c(float adc);
void ac_temperature(Controller *s, uint32_t now, float cabinet, float room);
// Submit one completed measurement window, timestamped at its first edge-count snapshot.
void ac_tach(Controller *s, uint32_t window_start_ms, uint32_t rpm);
typedef enum { AC_AUTO, AC_BOOST, AC_HOT, AC_PROBE, AC_START, AC_TACH } AcMode;
typedef struct { AcMode mode; uint8_t duty_pct; } AcCommand;
AcCommand ac_command(Controller *s, uint32_t now, uint8_t boost);
#ifdef __cplusplus
}
#endif
#endif
