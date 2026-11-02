---
name: pcb-design-rules
description: Jagme's standing rules for every Bhojanshala PCB (EasyEDA Pro / JLCONE + easyeda-copilot) — workflow, parts, power, layout, layer use, trace sizing, silkscreen and verification. Use whenever designing, laying out, routing, reviewing or preparing to order any PCB.
---

# PCB design rules (user's standing inputs)

These rules come from the user's own instructions on the RS485 load-cell node (2026-10) and human_counter (2026-10).
Apply them to every new board without being asked. A rule changes only when the user says so; record the change here.

Project files: `D:\Project\Bhojanshala\wscale\PCB\` (design/, docs/datasheets/, backups/).
Each board has a `design/<board>/CLAUDE.md` with its own state and API gotchas. Read it before touching that board.

## HARD RULES (never break)
1. **FINAL-STAGE CHECK: ensure ALL text on the bottom layer is mirrored.** Before the board is called finished or ordered, every bottom-silkscreen text must appear mirrored in the top view.
   That covers pin letters, connector and button names, the website, and any text added later by me or the user.
   - Check it on the real output: render the bottom-silk Gerber (`Gerber_BottomSilkscreenLayer.GBO`) from the top, or have the user look at the bottom layer in top view. The flag alone is not proof.
   - Fix any that read normally, then re-check.
   - API note: JLCONE auto-mirrors the bottom layer, so bottom text needs `mirror = false`. `mirror: true` makes it print backwards.
2. Part designators are 0.9 mm minimum, visible, and never on a footprint or pad.
3. Two classes of GND via:
   - **THERMAL vias** (for heat-dissipating parts: switching converters, chargers, LDOs, ESP32-class modules/MCUs, power FETs) use the NORMAL DRC clearance (1×).
     They're functional and high priority: plan room for them during placement/routing and put as many as fit close to the part's GND/EP copper.
   - **STITCHING vias** are low priority and go in LAST, ≥2× DRC spacing from pads, tracks and holes. Skip a spot if it doesn't fit, and never move routing or placement for them.
4. Read the current geometry before editing anything the user may have changed. Back up (`.epro`) before risky edits.
5. **UNUSED-PIN AUDIT: every unconnected pin must be proven genuine, not a mistake.** For each floating pin (ERC's "Pins floating" list, or every pin with no net):
   - Read its NAME and function from the datasheet. Never judge by the pin number alone.
   - Classify it and write the reason in the board docs (a table: pin, name, why it's open):
     - **OK open:** datasheet says NC, leave floating, or test pin (e.g. DW01A TD); unused interface pins (USB-C SBU); optional reference pins the datasheet allows open (USBLC6 VBUS).
     - **Strapping pins:** OK open only if the internal pull gives the required boot default. Never let them be pulled the wrong way.
     - **Spare GPIOs:** OK open on the board, but note that firmware must drive them or enable a pull (low-power leakage).
     - **NEVER open unless a specific reason is documented:** power, GND, enable/reset, sense/feedback, RF/antenna, exposed pad. Antenna pins may be open only when the module variant routes RF to its own connector; record which variant, and that the others must not be substituted.
   - Any pin that can't be justified this way is a MISTAKE: fix it before ordering.
   - Re-run the audit whenever parts or the schematic change.

## 1. Workflow
- Work section by section:
  1. Build a connection table from the datasheet pin tables.
  2. The user reviews the table.
  3. Draw one page.
  4. Script-diff the netlist against the table and run ERC.
  5. Only then move to the next section. Never do everything at once.
- Verify EVERY IC/MCU pin against its datasheet pin table before wiring or ordering. A swapped SDA/SCL pair nearly cost a $350 order.
  The user's standard is "pl pl pl dont make any mistake".
- **"commit"** = save an EasyEDA checkpoint AND export an `.epro` backup to `wscale/PCB/backups/<board>_<date>_<tag>.epro`.
  Back up before every risky edit too.
- Swapping a part in the schematic (delete + place, e.g. 10k → 4.7k) makes Import Changes add it as a NEW PCB part, parked off-board, and drop the old one.
  Before the import, record the old part's x/y/rotation and designator position. After it, move the new part back, then check DRC and netcheck.
- **Backups / project files must use the `.epro2` extension** (`sys_FileManager.getProjectFile` output = V3 format, same as JLCONE "Save as .epro2").
  JLCONE imports a `.epro` file as the OLD format and creates an EMPTY project (found 2026-10-11).
  The `importProjectByProjectFile` API also does nothing in JLCONE, so the user imports by hand.
- Mixed-design panels: EasyEDA's panel tool only repeats one design.
  - Build one merged PCB with a script instead (`design/panel_trial/work/build_panel.py`): copies get prefixed nets and +100/+200 designators.
  - The user imports the `.epro2`. Then verify every pad's net and position against the source boards.
- Panel separation: V-cut needs parts ≥1 mm (tall ≥2 mm) from the cut line and full-length straight cuts. With edge connectors, use mouse bites.
  Use 2 mm milled gaps, 0.55 mm holes at 0.85 mm pitch on copper-free edge stretches, and keep every cut-out ≥2 mm.
- **Every design gets a self-contained folder** `wscale/PCB/design/<board>/` (user, 2026-10-11). It holds:
  - `CLAUDE.md`, starting with a START HERE section.
  - `DESIGN_RATIONALE.md` (why each circuit and part) and `FIRMWARE_NOTES.md` (software duties and purpose).
  - `ORDERING.md` (JLC options, expected warnings, loose LCSC connectors).
  - `release/` (locked `.epro2` + Gerber + BOM + CPL).
  - `docs/` (datasheets, WORKING_WITH_JAGME, PCB_DESIGN_RULES, EASYEDA_API_NOTES).
  - `.claude/skills/pcb-design-rules/` (a copy of this skill).
  Keep them current. After editing this skill, re-copy it into every design folder. Index: `wscale/PCB/CLAUDE.md`.
- NEVER run schematic beautify. The user arranges schematics by hand.
- After ANY copilot circuit edit (extract_circuit with rm_components / external_connect), re-apply DNP (Add into BOM = no) and edited Values, then check them. The tool rebuilds parts and resets these (usb_dmx, 2026-10-15). Also re-run the duplicate-wire-label hide script (one label per connection).
- Schematic notes and any text added by script: re-runs must never stack copies.
  Before finishing, check every page for duplicate texts and leftover test strings (`work/sch_text_audit.js`), and remove extras (`work/sch_dedupe_notes.js`).
- Never delete zero-length wires: where a flag or port touches a pin they ARE the connection.
- Use one short, intuitive name per net, set by one net port. Never add a second label at the same connection.
- After the user edits anything by hand, re-export the netlist and re-verify. A net label dragged in JLCONE detaches from its wire.
- The user sometimes works remotely ("rc mode") and can't click EasyEDA dialogs then. Import Changes needs a user click.
- Don't stall. When a step is blocked, finish everything else and report clearly.

## 2. Parts
- JLCPCB Basic parts first, then the cheapest. Resistors: 0603 1%. Caps: 0603 X7R at the highest voltage available.
  Extended parts are OK if the extended fee is ≤ $2.
- **Check JLC assembly stock by script before choosing an IC** (found 2026-10-19: the SC8701 shows LCSC stock but has 0 at JLC). Script:
  `curl -s -X POST https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList -H "Content-Type: application/json" -A "Mozilla/5.0" -d '{"keyword":"<MPN>","currentPage":1,"pageSize":5}'`
  In the JSON, `data.componentPageInfo.list[]` gives `componentCode` (C#), `stockCount` and `componentLibraryType` (base / expand).
- **Prefer highly integrated parts that need few external components** (user, 2026-10-19). Compare the TOTAL cost of each circuit option, not just the main IC: count every external FET, diode, R/C and TVS, the number of Extended part numbers ($3 each per order), and board area. Fewer parts also means fewer Extended fees and less placement and routing.
  Example: the MX74700T ideal-diode controller ($0.35, one N-FET + one cap) beat the LM5050-1, which needs extra diodes for reverse polarity, and the TI LM74700 at 7× the price. Chinese equivalents on LCSC (Maxinmicro, SCT, etc.) are fine once checked against their own datasheet.
- Default picks for battery / USB-C boards:

  | Function | Part | LCSC |
  |---|---|---|
  | USB-C input OVP | ETA7014 | C7465500 |
  | 1S Li-ion charger with power path | ETA6003 | C5455585 |
  | 3V3 / 5V rails (buck-boost) | TPS631000 | C5219190 |
  | 3S Li-ion charger (7.5–28 V in, ≤5 A; pair with CH224K PD trigger for USB-C; pack needs a balancing BMS) | CN3703 | C34452 |
  | Power inductor | FXL0420 1 µH (1.5 µH for the ETA6003) | |
  | USB-C connector | TYPE-C-31-M-12 | C165948 |
  | Converter output caps | 2× 10 µF 0805 | C380347 |
  | Decoupling | 100 nF | C14663 |

- External-supply boards: input chain is Schottky (B5819W) → 0.1 Ω shunt + INA180A3 → one PTC per regulator branch → regulator.
  The INA180 must sit after the diode.
- Cost rules (user, 2026-10-11):
  - JLC charges $3 per Extended part number. Use Basic R/C wherever a drop-in exists, and check stock.
  - **Through-hole connectors are never JLC-assembled.** Mark them DNP, buy them loose from LCSC in the same checkout and hand-solder.
    This avoids the Extended fees, the THT-assembly fee, and JLC's footprint-drawing delay.
  - Optional ESD on an internal wire header can be DNP when series R + RC already protect the pin.
- Battery boards with on-board DW01A + FS8205A protection (user, 2026-10-10):
  - Add a 1206 2 A PTC (BSMD1206-200-6V, C883134) in the battery-negative lead, right at the battery connector. The user rejected 1812 as too big.
  - Keep DW01A VSS, its cap and the FET source on the board side of the PTC. Then the PTC shifts neither the overcurrent sense nor the overcharge reading.
  - The DW01A + FS8205A do NOT protect against a reversed battery. That needs a separate blocking FET: a low-side N-FET (drain to cell−, gate to VBAT).
- Power switch: use a 2-pin 2.0 mm straight header for an off-board mechanical (red) switch, never a PCB switch.
  Add ESD (ESD5Z) + series R + RC debounce to protect against switching transients.
  On the ETA6003, use ENPPB for ship mode.

## 3. Board layout
- All parts on the TOP side. JLCPCB assembles the top only.
- Connectors go on board edges, grouped as the user directs:
  - USB-C and the sensor (LiDAR) connectors share one edge.
  - All other headers go together on the opposite edge, in rows if needed.
  - Buttons go on a side edge, never mixed into the connector rows.
- Main module (MCU / Wio-S3) near the centre.
- Make the board as small as possible.
- Mounting / stack holes: plated free pads, with NO parts and NO vias inside a 6 mm diameter around each hole. Everything else can be tight.

## 4. Layers and copper
- 4-layer (JLC04161H-7628, 1.6 mm, provisional). All boards in one panel must share layer count and thickness.
- Don't waste both inner layers on ground:
  - INNER_1 = solid GND plane, kept unbroken under radios/modules for return paths.
  - INNER_2 = POWER: SYS plane, plus extra copper fill regions for other high-current rails (e.g. a 3V3 strip from regulator to module).
  - Use inner copper for routing too when it is needed.
- Use copper fill regions ("templates") for high-current paths wherever tracks are too narrow.

## 5. High-current trace sizing (must check on every board)
- Size every high-current path by IPC-2221 (1 oz outer, 10 °C rise):

  | Current | Minimum width |
  |---|---|
  | 0.5 A | 0.12 mm |
  | 1 A | 0.30 mm |
  | 1.5 A | 0.53 mm |
  | 2 A | 0.78 mm |

  Inner 0.5 oz needs about 2× the width. A 0.3 mm via carries about 1 A, so use 2+ vias per amp.
- Run a source→load path analysis per rail (lowest-resistance route, plus inner planes). Report R, drop at worst-case current, and the narrowest segment.
  Target drop: ≤ 30 mV per rail.
- Rails to check:
  - VBUS / VIN: charge plus load current.
  - Charger SW / SYS / VBAT: the charge current.
  - Battery-negative chain: BAT_N, protection FETs, shunt and its GND return.
  - Regulator outputs to the main module.
  - Sensor 5 V.
- Where outer layers can't fit the width, add a parallel path or fill on an inner power layer. Narrow necks are OK only at IC pins and ≤1.3 mm long.
  Count the current split across paired pins.

## 5a. Exposed pads (QFN/module GND pads)
- **FOLLOW THE DATASHEET's land pattern for exposed pads.**
  If the datasheet layout shows thermal vias in the EP (e.g. ETA6003), keep them: 0.3 mm vias, top exposed through the pad opening, bottom TENTED so solder doesn't wick away.
  Only when the datasheet shows NO in-pad vias on a tiny QFN/DFN do we leave them out and put thermal vias just outside the package.
  (2026-10-10: I removed U3's EP vias on a lift-off concern; the datasheet shows them, so the user restored them. Datasheet wins.)
- Also follow the datasheet's GROUND guidance, e.g. ETA6003: pins tied to GND (ENB, NTC, USB_DET) must go to GND pin 10, NOT to PGND pin 5 / the power-ground copper.
- Larger exposed pads (modules like Wio-S3, big QFNs) get a via grid in the pad, as below. Always follow the part's datasheet layout when it shows one.
- Every exposed/thermal pad that is allowed in-pad vias gets a grid of GND vias (about 1.0–1.2 mm pitch) down to the GND plane, for soldering, grounding and heat.
- Vias in pads: OPEN on top (`solderMaskExpansion.topSolderMask` = +2 mil) so solder wets the whole pad. Tented on the bottom (−1000 mil) so solder doesn't drain through.
- The global rule tents all vias, so EP vias need this per-via override.
- Keep other nets' bottom tracks out from under the EP so the grid can be complete.

## 5a1. THERMAL vias (high priority, functional)
- For every part that dissipates heat: switching converters/chargers (ETA6003, TPS631000…), LDOs, ESP32-class modules/MCUs, power FETs, current shunts.
- Place GND vias as close as possible to the part, on the GND copper connected to its GND pins / exposed pad.
  - Inside the EP only for large pads; for small QFNs (≤3×3 mm) keep them OUTSIDE, see 5a.
- Clearance: the NORMAL DRC spacing (1×, 0.152 mm + a small rounding margin) to other nets' tracks, vias and pads. Spacing between GND vias ≥0.8 mm.
- Reserve room for them DURING placement and routing. Keep other nets' bottom/inner tracks out of the area around hot parts so the vias fit. Add as many as fit; aim for 6–8+ around each converter.

## 5a2. GND stitching vias (low priority, last step)
- Spread GND stitching vias over the board (about 3 mm grid), so no large GND copper area hangs on a thin neck.
- Only place a via where it lies fully inside BOTH the top and bottom GND fills.
- Keep each via ≥ 2× the DRC spacing (2 × 0.152 = 0.305 mm) from ANY track, via/hole and PAD on all layers.
  Vias UNDER a part body are fine, e.g. under modules. Only pads need the margin.
  Also keep ≥1.5 mm from other GND vias and stay well clear of mounting holes, the board edge and SLOT regions (USB-C shell slots; +0.6 mm extra on THT/slotted pads).
- Heat-dissipating parts DON'T use this stitching margin; they get THERMAL vias (section 5a1).
- Stitching is ALWAYS the final step, after routing is finished, so it never constrains routing optimisation.
- If a spot doesn't meet every margin, SKIP that via. Never shrink the margins, never squeeze or nudge a via in, never move anything to make room. Fewer vias is fine.
- NEVER move routing or placement for stitching; it's a low-priority job.
- Script: `human_counter/work/layout2/stitch_gnd.js` (dry run, then apply).
- Re-run DRC afterwards; it caught one via near a USB-C slot.

## 5a3. Thermal relief and mounting holes
- Mounting holes: plated, tied to GND, direct connection.
- Hand-soldered THT headers: thermal relief (spokes) on their GND pins ONLY. SMD pads and vias stay direct.
  - Do it PER PAD: `pcb_PrimitivePad.modify(pinId, { heatWelding: {connectionMethod:"Divergent", divergenceAngle:45, divergenceLineWidth:mil(0.3), divergenceSpacing:mil(0.3)}, rotation: <current pad rotation in DEGREES> })`.
  - ALWAYS pass the pad's rotation in the same call. Without it EasyEDA rewrote 180° pads to 3.14159 (radians in a degrees field) and tilted square pads.
  - Afterwards check every pad rotation is a multiple of 90°. `getState_HeatWelding()` reads back `{}` even when it worked; confirm on the render.
  - Do NOT switch the copper-zone `multiLayerPadModel` to thermal board-wide: it also hits vias and breaks up the inner planes.
- If thermal relief brings another net's inner fill too close to a pin, add a small NO_POURS region (region rule type 7) on that inner layer around the pin.

## 5b. Routing and placement style (learned from the user's own edits, human_counter 2026-10-10)
- Grid: snap tracks, part positions and designators to a 5-mil (0.127 mm) grid. Nudge neighbouring parts in 0.13/0.25 mm steps so rows line up.
- Angles: only 0°, 45° and 90° segments. No odd angles, no near-zero-length stubs.
- Widths:
  - Signal tracks are 0.254 mm (10 mil) MINIMUM. Never leave 0.2 mm tracks.
    Exception (user, dmx_node 2026-10-22): that board's DRC minimum AND default are 8 mil (0.2032 mm), so two tracks fit between SOT-23 pins 1 and 2. Still use 10 mil or more where there is room.
  - Feedback / sense lines: 0.25–0.3 mm.
  - Local power branches (3V3/5V to caps, sense taps of a power rail): 0.4–0.5 mm, not 0.25.
  - The high-current paths follow section 5.
- Designators: close to their own part (the user pulled them in about 1.3 mm from where the script left them), in neat aligned rows.
  Vertical (90°) is fine where it fits better. Keep 0.9 mm.
- Before handing a layout back, diff against the user's last saved version (epru diff: `{header}||{json}|` records per doc uuid).
  Learn from what they changed, and apply the same style next time.

## 5c. Heat budget (power-loss table): every board with a switching converter or ≥ 2 W total
Added at the user's request (dmx_node, 2026-10-21). Redo it whenever a power-path part changes, and whenever the user asks "full board heat".
Reference script: `design/dmx_node/work/heat_estimate.py` (parameters at the top; `run(vin, vout)` returns every loss item; has a self-check).
- **Operating points:** at least 2 VIN → VOUT points at full rated load, at the lowest and highest step-down ratio (e.g. 32 → 24 V and 24 → 12 V).
  State the load split (e.g. 10 A as 5 channels × 2 A) and the logic current.
- **Per-element table, in path order from input to output.** Each MOSFET, diode, shunt, fuse and inductor gets one row:
  part, gate drive actually applied, datasheet RDS(on) **max at that Vgs**, RMS current, and loss.
  Use hot resistance (RDS × 1.4 ≈ 100 °C; inductor DCR + 20 %) and say so. When comparing parts, show "now" and "proposed" columns side by side, plus totals.
- **Switching converter: never report conduction loss alone.**
  - High side: conduction D·Irms²·R + switching ½·VIN·I·(tr + tf)·fsw (tr, tf from Qgd + Qgs2 ÷ gate-drive current).
    Add the charge losses the low side causes: Qrr·VIN·fsw plus ½·(Qoss_HS + Qoss_LS)·VIN·fsw. These are dissipated mostly IN THE HIGH-SIDE FET:
    show them on the high-side row and name Q-low-side as the cause. (I left them out of a MOSFET table once and had to correct it.)
  - Low side: (1 − D)·Irms²·R + dead time 2·t_dt·fsw·VF·I.
  - Also count: inductor Irms²·DCR + core loss, current shunt Irms²·R, gate drive Qg·VCC·fsw (dissipated in the controller), controller quiescent loss.
  - Part choice: in the two switching positions, **charge (Qg, Qgd, Qrr, Qoss) matters more than mΩ**. In static positions (ideal diode, reverse polarity, load switch) only RDS(on) matters.
    Run both candidates through the model across the VIN range before consolidating part numbers.
- **Other sources:** linear regulators (Vin − Vout)·I, diodes VF·I, PTCs I²·R (R1max). Logic power is on-board heat; power delivered to external loads is not.
- **PTC resettable fuses:** use the datasheet **hold-current derating table** at the fuse's local temperature, not the 25 °C rating. Check that the channel current still holds at about 55–70 °C.
  Place PTCs away from hot spots. Voltage rating: a tripped PTC sees the full rail only on a dead short; in an overload (e.g. a long LED strip) the load keeps most of the voltage (user, 2026-10-21).
- **Board temperature rise:** ΔT ≈ P_total ÷ (h × 2 × board area), with h ≈ 12–15 W/m²·K in still air (100 × 62 mm ≈ 5–6 °C/W). A concentrated block (buck) runs a further 15–25 °C hotter.
  Name the **single hottest part** (W in its package) and give it the largest copper pour and the most thermal vias (5a1).
  Say plainly when sustained full load needs airflow or a metal plate, and use on-board temperature sensors for firmware derating.
- **Electrolytic life:** rated hours × 2^((T_rated − T_actual) ÷ 10). Keep electrolytics away from the inductor and switching FETs.
- Label every figure as a model estimate and tell the user to confirm it at bring-up with the on-board temperature sensors or a thermal camera.

## 6. Silkscreen (finishing step on every board)
- BOTTOM silk:
  - **Pin letters:** one per header pin, right beside the pin.
    Pin-letter codes:

    | Letters | Meaning |
    |---|---|
    | G T R | GND, TX, RX |
    | 5 G | 5V, GND |
    | 3 G | 3V3, GND |
    | V G S D | VCC, GND, SCL, SDA |
    | + − | battery |
    | K A | LED cathode, anode |

    T and R are from the MCU's side.
  - **Connector names:** 3–5 letters, e.g. USB, BAT, UART, I2C2, OLED, LID1, LID2, PWR, BTN, LED.
  - **Button names:** BOOT and RESET (etc.) beside each push-button, same 1.2 mm style.
  - **"WWW.DADABHAGWAN.ORG":** big (3.6 mm on human_counter). The user likes it VERTICAL up the board middle. It must stay clear of THT pads, so shrink it or shift it rather than let it run over pads.
- Modules / ICs without a JLC 3D model (e.g. Wio-S3): draw on TOP silk a module outline about 1.0 mm outside the pad rows, a thick (0.3 mm) L-marker at the pin-1 corner and a "1" beside pin 1.
  Skip outline pieces that would hit pads or labels. This lets JLC confirm the rotation in DFM.
- TOP silk designators: each one just OUTSIDE its own part, never on the footprint and never overlapping another label, pad or part body.
  Size is 0.9 mm MINIMUM / 0.15 mm stroke. NEVER go smaller: the user says it isn't legible. If a label doesn't fit, nudge parts or use 90° instead of shrinking.
  Script: `human_counter/work/layout2/designators.js` (it tries many orderings, keeps the best, then checks real attribute bboxes).
- Sizes: letters and names 1.2 mm high / 0.18 mm stroke. When the user asks for "next size up", step about ×1.2 (9 → 11).
- Every designator must be VISIBLE. The copilot autorouter hides some (`valueVisible:false`).
  Check visibility in the final verification; a bbox check alone misses hidden labels. Fix script: `human_counter/work/layout2/unhide_designators.js`.
- **Bottom-side text must ALWAYS look MIRRORED in the top view** (and therefore read normally on the back of the real board). That covers every string on bottom silk: pin letters, names, website, anything.
  - CAUTION (verified 2026-10-10 with a Gerber render): in JLCONE / EasyEDA Pro the bottom layer ALREADY mirrors text automatically.
    Via the API, create bottom text with `mirror = false`. Setting `mirror: true` cancels the auto-mirror and the text prints BACKWARDS on the board.
    The format doc's "set mirror 1 on bottom" does NOT apply to API-created strings.
  - Verify by exporting `pcb_ManufactureData.getGerberFile()` and rendering `Gerber_BottomSilkscreenLayer.GBO` from the top: the letters must look backwards.
    Or ask the user to look at the bottom layer in top view. Never trust the flag alone. Use alignMode 5 (centre).
  preview_pcb does not render silk text.

## 6b. Editing what the user has touched
- The user edits the board by hand between my steps.
- Before changing any pour, track or label, READ its current geometry and change only what the fix needs.
- Never rebuild an object from my own earlier coordinates. On 2026-10-10 that wiped the user's enlarged 5V pour; it was rolled back via the execute_js checkpoint.
- After a checkpoint restore, rebuild ALL pours on ALL layers. A restore can leave a plane with no fill (Inner 1 GND did).
- Same-layer pours need at least 0.35 mm between outlines; the zone spacing rule is 0.25 mm.

## 7. Verification before calling anything done
- Bottom text: ALL bottom-layer text is mirrored, checked on the Gerber render or the user's screen (HARD RULE 1).
- Unused pins: every floating pin justified from its datasheet name/function (HARD RULE 5).
- DRC = 0.
  - The "Connection Error" list shows BOTH ends of each missing link.
  - GND copper-zone connectMode: 1 = direct, 0 = thermal, 2 = none.
- Netcheck: every PCB pad net = the verified schematic netlist.
- Dead-end scan: DRC ignores dangling stubs, so scan for track ends that touch nothing.
- Power-path analysis (section 5).
- Fresh `.epro` backup. Update the board's CLAUDE.md.
- JLCONE stability:
  - Every execute_js call makes a checkpoint that is written to JLCONE's IndexedDB.
  - Hundreds of calls bloated `AppData\Roaming\jlcone\Partitions\shared\IndexedDB\https_pro.easyeda.com_0.indexeddb.leveldb` to 14–23 GB and crashed JLCONE twice.
  - Batch work into few, larger scripts.
  - Check that folder's size during long sessions.
  - Fix: close JLCONE, rename the folder aside, reopen. Projects live in the cloud account. Then re-import the copilot `.eext`.
- Before ordering, run the board's ORDER-TIME CHECKLIST.
- **PLASTIC HEADER CONNECTORS (user rule, 2026-10-11):**
  - **Design time:** every plastic through-hole header/connector (XH, PH, A2002/PZ200 2 mm, 2.54 mm pin headers, terminal blocks…) gets **DNP** ("Add into BOM = no") on both the schematic and the PCB. Keep the footprint.
  - **Order time: ALWAYS remind the user** to add these connectors to the LCSC cart separately (same checkout as JLC).
    Give a table: designator, LCSC number, part number, qty per board × number of boards, minimum order quantity. Note the orientation (opening to the edge, pin 1 = square pad).
  - Also tell the user that JLC's "won't be assembled / designator not in BOM" warning lists them and is expected.
  - SMD connectors (e.g. USB-C TYPE-C-31-M-12) stay JLC-assembled.
