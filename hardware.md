# AC-R2 | Parts and wiring

R2 replaces the R1 open-collector stage with a push-pull inverter. The D9 polarity is unchanged; R1 electrical measurements do not qualify R2.

| Qty | Part | Role |
| ---: | --- | --- |
| 1 | Arduino Uno Rev3, ATmega328P, 5 V / 16 MHz; USB-B data cable | Controller and logic supply |
| 1 | Noctua NF-P12 redux-1700 PWM, 12 V, four-pin | Cabinet exhaust; do not substitute a 3-wire fan |
| 2 | Adafruit #372, 10 kOhm NTC at 25 C, beta 3950 K | One probe for cabinet air, one for room air |
| 1 | TI SN74LVC1G04DBVR on a SOT-23-5 breakout | U1, push-pull PWM inverter |
| 3 | 10 kOhm 1% resistor | Two thermistor dividers and one tach pull-up |
| 1 each | 100 kOhm resistor; 100 nF ceramic capacitor | U1 input pull-down; local supply bypass |
| 1 | Normally open momentary button | Manual boost |
| 1 each | Regulated 12 V DC / 1 A adapter, matching barrel-to-terminal adapter, keyed four-pin PC fan header/breakout, breadboard and jumpers | Power and connections |
| 1 | 120 mm finger guard and mounting hardware | Guard the exposed side; keep wires clear |

Use the **numbered fan connector positions**, not wire color. Verify the keyed fan header and U1 breakout against the package top view. Do not attach the fan to a PC motherboard header at the same time.

```text
Fan 1 GND   ---------------- adapter (-) and Arduino GND
Fan 2 +12V  ---------------- adapter (+12 V) ONLY
Fan 3 TACH  ------+--------- D2
                  +-- 10k -- Arduino 5V
Fan 4 PWM   ---------------- U1 pin 4 (Y)
Arduino D9  ---------------- U1 pin 2 (A); 100k from A to GND
Arduino 5V  ---------------- U1 pin 5 (VCC)
Arduino GND ---------------- U1 pin 3 (GND)
100 nF directly between U1 pins 5 and 3; U1 pin 1 (NC) unconnected

Arduino 5V -- 10k --+-- cabinet NTC -- GND
                    +-- A0
Arduino 5V -- 10k --+-- ambient NTC -- GND
                    +-- A1
Arduino D4 -------- normally open button -------- GND
Fault indicator: built-in LED, no extra wiring.
```

**Power:** use USB for the Uno; use the separate 12 V adapter only for the fan. Join the two grounds. Never put 12 V on 5V, GPIO, TACH, or PWM. Keep the fan motor current on the adapter-to-fan path, not through an Arduino pin. Cut power before rewiring. A USB wall adapter can replace the computer after uploading.

**PWM interface:** Noctua recommends a CMOS drive because the fan's internal pull-up can give slow rising edges with an open collector. U1 actively drives both edges. With 5 V present and D9 high-impedance during reset, the pull-down selects a high PWM output. With U1 unpowered the selected device supports partial power-down; verify the fan-only state and actual connector waveform on the bench. Tach remains an open-collector signal with a 10k pull-up. PWM and tach are not interchangeable.

**Commissioning:** With the fan connector completely unplugged, power only the Uno over USB and check both probe readings. A tach fault is expected. Switch off, verify polarity and ground continuity, fit the guard and connect the fan. Apply 12 V first, then USB; disconnect USB first on shutdown. Do not intentionally drive an unpowered fan's PWM input. After a persistent zero RPM reading, remove power and check wiring before reset.

**Loss of fan power:** AC-R2 does not sense the 12 V rail or isolate PWM when that rail disappears. With USB still present, U1 can continue to drive pin 4 of an unpowered fan. The tach latch reports lost rotation; it cannot isolate the pin or restore cooling. Remove USB power if the fan supply is lost. This split-supply design is not qualified for unattended supply interruptions.

**Part references:** [Arduino Uno Rev3](https://docs.arduino.cc/hardware/uno-rev3/), [Noctua MCU interface guidance](https://www.noctua.at/en/support/faqs/microcontroller-guide-pwm-setup-and-rpm-monitoring), [Adafruit #372 thermistor](https://www.adafruit.com/product/372), [TI SN74LVC1G04 datasheet, DBV package](https://www.ti.com/lit/ds/symlink/sn74lvc1g04.pdf).
