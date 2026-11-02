# Remote v2: connection table (for review, 2026-10-11)

**Budget build (user):** 7-day use this year. Basic parts and 0603 everywhere possible.
Only 3 Extended parts: U1 ATtiny3226 C3234898, U2 TPS631000 C5219190, L1 FXL0420 1 µH C167203. No Basic boost IC or 1 µH power inductor exists.

New JLCONE project. The old board (remote_1) is used only for its outline and the button/header positions (`work/old_board_geom.md`).
**All SMD parts on TOP (JLC assembly).** THT parts are DNP and hand-soldered: 6 buttons, OLED header, HC-12 header, battery, UPDI, debug. They go on the back if the top is full.
No crystal: internal oscillator; the panel's ACK carries its remaining seconds.

## Power
| Ref | Part | LCSC (class) | Pins → net |
|---|---|---|---|
| J1 | 2-pin header 2.54 (battery 2×AAA wires), DNP | loose | 1 BAT_IN, 2 GND |
| Q1 | AO3401A (reverse-battery) | C15127 (Basic) | G → R1, D BAT_IN, S VBAT |
| R1 | 100k | C25803 (Basic) | Q1.G – GND |
| U2 | TPS631000DRLR buck-boost | C5219190 (Ext) | 1 VOUT VCC, 2 LX2, 3 LX1, 4 VIN VBAT, **5 EN VBAT (always on)**, **6 MODE GND (PFM)**, 7 GND, 8 FB |
| L1 | FXL0420-1R0-M 1 µH | C167203 (Ext) | LX1 – LX2 |
| C1 | 10 µF 0603 10 V | C19702 (Basic) | VBAT – GND (U2 in) |
| C2, C3 | 22 µF 0603 | C59461 (Basic) | VCC – GND (U2 out; datasheet needs ≥ 10.4 µF effective) |
| R2 | 470k | C23178 (Basic) | VCC – FB |
| R3 | 82k | C23254 (Basic) | FB – GND. VOUT = 0.5 × (1 + 470/82) = 3.37 V |
| C4 | 100 nF | C14663 (Basic) | VCC – GND at U1 |
| C5 | 22 µF 0603 | C59461 (Basic) | VCC – GND at U1 (holds VCC up when Q2/Q3 switch on) |

## Load switches (P-FET, ON when MCU pin low; 100k pull-up = OFF in reset/sleep)
| Ref | Part | Pins → net |
|---|---|---|
| Q2 | AO3401A C15127 | S VCC, D **VRF**, G RF_G |
| R4 / R5 / C6 | 100k C25803 / 10k C25804 / 47 nF C1622 | R4 VCC–RF_G; R5 RF_EN (U1) – RF_G; C6 VCC–RF_G (soft-start ≈ 0.5 ms) |
| C7 | 22 µF 0603 C59461 | VRF – GND (HC-12 TX bursts up to 100 mA) |
| Q3 | AO3401A C15127 | S VCC, D **VOLED**, G OLED_G |
| R6 / R7 / C8 | 100k / 10k / 47 nF | same as Q2 with OLED_EN / OLED_G |
| C9 | 10 µF 0603 C19702 | VOLED – GND |

## MCU U1: ATtiny3226-SU (SOIC-20) C3234898 (Ext). Pins verified in DS40002345B Table 3-1 / PORTMUX
| Pin | Port | Net | Function |
|---|---|---|---|
| 1 | VDD | VCC | |
| 2 | PA4 | BTN_UP | button to GND, internal pull-up |
| 3 | PA5 | BTN_DOWN | ″ |
| 4 | PA6 | BTN_PWR | power button. **Fully-async pin, wakes from power-down** |
| 5 | PA7 | VBAT_SNS | AIN7: VBAT → R8 100k → pin, C10 100 nF to GND |
| 6 | PB5 | RF_EN | Q2 gate drive (low = HC-12 on) |
| 7 | PB4 | OLED_EN | Q3 gate drive (low = OLED on) |
| 8 | PB3 | HC_RX | USART0 RxD (default) ← HC-12 TXD |
| 9 | PB2 | HC_TX | USART0 TxD (default) → HC-12 RXD |
| 10 | PB1 | SDA | TWI0 SDA |
| 11 | PB0 | SCL | TWI0 SCL |
| 12 | PC0 | BTN_LEFT | ″ |
| 13 | PC1 | HC_SET | HC-12 SET (low = AT mode) |
| 14 | PC2 | BTN_RIGHT | ″ |
| 15 | PC3 | BTN_OK | ″ |
| 16 | PA0 | UPDI | programming |
| 17 | PA1 | DBG_TX | USART1 TxD (default) |
| 18 | PA2 | DBG_RX | USART1 RxD (default) |
| 19 | PA3 | SPARE | **unused: firmware drives it low** |
| 20 | GND | GND | |

## Connectors / THT (DNP, hand-soldered)
| Ref | What | Pins (same order as the old board) |
|---|---|---|
| SW1–SW6 | 6 × tactile 6×6 THT (old footprint SW_55MM), at the old positions | each: 1,2 = BTN_x; 3,4 = GND. SW1 = PWR (old S1 spot) |
| J2 | HC-12 module header 1×5 | 1 VRF, 2 GND, 3 RXD ← HC_TX (via R9 1k C21190), 4 TXD → HC_RX (via R10 1k), 5 SET ← HC_SET (via R11 1k) |
| J3 | OLED 0.96″ I2C header 1×4 | 1 SDA, 2 SCL, 3 VOLED, 4 GND (old JP1 order; **check against your module**). R12/R13 4.7k C23162: SDA/SCL → VOLED |
| J4 | UPDI 1×3 | 1 VCC, 2 GND, 3 UPDI (via R14 470 Ω C23179) |
| J5 | Debug UART 1×4 | 1 VCC, 2 DBG_TX, 3 DBG_RX, 4 GND |

Series 1k on the HC-12 lines limits back-feed current into a powered-off module. Firmware also drives those pins low before turning Q2 off.

## Current budget
- **Off:** U2 Iq ≈ 8 µA at VOUT (≈ 14 µA from the cells) + ATtiny power-down < 1 µA + FET leakage. About 15 µA.
- **On:** OLED ≈ 10 mA + MCU ≈ 2 mA. About 15 mA from the cells, roughly 65 h on alkaline AAAs.
- **Per command:** HC-12 ≈ 0.3–0.5 s.
