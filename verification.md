# Acceptance record | AC-R2

**Decision: open.** Scope is one nominated cabinet, one specified fan, indoor dry use, room temperature 18..30 C. Thresholds below are project targets, not certifications or manufacturer guarantees. Do not extrapolate the result to other cabinets, heat loads, fans or production units. This is an accessory, not a safety interlock.

## Release gates

| ID | Pass criterion | How to verify and what to retain | Current evidence |
| --- | --- | --- | --- |
| SW-01 | Native regression suite passes; target builds with pinned Uno core | Run CI on the release commit; retain log including flash and SRAM use | Both checks passed locally; release-commit CI evidence pending. See build record below. |
| ELEC-01 | At the powered fan connector: 25 kHz +/- 1 kHz and 30/50/75% commands within 3 percentage points; a full-speed command holds PWM continuously high | Scope pin 4 relative to pin 1 with the actual cable and fan; record period, high time and high/low voltages. Check low <0.8 V and high 2.1..5.25 V. Check continuous high with only fan power, during Uno reset with both supplies on, during the two-second startup command, and in manual BOOST. After startup, healthy AUTO may reduce duty. | Open |
| TEMP-01 | Both probes within +/-1.5 C of a reference; disagreement <=1.0 C side by side | Compare at approximately 22 and 40 C in stable air using a reference of specified uncertainty <=0.3 C; record values after stabilization. Replace mismatched probes; do not adjust trip points just to obtain a pass. | Open |
| FAIL-01 | Either probe open or short commands full duty within 300 ms; no return before 2 s of continuous valid samples | With the fan guarded, insert a prewired switch or short across the NTC only (the fixed 10k stays in circuit). Scope ADC node and fan PWM, each channel separately. Bounce the fault during recovery. One-second CSV alone cannot prove latency. | Logic tested; physical fault injections open |
| FAIL-02 | Loss of tach feedback in AUTO latches full duty and LED within 4.1 s; feedback restoration alone cannot clear | Interrupt only the tach signal via a prewired switch. Record switch time, LED/PWM, RPM, and reset after repair. Do not block the rotor. Test at normal 30% as well as a higher duty. | Logic tested; physical fault injections open |
| SYS-01 | Under the declared workload, AUTO lowers steady cabinet-to-room rise by >=2.0 C versus fixed 30% PWM duty; cabinet temperature stays <40 C | Run the paired cabinet experiment below. Record RPM in both configurations. | Open |
| SYS-02 | 8 h powered with zero unexpected resets, no false faults, and no missing telemetry interval >2.5 s | Log a complete run with an idle/load/idle cycle and host timestamps. Reset or data loss invalidates the run until explained. Test 10 power cycles separately: fan starts every time. | Open |

## Local build record (2026-11-25)

| Local check | Result |
| --- | --- |
| Native control tests, Windows x64 | Ten regression groups passed with TinyCC 0.9.27 (`-std=c99 -Wall -Werror`), including tach window eligibility, timer rollover, and command duty/mode priority under concurrent overrides. |
| Clean Uno build | Arduino CLI 1.3.1, Arduino AVR Boards 1.8.6, `arduino:avr:uno`, `--warnings all`. No project-source warnings; four unused-parameter warnings in the pinned core's `new.cpp`. |
| Linked image | 7,514 / 32,256 bytes flash; 285 / 2,048 bytes static SRAM. Static allocation is not a runtime stack measurement. |
| Sketch HEX, without bootloader | SHA-256 `67ba83fdd5f623544802f5cbe66b7c5fe69034f3c08f2d88c2f9cfda85e88319` |

No board upload, waveform measurement or cabinet test was performed for this record.

## Paired cabinet experiment

1. Record cabinet dimensions, intake opening, fan orientation, probe positions, room temperature, PC workload and its measured input power. Use the same closed cabinet and cable routing in both runs. Check probes against the reference before and after the experiment.
2. Power down before exchanging drivers. For the baseline, disconnect U1 output from fan pin 4 and substitute a common-ground, push-pull 0..5 V PWM source, 25 kHz, 30% high time verified at the loaded connector. Never connect both outputs. Keep tach, probes and logging connected; label the run **external fixed duty**. The duty column is the Uno request, not the applied baseline duty.
3. Repeat with U1 connected and boost off. Begin workload and scoring only after startup and probe qualification. Start within 1 C of the same ambient and cabinet temperatures; keep mean input power within 5% and ambient drift within 1 C. Supervise **both** runs using an independent cabinet thermometer with peak capture. Stop the workload at 40 C on either cabinet instrument, a HOT indication, or PC thermal throttling. Do not rely on one-second serial snapshots to catch a peak.
4. Keep the entire log including startup. During scoring require AUTO, both fault flags clear, finite temperatures, increasing device timestamps, no restart header and no host logging gap >2.5 s. Mark incomplete or fault-contaminated comparisons inconclusive; retain the fault as a separate finding. Do not splice runs or silently discard suspect intervals.
5. A confirmed cabinet temperature >=40 C in the automatic run **fails SYS-01** regardless of its average. A baseline stop, throttling without a confirmed cabinet limit violation, or uncertainty spanning a decision threshold makes the comparison inconclusive. For uninterrupted runs, compare successive non-overlapping 5-minute mean temperature rises; take the first pair differing by <=0.3 C. If none qualifies within 60 min, report inconclusive.
6. Compare the 10-minute mean of `cabinet_c - ambient_c` over those two windows. Repeat in reverse order. Require >=2.0 C improvement in **both** pairs; report each pair, not just the best.

## Failure boundaries and installation

The tachometer detects pulses, not airflow. Blocked filters and recirculation can leave healthy RPM; inspect the intake and verify SYS-01 after installation changes. A detached probe can still return plausible room temperature; mechanically retain and label each probe. The reset default depends on a working fan supply and driver. An MCU freeze can leave PWM running at the last setting: there is no independently validated watchdog or overtemperature cutoff. Never use this unit as the sole protection for an unattended heat source.

Breadboard and loose jumpers are for supervised bench work only. Before cabinet installation use a soldered or PCB assembly, keyed and labeled connectors, insulated enclosure, cable strain relief, and guards on accessible blades. Use external, enclosed, current-limited supplies; do not modify mains wiring. Inspect polarity and continuity with power off.

## Evidence record

Create one record per attempt: `ID | commit | board/fan/probe IDs | supply | instrument/accuracy | setup | raw file names | measured value and uncertainty | PASS/FAIL/INCONCLUSIVE | date`. Keep original scope exports or CSV with that record. Re-run the affected gates after any firmware, wiring, fan or enclosure change. No noise reduction, deployment qualification or compliance claim is supported by native tests alone.
