# Remote v2: architecture (proposal, 2026-10-11)

Goal: portable HC-12 remote, 2×AAA. Sends start/stop/pause/set to the timer panel's HC-12; its OLED mirrors the panel state.
Hold the power button 1 s to toggle on/off (off = deep sleep). Auto-off after 5 h.

## Power: the converter is ALWAYS on, so the MCU never loses power
```
2xAAA ─ Q1 AO3401A reverse-battery FET ─ VBAT ─ U2 TPS631000 (EN=VBAT, MODE=GND) ─ VCC 3.3 V (always on)
                                           │                                          ├─ U1 ATtiny (always powered, deep sleep)
                                           └─ 100k ─ ADC (VBAT sense, no divider)     ├─ Q2 AO3401A ─ VRF:   HC-12 (on only per command)
                                                                                      └─ Q3 AO3401A ─ VOLED: OLED + I2C pull-ups (on while "on")
```
Why: the old board's faults all came from switching the MCU's own supply (diodes, a backwards FET, no VDD cap, brown-out on switch-on).
Here the MCU sits on a stable 3.3 V. Only the radio and OLED are switched (separately), and each FET gets a full −3.3 V of gate drive.

**Radio use (user, 2026-10-11):** the HC-12 is powered ONLY for a button command. Power up → send → wait for the panel's ACK → power off.
No sync: the remote and the panel each run the countdown on their own.

| State | Draw from battery | 2×AAA (~1000 mAh) |
|---|---|---|
| Off (deep sleep) | ≈ 15 µA | years |
| On, countdown on the OLED, radio off | ≈ 12–15 mA (OLED dominates) | ≈ 65 h ≈ 13 five-hour shows |
| Per command (HC-12 ~0.3–0.5 s at up to 100 mA TX) | negligible | |

**Clock:** two free-running countdowns drift apart. The ATtiny internal 32 kHz is only a few % accurate (about 1–2 min per hour).
Add a 32.768 kHz crystal (TOSC on PB2/PB3; the HC-12 uses the alternate USART pins PA1/PA2): ±20 ppm, about 0.1 s per hour.
Optional, at no cost: the panel's ACK carries its remaining seconds, and the remote adopts it.

## Blocks
- **Converter:** U2 TPS631000 (C5219190) + 1 µH FXL0420 + 10 µF in + 2×10 µF out (C380347). FB 511k/91k = 3.31 V.
- **Reverse battery:** Q1 AO3401A (C15127, Basic). D → battery+, S → VBAT, G → GND via 100k. At 2 V VGS it is ≈ 0.1 Ω, versus 0.3 V lost in a Schottky.
- **Radio switch Q2 / OLED switch Q3:** AO3401A each, S = VCC, D = VRF / VOLED. 100k gate pull-up to VCC = OFF in reset/sleep.
  MCU drives the gate through 10k with 47 nF gate–source for a ~1 ms soft-start, so VCC doesn't dip. 22 µF bulk on VCC at the MCU.
- **MCU:** ATtiny1616 (as before), or ATtiny3226 (same 20-pin SOIC, 2 USARTs → debug UART + HC-12 at once). TBD on stock.
- **Buttons:** 6 tactile switches (up/left/right/down/OK + power) straight to GND, using the MCU's internal pull-ups. No external R/C.
  Power on a fully-asynchronous pin (wakes from power-down).
- **HC-12:** 5-pin header on VRF. MCU drives SET (no pull-down resistor).
- **OLED:** 4-pin I2C header on VOLED, 4.7k pull-ups to VOLED.
- **Clock:** 32.768 kHz crystal on TOSC1/TOSC2 + load caps.
- **Headers:** UPDI 3-pin, battery 2-pin (THT: DNP, buy loose).

## Firmware duties (summary)
- Hold the power button 1 s to toggle. Off: Q2/Q3 off, then UART/SET/I2C pins low, then power-down sleep with a wake on the power pin.
- Auto-off after 5 h (RTC on the 32.768 kHz crystal).
- HC-12 per command: Q2 on → wait ~80 ms (module boot) → send cmd + seq number → wait for ACK ≤ 300 ms, retry ×3 → Q2 off; UART/SET pins low.
  The OLED shows "sent ✓" or "no ACK ✗".
- Countdown runs locally from the 32.768 kHz crystal (RTC). Optionally take the panel's remaining time from the ACK.
- Battery: ADC VBAT against VDD (3.3 V); show a gauge; warn below 2.0 V.
