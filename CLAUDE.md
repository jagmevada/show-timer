# START HERE: remote_v2 (HC-12 show-timer remote)

**Status: FINAL / ORDERED (2026-10-11).** The JLC trial order went through to payment ($38).
**Git: github.com/jagmevada/show-timer (public, since 2026-11-02).**
- This folder is the working copy of branch `pcb`.
- `firmware/remote/` is a worktree of branch `remote`, which holds remote_v2 + panel_sim + `bringup/`.
- `firmware/panel/` is a worktree of branch `panel`: the panel firmware from jj-timer-remote timer-boilerplate 78cb7f6, with its history.
- The README on `main` maps every old timer repo.

**2026-10-31: boards RECEIVED, bring-up started.** Test firmware: `firmware/remote/bringup/` (PlatformIO, pymcuprog SerialUPDI on J4; debug J5 115200). Old firmware (old board pin map, HC-12 untested): `timer_remote_code/` (git clone of github.com/jagmevada/timer_remote_code).
**2026-11-01: real firmware `firmware/remote/` flashed and running on battery** (OLED, countdown, PWR on/off, VBAT). Panel protocol copied byte-for-byte from the deployed receiver `jj-timer-remote/` (branch timer-boilerplate); `firmware/remote/test/test_protocol.cpp` checks it. Pending: panel link test (HC-12 CH011 / 2400 / P8), VBAT_K trim with 2×AAA, sleep current.
No panel on the bench: `pio run -e panel_sim -t upload` turns a second remote_v2 board into a panel simulator (`src/panel_sim.cpp`, the panel's receive/ACK code copied). **2026-11-01: remote ↔ panel_sim link works (HC-12 commands + ACK).**
**2026-11-02:** 10 panel+remote sets exist, on addresses 1..10. A panel shows its address as 00:NN at power-up. Flash a remote for set N with `PLATFORMIO_BUILD_FLAGS=-DREMOTE_ADDR=N pio run -t upload`. A remote was flashed for address 2 to test against a real panel.
Bring-up gotchas: **J6 must be shorted** (else EN low, VCC 0, chip only runs from the programmer). Flash with **only U + G** on J4 when the battery is in (the V pin back-feeds the CP2102 and wedges it).
**Silk text (BAT, UPDI, DBG, OLED, OLED-module outline 27.3×27.8 mm; bottom HC-12, website, battery + / −) was added after that trial.** Order from `release/` (FINAL) so the board has it. Copper is identical.

| What | Where |
|---|---|
| Final project (import in JLCONE as a new project) | `release/remote_v2_2026-10-11_FINAL.epro2` |
| Fab / assembly files | `release/remote_v2_2026-10-11_GERBER.zip`, `…_BOM.csv` (18 lines / 58 parts), `…_CPL.csv` |
| Why each circuit and part | `DESIGN_RATIONALE.md` |
| Everything firmware must do + verified pin map | `FIRMWARE_NOTES.md` |
| How to order + loose LCSC parts + bring-up | `ORDERING.md` |
| Schematic source of truth (66 parts, every pin) | `work/gen_spec.py` → `work/spec/expected.json` |
| Old remote failure analysis (why v2 exists) | `docs/old_remote/DIAGNOSIS.md` |
| User preferences / rules / EasyEDA API notes | `docs/WORKING_WITH_JAGME.md`, `docs/PCB_DESIGN_RULES.md`, `docs/EASYEDA_API_NOTES.md`, `.claude/skills/pcb-design-rules/` |
| Gerber silk renders (mirror check) | `docs/images/` |
| Intermediate backups / early planning notes | `archive/` |
| User's old Eagle design (git clone, don't edit) | `timer_remote/` |

JLCONE project **remote_v2** (uuid 256f3e6816a44d998fc1c5ea3f9f5d1a). Schematic page 2d1452873375e6bd; PCB 47954f363bc20433.

## The board in one paragraph
2-layer, 36 × 76.5 mm, same outline and button positions as the user's old remote (fits the old case).
2×AAA alkaline → J1 → Q1 AO3401A reverse-battery FET → U2 TPS631000 buck-boost → VCC 3.37 V.
EN goes through **J6, an off-board power switch** (R29 510k pull-down).
U1 ATtiny3226-SU (SOIC-20W, user's chip, hand-soldered, **bottom**) stays powered and deep-sleeps.
Q2 switches the HC-12 (VRF); Q3 switches the 0.96″ I2C OLED (VOLED).
Six TS-1187A buttons have 1k series + 100 nF + 10k pull-up each. 3 mm status LED. Battery sense is a 330k/1M divider for the 2.5 V ADC reference.
HC-12 module: castellated edge, on the **bottom**, antenna toward the bottom edge. No ground plane anywhere (user: HC-12 antenna).

## Final verification (2026-10-11)
- Schematic: 66 parts; netlist = spec pin-for-pin; no single-pin nets.
- PCB: 66 parts. Every pad on the spec net. Channel IDs intact. DRC 0. 39 nets routed, 48 vias, no track < 0.25 mm.
- BOM 18 lines / 58 parts (66 − 8 DNP): every line has an LCSC number, no duplicate lines (a duplicate C21190 line once made JLC show 0 qty; fixed by setting Value).
- Bottom silk checked on the real Gerber: it reads backwards in top view = correct (HARD RULE 1). The outline arcs were checked on the Gerber too.
- ATtiny3226 pins checked against DS40002345B Table 3-1 (SOIC-20). HC-12 TX/RX cross-check, I2C, UPDI, debug all OK.
- Battery current loop: ~35 mΩ (VBAT ~0.73 mm avg, GND return 0.5 mm direct track).

## History (2026-10-11, one session)
1. The old remote (Eagle, timer_remote repo) was diagnosed: TPS631000 EN tied low / SHDN on MODE; U4 P-FET backwards (fed VCC from VBAT); no VDD cap → power-button resets.
2. The user chose a fresh board. Changes through the session:
   1. ATtiny3226 (user had them).
   2. DC-DC always on, then switched by J6.
   3. HC-12 / OLED individually switched.
   4. No crystal: internal oscillator; the ACK carries the panel time.
   5. Debounce RC added (1k, not 10k: logic-LOW margin), then 10k pull-ups.
   6. VBAT divider (user: 1 M class, 3.3 V → 2.5 V).
   7. Taller HC-12 pads (user: castellations hard to solder).
   8. The user placed many parts by hand (top tab, J4/J5, J1 under the RIGHT button). The DC-DC is right of UP / above RIGHT, away from the HC-12.
3. The schematic page is not tidy: the copilot beautify fails on this page and the API can't move net labels. Functional-block boxes were drawn and the drawing frame removed. Tidy by hand if wanted, then re-verify the netlist.
