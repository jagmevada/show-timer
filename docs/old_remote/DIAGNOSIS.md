# Remote: diagnosis (2026-10-11)

## LATER VERSION (the one built): `timer_remote/wireless_timer/pcb/combined.sch`, branch dcdc
Fabbed from branch `panel` (Gerbers 2024-08-14). Same nets as dcdc; checked with `eagle_net.py` and the .brd contactrefs.
Also confirmed in JLCONE project **remote_1** (schematic + PCB, 2026-10-11): U2 pad 5 = N$36 (R45 only), pad 6 = SHDN; U4 pad 2 (S) = VCC, pad 3 (D) = VDD. U2/U3/U4/D1–D3 are on the BOTTOM layer.

Power tree as drawn:
```
2xAAA VBAT -JP6/JP7- VBAT_O -+- D1 ---------------------> VDD (ATtiny1616, power button S1 pull-up)
                             +- U3 P-FET (DC_SW) || D3 --> VIN -> U2 TPS631000 buck-boost -> VCC 3.3 V
VCC (HC-12 A1, OLED, I2C + nav-button pull-ups) -D2-> VDD ;  U4 P-FET S=VCC D=VDD G=VCC_SW
```
| # | Fault | Evidence | Effect |
|---|---|---|---|
| A | **TPS631000 EN (pin 5) has only R45 50k to GND.** The MCU's SHDN (PB5) goes to MODE (pin 6) | brd: U2.5 = N$36 (R45 only); U2.6 = SHDN. Datasheet: EN high = on, must not float | **Buck-boost never runs.** SHDN only toggles PFM/PWM |
| B | **U4 is backwards:** its body diode (D→S) conducts VDD → VCC | U4 S=VCC, D=VDD | VCC is fed from the battery via D1 + body diode/channel ≈ VBAT − 1 V. This is the only reason the HC-12 works at all |
| A+B | HC-12 runs on ~2.0–2.6 V (spec 3.2–5.5 V), through a FET whose VGS(th) is up to −2.1 V | | "Sometimes doesn't power on"; works only on fresh cells |
| C | VCC can never be switched off: B back-feeds the HC-12 (~16 mA idle), the OLED and the pull-ups all the time | | Battery drains in days |
| D | R19 30k SET → GND | HC-12 SET low = AT mode | ~0.1 mA while SET is held high |

### Rework on existing boards (no re-spin)
1. Solder-bridge U2 pin 5 (EN) to pin 6 (MODE/SHDN). They are adjacent pads on the same side (0.5 mm pitch).
   PB5 then enables the converter (forced PWM while on). R20 ∥ R45 keep it off during reset.
2. Remove U4. VDD then comes from VBAT via D1, or from VCC via D2.
3. Firmware:
   - PB5 high = VCC on; low = everything except the MCU off.
   - Before VCC off: TXD1, SET, SDA/SCL → low or input (no back-powering).
   - MCU power-down sleep; wake on the power button (PA6, pulled up to VDD).
   - Turn VCC off after an idle timeout.
### For a re-spin
- EN = SHDN (keep the 30k pull-down); MODE = GND (power-save).
- U4 removed, or flipped (D=VCC, S=VDD, gate pull-up to VDD).
- R19 DNP.

---
## OLDER VERSION: `src/ProPrj_remote_sch_2026-10-11.epro2` (reference only)

Source: `src/ProPrj_remote_sch_2026-10-11.epro2` (Eagle import, one page, two circuits).
Netlist from `netlist.py src/<file> netlist.json 1,1,-1`: 430 pins, 1 floating (U9 gate).

- **Remote:** U1 ATtiny1616, HC12, OLED, S1–S5 nav (up/left/right/down/OK), S6 power button, 2×AAA.
- **Timer receiver:** U4 ATtiny1616, U5 AMS1117-3.3 from 12 V, HC0/HC1, U7/U9 12 V switch with Q1, TMR header (AX–FX).

## Remote faults (battery life + "sometimes won't power on")
| # | Where | Fault | Effect |
|---|---|---|---|
| 1 | D1 CGRM4005 (1 A rectifier) in series with the HC-12 supply | Drops 0.7–0.9 V. The HC-12 datasheet's series 1N4007 is only meant for supplies above 4.5 V | 3.0 V battery → about 2.2 V at the HC-12 (spec 3.2–5.5 V). Works only on fresh cells |
| 2 | U2 DMP3165L HC-12 switch, gate = PA3 (M0) | VGS(th) up to −2.1 V, RDS(on) specified only at −4.5 V. Gate drive = VCC − D1 drop ≈ 2.2 V, falling to 1.7 V | FET at threshold: sometimes doesn't turn on (the "MOSFET issue") |
| 3 | U2 gate | No pull-up. Floats while the MCU is in reset or being programmed | Random HC-12 on/off at power-up |
| 4 | U3 OLED switch: gate = RXD0 through divider R10/R11 to GND | A high level only reaches VCC/2, so VGS = −1.5 V | OLED can never be fully switched off: constant drain |
| 5 | R7 10k SET → GND | HC-12 SET low = AT command mode. Holding SET high wastes about 0.3 mA | Drain, plus AT mode whenever the pin floats |
| 6 | MCU TX → HC-12 RX, SDA/SCL pull-ups while the HC-12/OLED are switched off | Back-powers the unpowered module through its I/O pins | Drain, latch-up, failed start |
| 7 | R3/R4 10k/10k always across VBAT | 150 µA continuous | About 3.6 mAh/day |
| 8 | Firmware | HC-12 FU3 idle ≈ 16 mA unless switched off or sent AT+SLEEP | 2×AAA (≈1000 mAh) lasts about 2–3 days, and much less given #1/#2 |

## Receiver faults
- D2/D3 diodes: same as #1. 3.3 V − 0.8 V = 2.5 V at HC0/HC1, below spec.
- U9 sits on top of U7 (parallel P-FET). **U9 gate is not connected.** If it's fitted, the 12 V output can't be switched reliably.
- U6/U8 gates have no pull-up (same as #3).

## Open questions to the user
- What connects BAT+ (VBAT) to VCC on the remote? Off-board test points BAT+, TP4 (VCC), CHG, TPVAO and K (= S6 node): a boost or charger module, or direct?
- Is U9 fitted on the receiver board?
