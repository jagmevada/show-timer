# remote_v2: how to order (final 2026-10-11)

Order from JLCONE project **remote_v2**, or import `release/remote_v2_2026-10-11_FINAL.epro2`.
Fab files: `release/remote_v2_2026-10-11_GERBER.zip` (2 layers). BOM: `…_BOM.csv` (18 lines / 58 parts). CPL: `…_CPL.csv`.
The trial order went to payment at **$38** (2026-10-11). The final files add silk text only; copper is identical. Re-export or upload the FINAL files.

## 1. JLC options
- PCB: 2 layers, 1.6 mm, 36 × 76.5 mm (shaped outline, copied from the old remote). Check the outline arcs in the Gerber viewer.
- **Tented vias** (a few vias sit under the HC-12 module on the bottom).
- PCBA: **top side only.** Extended parts: TPS631000DRLR C5219190, FXL0420-1R0-M C167203. Everything else Basic.
- **Expected warning** ("won't be assembled / not in BOM"): **J1, J2, J3, J4, J5, J6, U1, D1.** All intentional; continue.
- In the placement preview check:
  - U2 (TPS631000) pin 1;
  - Q1–Q3 orientation;
  - the switches.
  - Bottom labels (U1/J2, website, battery + −) must look mirrored from the top.

## 2. AFTER paying JLC: loose parts on LCSC ("combine with JLCPCB order")
| Ref | Part | LCSC | Per board |
|---|---|---|---|
| J1 | 1×2 header 2.54 mm (battery) | **C492401** | 1 |
| J6 | 1×2 header 2.54 mm (power switch) | **C492401** | 1 |
| J3 | 1×4 header (OLED) | **C2691448** | 1 |
| J5 | 1×4 header (debug) | **C2691448** | 1 |
| J4 | 1×3 header (UPDI) | **C2937625** | 1 |
| D1 | 3 mm LED (Everlight 204-10SYGD) | **C85161** | 1 |
Check the MOQ (headers are often sold in 5s or 10s).
The user supplies: **ATtiny3226-SU** (SOIC-20W), **HC-12** module, **0.96″ I2C OLED**, power switch + wires, 2×AAA holder, case.

## 3. Hand assembly (bottom first)
1. **U1 ATtiny3226-SU on the BOTTOM**, pin 1 at the silk dot/notch (rotated 180°).
2. **HC-12 on the BOTTOM**, castellated edge on the J2 pads, **antenna toward the board's bottom edge**. Pad 1 = VCC (pin-1 bar), pad 5 = SET.
3. J1 battery: + = pin 1 (BAT_IN), − = pin 2. Marked + − on top and bottom silk.
4. **J6 power switch (or a jumper). Without it the converter is OFF.**
5. D1: long lead (anode) in the "+" pad; the flat outline = cathode.
6. J4 UPDI / J5 debug headers if needed; J3 OLED last (nothing is placed under it).

## 4. Bring-up (power)
1. Battery in, J6 closed: VCC (J4 pin V) = **3.37 V**. Open J6 → VCC = 0.
2. Current with the MCU unprogrammed: a few mA at most (no shorts).
3. Program via UPDI, then follow `FIRMWARE_NOTES.md` §6.
