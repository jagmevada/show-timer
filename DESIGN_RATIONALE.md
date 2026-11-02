# remote_v2: why each circuit and part was chosen

Final 2026-10-11. "User" = the user's decision; everything else is the engineering reason.
Software duties: `FIRMWARE_NOTES.md`. Spec of every pin: `work/gen_spec.py`.

## System choices
| Decision | Why |
|---|---|
| **New board instead of reworking the old remote** (user) | The old board had three wiring faults (`docs/old_remote/DIAGNOSIS.md`). The user didn't want to patch it before a show a month away. |
| **Same outline, button and OLED/HC-12/battery positions as the old board** (user) | Fits the existing 3D-printed case. The outline was copied arc-for-arc from remote_1 and checked on the Gerber. |
| **2×AAA alkaline** (user) | The existing case. 2.0–3.3 V, so a buck-boost is required: the HC-12 needs ≥ 3.2 V. |
| **HC-12 on only for a command** (user) | Press → power HC-12 → send → wait for ACK → power off. The remote and the panel run the countdown independently. **No sync, no crystal** (user). |
| **OLED shows the countdown; it sleeps during the 20 min countdown; any button wakes it** (user) | The OLED dominates the "on" current; blanking it saves the battery. |
| **Deep sleep instead of a power cut, plus a hard switch J6** (user) | "Off" = MCU deep sleep (~15 µA). J6 is a real power cutoff in case sleep firmware misbehaves. |
| **All SMD on top for JLC; only the MCU and HC-12 on the bottom, hand-soldered** (user) | JLC assembles one side. The user has ATtiny3226-SU chips and the HC-12 modules. |
| **No ground plane anywhere** (user) | Keep copper away from the HC-12 antenna. GND is routed as wide tracks instead. |
| **Budget build: Basic parts and 0603 wherever possible** (user: 7-day use) | Only 2 Extended parts are assembled (TPS631000, its inductor), ~$6 in fees. |

## Power
| Part | Choice | Why |
|---|---|---|
| J1 | 2-pin 2.54 mm header, DNP (C492401) | Battery wires. Under the RIGHT button, so the battery→converter path is short (user). |
| Q1 | AO3401A P-FET, reverse-battery (Basic C15127) | D to the battery, S to VBAT, G to GND via R1 100k. Reversed cells → FET off, so the TPS631000 is saved. VGS(th) ≤ 1.3 V, so it is fully on down to ~1.6 V. Loss is mΩ, versus 0.3 V for a Schottky. |
| U2 | **TPS631000** buck-boost (C5219190) | 1.6–5.5 V in, 3.37 V out across the whole alkaline range (in > out when fresh, < out when low). ~8 µA Iq, true load disconnect when EN is low. This is the user's preferred part; the old remote used it too, but with EN miswired. |
| MODE | → GND | Auto PFM: best light-load efficiency (the board mostly sleeps). |
| EN | → **J6** → VBAT, R29 **510k** → GND (user: 100k leaked 32 µA) | J6 = off-board power switch. EN must never float (datasheet), so R29 holds it low when the switch is open. 510k ≈ 6 µA when closed. **J6 must be fitted (switch or jumper) or the board is dead.** |
| L1 | FXL0420 1 µH (C167203) | Datasheet range 0.7–1.3 µH; the same part was used on human_counter. |
| R2/R3 | 470k / 82k (Basic) | VOUT = 0.5 × (1 + 470/82) = 3.37 V. The datasheet requires the low-side resistor ≤ 100 kΩ (82k OK). 3.37 V is inside the HC-12 (3.2–5.5 V), OLED and ATtiny ranges. |
| C1/C2, C3/C4/C5 | 10 µF + 100 nF in; 2×22 µF + 100 nF out (0603 Basic) | The datasheet needs ≥ 4.2 µF effective in and ≥ 10.4 µF effective out at 3.3 V; 22 µF 0603 derates to ~8–10 µF each. |
| Layout | Hand-routed converter core | Short LX1/LX2. VIN caps at pins 4/5, VOUT caps at pin 1. The output-cap GND returns to U2 GND through 2 vias + a bottom track, so the FB trace isn't crossed. The block sits right of UP / above RIGHT, away from the HC-12 (user). |
| Battery loop | VBAT ~0.73 mm avg, GND 0.5 mm direct bottom track | User: thick path. Measured ~35 mΩ total, about 17 mV at 0.5 A. |

