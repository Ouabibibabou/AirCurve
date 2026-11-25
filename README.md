# AirCurve: Cabinet Exhaust Controller

AirCurve adjusts a 120 mm exhaust fan based on the temperature difference between a PC cabinet and its air intake. The Arduino Uno R3 monitors fan speed through the tachometer input. A pushbutton toggles full speed. Once programmed, the controller runs without a host PC.

**Prototype status:** The control tests pass and the firmware builds for the Uno R3. Electrical and cabinet thermal tests are still pending. See the [build record and acceptance criteria](verification.md). The controller does not replace the PC's thermal protection.

## Wiring and First Startup

1. Wire the unpowered assembly using the [AC-R2 parts and wiring guide](hardware.md). Leave the entire four-pin fan connector unplugged for the USB-only check.
2. Connect USB. Open `firmware/AirCurve/AirCurve.ino` in Arduino IDE with **Arduino AVR Boards 1.8.6**, select **Arduino Uno** (classic R3, not R4) and upload. No external libraries are needed. Open Serial Monitor at **115200 baud** and check both probe readings. A tach fault is expected without the fan.
3. Disconnect USB. With both supplies off, check polarity and common ground, fit the guard and plug in the fan. Apply fan 12 V first, then USB. After reset the controller commands full speed for two seconds, then AUTO if no override is active. Opening Serial Monitor may reset the Uno. Disconnect USB first on shutdown.
4. Place the inside probe near the cabinet's upper exhaust path, away from direct contact with the PC or fan motor. Place the reference probe by the room-air intake and leave a clear intake opening.

## Control behavior

| Condition | Command | Return condition |
| --- | --- | --- |
| Startup | 100% for 2 s | Switch to automatic if no override |
| Filtered cabinet minus room temperature reaches 3 / 6 / 10 C | 30% base, then 50 / 75 / 100% | Step down at or below 8 / 5 / 2 C, from highest to lowest |
| Cabinet reaches 40 C | 100% | Cabinet at or below 37 C |
| Boost enabled by pushbutton | 100% | Press again or reset to clear the request |
| Probe open, short, out of range, or samples more than 400 ms apart | 100%, fault LED on | Both probes continuously valid for at least 2 s |
| Tach outside 120..2400 RPM for three consecutive 1 s windows starting at or after the end of the startup hold | 100%, fault LED on | Inspect fan and wiring, repair, then reset; healthy pulses alone cannot clear the latch |

Temperature is sampled every 250 ms. Differential control uses an exponential filter (alpha = 0.25); the absolute cutoff checks unfiltered samples. A nonblocking, debounced button remains responsive during temperature and RPM scheduling. Tach faults indicate missing/slow feedback; they do not distinguish an unplugged signal from a mechanical failure. Full duty is a **command**, not a guarantee of airflow. Sensor limits cannot detect every plausible but incorrect reading.

**Timer ownership:** Timer1 generates 25 kHz for the four-wire fan's control input. Do not combine this sketch with Servo or other Timer1 users. Timer0 still supplies `millis()`. The tach ISR only counts falling edges; the main loop takes an atomic snapshot and uses the actual elapsed interval to calculate RPM.

## Diagnostics

USB serial, 115200 baud, one CSV row per nominal one-second tach window. Read columns by header name:

`ms,cabinet_c,ambient_c,duty_pct,rpm,mode,probe_fault,tach_fault,cabinet_raw_c,ambient_raw_c,hot,boost_requested`

`mode` is the reason for the current command, in descending priority: TACH, START, PROBE, HOT, BOOST, AUTO. `boost_requested` is the button's latched request (0 or 1), even when a higher-priority condition masks it. For example, `HOT` with `boost_requested=1` returns to `BOOST`, not `AUTO`, after the cabinet cools if no other override is active. Readers of older logs should treat a missing `boost_requested` column as unknown, not zero.

`cabinet_c` and `ambient_c` are the filtered pair used for differential control. Raw columns are the most recent unfiltered temperatures, not ADC counts; `nan` denotes an invalid channel. `hot` tracks the cabinet trip independently of the ambient probe. Fault flags can coexist. Duty is commanded, RPM is measured; neither measures airflow. The log is a periodic snapshot, not an event recorder.

When the fan stays at full duty, read `mode` before changing the curve. `AUTO` at 100% is normal at the top stage. For `PROBE`, inspect the two **raw** columns: either bad channel invalidates both filtered readings. For `TACH`, check the fan supply, rotation and D2 signal before reset; a nonzero RPM after repair does not clear the latch. Where faults coincide, `mode` shows only the highest-priority condition; inspect the individual flags as well.

| First-run check | Expected observation |
| --- | --- |
| Put probes side by side | Temperatures close, duty settles to 30%, RPM nonzero. Investigate a persistent offset before mounting. |
| Warm only A0's probe gently in your hand | Cabinet reading rises before ambient; duty increases when filtered difference crosses 3 C. |
| Press and release the button twice | First press gives BOOST/100%; second returns to AUTO when no override exists. |
| With power off, wire a closed switch in series with one NTC. Power up, wait for healthy AUTO, then open the switch. | PROBE, fault LED, 100% within a sample; close the switch and allow two seconds of valid readings to recover. |
| Open a prewired switch in the tach signal, leaving the D2 pull-up attached | TACH within about four seconds in steady operation. Reclose: RPM returns but fault stays latched; inspect, then reset. Never obstruct the blades. |

These checks establish wiring and basic function only. Use the [acceptance procedure](verification.md) for timing limits, paired thermal runs and evidence retention.

## Command-line build

From the repository root, with Arduino CLI 1.3.1 installed. Use the same USB-only upload procedure as above:

```sh
arduino-cli core update-index
arduino-cli core install arduino:avr@1.8.6
arduino-cli compile --warnings all --fqbn arduino:avr:uno --output-dir build/uno firmware/AirCurve
arduino-cli board list
# Replace COM4 with the port reported above:
arduino-cli upload -p COM4 --fqbn arduino:avr:uno --input-dir build/uno firmware/AirCurve
arduino-cli monitor -p COM4 --config baudrate=115200
```

The GitHub Actions workflow runs the native control tests and compiles the sketch for the Uno. Host reproduction: `cc -std=c99 -Wall -Wextra -Werror -pedantic tests/control_test.c firmware/AirCurve/control.c -lm -o control-test`, then run `./control-test`. Neither a native test nor a successful target build is a hardware test.
