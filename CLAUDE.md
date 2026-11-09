# START HERE: remote_v2 (HC-12 show-timer remote)

**Status: FINAL / ORDERED (2026-10-11).** The JLC trial order went through to payment ($38).
**Git: github.com/jagmevada/show-timer (public, since 2026-11-02).**
- This folder is the working copy of branch `pcb`.
- `firmware/remote/` is a worktree of branch `remote`, which holds remote_v2 + panel_sim + `bringup/`.
- `firmware/panel/` is a worktree of branch `panel`: the panel firmware from jj-timer-remote timer-boilerplate 78cb7f6, with its history.
- The README on `main` maps every old timer repo.

**2026-10-31: boards RECEIVED, bring-up started.** Test firmware: `firmware/remote/bringup/` (PlatformIO, pymcuprog SerialUPDI on J4; debug J5 115200). Old firmware (old board pin map, HC-12 untested): `timer_remote_code/` (git clone of github.com/jagmevada/timer_remote_code).
**2026-11-01: real firmware `firmware/remote/` flashed and running on battery** (OLED, countdown, PWR on/off, VBAT). Panel protocol copied byte-for-byte from the deployed receiver `jj-timer-remote/` (branch timer-boilerplate); `firmware/remote/test/test_protocol.cpp` checks it. **2026-11-02: works wirelessly with a REAL deployed panel (address 2, CH012).** Pending: VBAT_K trim with 2×AAA, sleep current, SEC_MS drift check.
**Panel sync (remote only, the panels can't be reflashed):**
- The panel never sends its time, so the remote mirrors the panel's state (count, HOLD, NEXT SHOW at 00:00) through sleep AND PWR-off.
- Only a J6 / battery power loss loses sync: the remote then starts paused, showing "NOT SYNCED".
- After a panel power-up, make the first command OK+RIGHT: the panel's first command always loads the panel's own saved time.
- A battery swap / J6 off loses sync, because the onboard VCC caps (~35 µF effective) last only ~2–3 s at ~9 µA. Recovery: freeze and copy (OK → copy the panel time → OK).
- Considered and SKIPPED by the user (2026-11-02): a 0.22 F supercap + 22 Ω across J4 V–G would hold the chip ~4.5 h through a swap.
No panel on the bench: `pio run -e panel_sim -t upload` turns a second remote_v2 board into a panel simulator (`src/panel_sim.cpp`, the panel's receive/ACK code copied). **2026-11-01: remote ↔ panel_sim link works (HC-12 commands + ACK).**
**2026-11-02:** 10 panel+remote sets exist, on addresses 1..10. A panel shows its address as 00:NN at power-up. Flash a remote for set N with `PLATFORMIO_BUILD_FLAGS=-DREMOTE_ADDR=N pio run -t upload`. A remote was flashed for address 2 to test against a real panel.
**HC-12 modules (2026-11-03), important:**
- The 4 NEW HC-12s (same AT+RX/AT+V as the old ones: FU3, 2400, C012, +20 dBm, v2.6) are an RF-incompatible batch: they link new<->new only, never with the old/genuine modules in the panels.
- Result: 8 remotes work, all with old modules. 2 boards are waiting for compatible modules.
- Per-remote flashing (2026-11-09: address menu replaces the FIRSTTIME two-step):
  1. Build once: `pio run` (default address 1; `-DREMOTE_ADDR=N` changes the default).
  2. Flash each remote: `pymcuprog write --erase --verify -t uart -u COM4 -d attiny3226 -c 230400 -f .piouild\ATtiny3226irmware.hex`.
     - Use the working CP2102; its COM number depends on the USB port.
     - Batteries + J6, U + G only.
  3. Set the address on the remote: UP + DOWN 3 s, pick 1..15, OK. This saves it to EEPROM and sets up the HC-12 on channel 10+N; it survives power cycles.
     A flash erases the EEPROM, so open the menu and press OK once after every flash.
- Diagnostic tools: J5 log (115200), env `radiotest` (board-to-board link test), `hc12_firsttime/` (Arduino IDE sketch).
- Panel (branch `panel`): build with `-DMYADDRESS=N [-DFIRSTTIME]`. The deployed address-2 panel firmware is backed up in `firmware/panel/backups/`.
**TODO (open items, 2026-11-09):**
- [ ] **Board with `HC12 no reply AT`** (flashed 2026-11-09 via COM4; it fails in the address menu): the HC-12 does not answer the first AT.
  - Measure in the ~3 s "setting HC-12..." window: pad1 VCC ~3.3 V? pad5 SET ~0 V? RXD/TXD alive? Check R9/R10/R11 and the Q2 joints.
  - Or swap in a known-good module (fault moves with the module = bad module, else the board).
  - Confirm it runs on batteries, not programmer V.
- [ ] **Address menu (UP+DOWN 3 s)**: confirm on hardware that after setting an address, OK at that panel gives `HOLD: OK`.
- [ ] **2 boards need compatible HC-12s.** The new batch is RF-incompatible with the panels' genuine modules.
- [ ] **Panel address 2**: make sure its original (old) HC-12 is back. It had a new module during the 2026-11-03 test. Backup firmware: `firmware/panel/backups/`.
- [ ] **1 remote freezes after waking from sleep** (9 of 10 OK): catch it on the J5 log (CH340 on J5, 115200); the last line shows the step.
- [ ] **Programmer COM3 (CP2102 serial 0001) is broken**: no TX->RX echo (resistor/diode/U lead or the adapter). Use the other CP2102 (COM4/COM9, numbered by USB port).
- [ ] Decide: set the EESAVE fuse so a reflash keeps the address + saved time (one pymcuprog fuse write per remote).
- [ ] Still pending from bring-up: VBAT_K trim with 2xAAA, sleep current, SEC_MS drift vs the panel.
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
| 3D assembly for case design (2026-11-07): all 66 parts incl. DNP + HC-12 + approximate OLED (no vendor CAD; knobs at the top of the script). OLED PCB 1.2 mm above the board (user's build), J3 = bare pins | `cad/remote_v2_assembly.step` ← `cad/make_assembly.py` (cadquery-ocp) from `cad/remote_v2_easyeda.step` (EasyEDA export) |
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