## Load switches (HC-12 and OLED powered separately)
| Part | Choice | Why |
|---|---|---|
| Q2 / Q3 | AO3401A, S = VCC, D = VRF / VOLED | HC-12 and OLED can each be switched off completely (0 µA). The old design could not switch them. |
| R4/R6 | 100k gate pull-up to VCC | OFF at reset and while the MCU pin is Hi-Z. |
| R5/R7 | 10k from the MCU pin | With the 100k the gate sits at 0.31 V when ON, so VGS ≈ −3.06 V (rated at −2.5 V). |
| C6/C8 | 47 nF gate–source | ~0.5 ms soft-start, so switching on the HC-12/OLED caps doesn't dip VCC and reset the MCU (that was the old remote's "power button doesn't work" fault). |
| C7 / C9 | 22 µF on VRF / 10 µF on VOLED | HC-12 TX bursts up to 100 mA. |

## MCU
| Part | Choice | Why |
|---|---|---|
| U1 | **ATtiny3226-SU** SOIC-20W (user's own chips), bottom, DNP | Same pinout family as the old ATtiny1616 the user knows. Two USARTs: HC-12 + debug at the same time. 32 KB flash. JLC stock was only 5, so the user hand-solders. |
| C10/C11 | 100 nF + 22 µF next to VDD | Decoupling; VCC is always on. |
| R14 | 470 Ω UPDI series | Standard UPDI protection. |
| J4 / J5 | UPDI 1×3 (V G U) / debug 1×4 (V T R G), DNP | Same pin order as the old board (user's programmer cable). Moved by the user to avoid overlaps. |
| Pin choice | USART0 default (PB2/PB3) → HC-12, USART1 default (PA1/PA2) → debug, TWI0 default (PB1/PB0) → OLED, PA6 (fully async) → power button, PA7 (AIN7) → battery | All default peripheral routes, so no PORTMUX surprises. PA6 is one of the Px2/Px6 pins that can wake from power-down on an edge. Verified against DS40002345B Table 3-1. |

## HC-12
| Part | Choice | Why |
|---|---|---|
| J2 | Custom footprint HC12_CASTELLATED_5P: 5 SMD pads, 2.54 mm pitch, 2.03 mm wide, **3.05 mm tall** | Pitch/position copied from the user's old working board (pad 1 VCC … pad 5 SET). Pads extended 1.27 mm outward past the module edge (user: castellations hard to solder). Bottom side mirrors y, so the footprint offset is −25 mil. |
| R9/R10/R11 | 1k series on TX, RX, SET | Limit current into an unpowered module (back-power) and ESD. |
| Antenna keepout | No wires/pours (regions on both layers) at the antenna end | The module's antenna end sits at the board's bottom edge (user). |

## OLED (0.96″ I2C module)
| Part | Choice | Why |
|---|---|---|
| J3 | 1×4 at the old JP1 spot: pins SDA, SCL, VCC, GND; from the button side GND VCC SCL SDA (labelled G V S D) | Matches the user's modules and the old board. The module mounts face-up over the board. |
| No parts under the OLED footprint (user) | | So the user can rework after the OLED is soldered. |
| R12/R13 | 4.7k I2C pull-ups to VOLED | Powered with the OLED, so there's no leakage when it is off. |
| Q3 group + pull-ups + LED on the top tab above J3 (user) | | Free space, out of the OLED footprint. |

## Buttons
| Part | Choice | Why |
|---|---|---|
| SW1–SW6 | TS-1187A 5.1 mm SMD (Basic C318884), 1.5 mm tall | JLC-assembled at the old positions. SW1 = PWR, SW2 UP, SW3 LEFT, SW4 OK, SW5 RIGHT, SW6 DOWN. A–B and C–D are internally joined (datasheet). |
| R16–R21 | **1k** series (user asked for debounce RC) | With a 10k series R and the MCU's 20 kΩ minimum pull-up, a pressed pin sat at 1.12 V, above VIL 1.01 V (bug found in review). 1k gives ≤ 0.31 V. |
| C13–C18 | 100 nF at the MCU pin | Debounce, ~1 ms. |
| R23–R28 | 10k external pull-ups to VCC (user) | Defined level without relying on firmware pull-ups; release τ 1 ms. |

## Battery sense, LED
| Part | Choice | Why |
|---|---|---|
| R8/R22 | 330k / 1M divider + C12 100 nF on PA7 | User: high-value divider, ADC on the internal 2.5 V reference. 3.3 V reads 2.48 V (full scale 3.32 V, covering fresh alkaline 2×1.6 V). ~2.4 µA. |
| D1/R15 | 3 mm THT LED (C85161, DNP) via 1k from PA3 | User: on/off indicator while the OLED sleeps. ~1.3 mA. Centre of the top tab, above J3 (user). |

## Silkscreen
- Pin letters on every header: J3 G V S D; J4 V G U; J5 V T R G; J1 + − on top and bottom.
- Names: BAT, UPDI, DBG, OLED. Bottom: HC-12, REMOTE V2 2026-10, WWW.DADABHAGWAN.ORG.
- Designators 0.9 mm. Bottom text mirror = false (JLCONE auto-mirrors), checked on the Gerber render (`docs/images/`).
