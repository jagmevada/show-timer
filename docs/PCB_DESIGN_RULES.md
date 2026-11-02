# PCB Design Rules — Bhojanshala boards

Standing rules for every board, built from real project experience (RS485 load-cell node and the human-counter LoRa board, 2026).
They target EasyEDA Pro / JLCONE with JLCPCB fabrication and assembly. Apply them to every new board.

---

## Hard rules (never break)

1. **Final-stage check: all bottom-layer text must be mirrored.**
   Before a board is called finished or ordered, every bottom-silkscreen text must look mirrored in the top view, so it reads normally on the back of the real board.
   - That covers pin letters, connector and button names, the website text, and anything added late.
   - Check it on the real output: the bottom-silkscreen Gerber viewed from the top, or the bottom layer in the editor's top view. A property flag alone is not proof.
2. **Part designators are at least 0.9 mm high, always visible, and never on a footprint or pad.**
3. **Two kinds of GND via:**
   - **Thermal vias** for heat-dissipating parts (switching converters, chargers, LDOs, ESP32-class modules/MCUs, power FETs) use the normal DRC clearance.
     They are functional: plan room for them during layout and fit as many as possible close to the part.
   - **Stitching vias** are low priority and go in last, at least 2× DRC spacing from pads, tracks and holes.
     Skip any spot that doesn't fit, and never move routing or placement for them.
4. **Before editing anything someone else has touched, read its current state** and change only what's needed. Back up before risky edits.
5. **Unused-pin audit: every unconnected pin must be proven genuine.**
   For each floating pin, read its name and function in the datasheet and record why it's open: datasheet NC/test pin, unused interface pin, strapping pin with a safe internal default, or spare GPIO handled in firmware.
   **Power, GND, enable/reset, sense/feedback, RF/antenna and exposed pads are never left open** without a specific documented reason (e.g. a module variant that routes RF to its own connector).
   Anything that can't be justified is a mistake and must be fixed before ordering.

---

## 1. Workflow
- **Work section by section:**
  1. Build a connection table from the datasheet pin tables.
  2. Review it.
  3. Draw one schematic page.
  4. Diff the netlist against the table and run ERC.
  5. Only then move to the next section.
- **Verify every IC/MCU pin** against its datasheet pin table before wiring and again before ordering. (A swapped SDA/SCL pair once nearly cost a full PCB order.)
- **"Commit" before risky edits:** save an editor checkpoint and export a dated project backup (`<board>_<date>_<tag>.epro`).
- **Never auto-beautify a hand-arranged schematic.**
- **Never delete zero-length wires:** where a flag or port touches a pin, they are the connection.
- **One short, intuitive name per net**, set by one net port. Never put a second label on the same connection.
- **After any manual edit, re-export the netlist and re-verify.** Dragging a net label can silently detach it from its wire.

## 2. Parts
- JLCPCB **Basic** parts first, then the cheapest. Extended parts are fine when the extended fee is small (≤ $2).
- Resistors 0603 1%. Capacitors 0603 X7R, highest voltage rating available.
- **Default picks** for battery / USB-C boards:

  | Function | Part | LCSC |
  |---|---|---|
  | USB-C input over-voltage protection | ETA7014 | C7465500 |
  | 1S Li-ion charger with power path | ETA6003 | C5455585 |
  | 3V3 / 5V rails (buck-boost) | TPS631000 | C5219190 |
  | Power inductor | FXL0420 1 µH (1.5 µH for the ETA6003) | C167203 / C167205 |
  | USB-C connector (charging + USB 2.0) | TYPE-C-31-M-12 | C165948 |
  | Converter output caps | 2× 10 µF 0805 X7R | C380347 |
  | Decoupling | 100 nF 0603 X7R | C14663 |

- **Boards powered from an external supply:** Schottky (reverse polarity) → 0.1 Ω shunt + INA180 current sense → one PTC fuse per regulator branch → regulator. The INA180 must sit after the diode.
- **Power switch:**
  - Use a 2-pin 2.0 mm header for an off-board mechanical switch, not a PCB switch.
  - Add ESD protection, a series resistor and RC debounce against switching transients.
  - With the ETA6003, drive its ship-mode pin (ENPPB) from the switch.

## 3. Board layout
- **All parts on the TOP side** (JLCPCB assembles one side).
- **Connectors on board edges, grouped by function:**
  - USB-C and the sensor connectors share one edge.
  - All other headers go together on the opposite edge, in rows if needed.
  - Buttons go on a side edge, never mixed into the connector rows.
- **Main module / MCU near the centre.** Keep the board as small as possible.
- **Mounting holes:** plated, tied to GND. No parts and no vias within a 6 mm diameter around each hole. Everything else can be tight.

## 4. Layers and copper (4-layer)
- **Stack:** 1.6 mm 4-layer, e.g. JLC04161H-7628. Boards that share a panel must share layer count and thickness.
- **Inner 1: solid GND plane**, unbroken under radios and modules (return path).
- **Inner 2: power.** One main rail plane plus copper fill regions for other high-current rails (e.g. a 3V3 strip from regulator to module, a 5V area to the sensor connectors).
  Don't spend both inner layers on ground.
- **Copper fills for current:** where a track is too narrow for its current, add a fill region or a parallel inner-layer path.
- **Same-layer fills:** keep at least 0.35 mm between neighbouring fill outlines.

## 5. High-current trace sizing (check on every board)
- **Size by IPC-2221** (1 oz outer copper, 10 °C rise):

  | Current | Minimum width |
  |---|---|
  | 0.5 A | 0.12 mm |
  | 1 A | 0.30 mm |
  | 1.5 A | 0.53 mm |
  | 2 A | 0.78 mm |

  Inner 0.5 oz copper needs about 2× the width. A 0.3 mm via carries about 1 A, so use 2+ vias per amp.
- **Calculate each rail's voltage drop** from source to load at worst-case current. **Target ≤ 30 mV per rail.**
  - **USB:** VBUS / VIN (charge + load).
  - **Charger:** switch node, system rail, battery.
  - **Battery negative:** the battery-negative chain, including protection FETs, shunt and GND return.
  - **Regulator outputs:** to the main module and to the sensors.
- **Short narrow sections** are acceptable only right at IC pins (≤ 1.3 mm long). Count the current split across paired pins.

## 6. Exposed / thermal pads
- **Small QFN / DFN parts (about 3×3 mm and smaller): no vias inside the exposed pad.**
  A via that isn't perfectly flat can lift or tilt a light part during reflow.
  Connect the pad to GND with wide top copper, and place thermal vias just outside the package on that copper.
  Reserve space for them during placement and routing; keep other tracks out of that area on every layer.
- Always follow the datasheet's recommended land pattern and via scheme when it gives one.
- **Large exposed pads** (modules, big QFNs):
  - **A grid of GND vias** at about 1.0–1.2 mm pitch down to the GND plane, for soldering, grounding and heat.
  - **Vias inside the pad:** open (no solder mask) on top so solder wets the whole pad; tented on the bottom so solder doesn't drain through.
  - **Keep other nets' tracks out from under the pad** so the via grid can be complete.

## 6b. Thermal vias (functional, high priority)
- **For every heat-dissipating part** (switching converters/chargers, LDOs, ESP32-class modules/MCUs, power FETs, current shunts): place GND vias as close as possible, on the GND copper tied to its GND pins / exposed pad.
- **Clearance:** normal DRC spacing (not the 2× stitching margin). GND vias at least about 0.8 mm apart.
- **Reserve the space** during placement and routing: keep other nets' tracks (all layers) out of the area around hot parts. Aim for 6–8 or more vias around each converter.

## 7. GND stitching vias (last step, low priority)
- **Spread across the board** at about 3 mm pitch, only where a via sits fully inside both the top and bottom GND copper. This prevents large ground areas hanging on a thin link.
- **Hot parts** don't rely on stitching; they get thermal vias (section 6b).
- **Clearance:**
  - At least 2× DRC spacing from any track, via, hole and pad. Vias **under** a part body are fine; only pads need the margin.
  - Keep clear of mounting holes, the board edge and connector slots (e.g. USB-C shell slots).
- **If a spot doesn't meet every margin, skip it.** Never shrink margins, never move anything to make room. Fewer vias is fine.
- **Always done after routing is finished,** so it never constrains routing.

## 8. Thermal relief
- **Default: direct connection.** SMD pads and vias connect directly to ground copper.
- **Hand-soldered through-hole headers:** use thermal relief (spokes) on their GND pins only, so they can be soldered by hand.
  Don't switch thermal relief on board-wide: it also affects vias and breaks up the planes.
- **Mounting holes:** direct connection to GND.

## 9. Routing and placement style
- **Grid:** snap tracks, parts and labels to a 5-mil (0.127 mm) grid. Line up rows of parts (nudge in 0.13 / 0.25 mm steps).
- **Angles:** 0°, 45° and 90° segments only. No odd angles, no tiny stubs.
- **Widths:**
  - Signals 0.254 mm (10 mil) minimum.
  - Feedback / sense lines 0.25–0.3 mm.
  - Local power branches (rail to caps, sense taps) 0.4–0.5 mm.
  - High-current paths per section 5.

## 10. Silkscreen
**Top side:**
- **Designators:** just outside their own part, in neat aligned rows, close to the part.
  - 0.9 mm minimum height, 0.15 mm stroke.
  - Never on a footprint, pad or other label.
  - 90° text is fine where it fits better.
  - If one doesn't fit, nudge parts or rotate it; don't shrink it.
- **Every designator must be visible.** Auto-routers can hide some, so check before finishing.
- **Modules without a 3D model at the assembler:** draw a module outline about 1 mm outside the pad rows, a thick L-marker at the pin-1 corner and a "1" next to pin 1, so the assembler can confirm rotation.

**Bottom side** (labels 1.2 mm high, 0.18 mm stroke):
- **Pin letters:** one per header pin, right beside it. T and R are named from the MCU's side.

  | Letters | Meaning |
  |---|---|
  | G T R | GND, TX, RX |
  | 5 G | 5V, GND |
  | 3 G | 3V3, GND |
  | V G S D | VCC, GND, SCL, SDA |
  | + − | battery |
  | K A | LED cathode, anode |

- **Connector names:** 3–5 letters (USB, BAT, UART, I2C2, OLED, LID1, LID2, PWR, BTN, LED…).
- **Button names:** BOOT, RESET, etc. beside each button.
- **"WWW.DADABHAGWAN.ORG":** large (about 3.6 mm), usually vertical up the middle of the board, clear of all pads.
- **All bottom text mirrored** (hard rule 1).
- **"One size up"** means about ×1.2 (e.g. 9 → 11).

## 11. Final verification (before calling a board done)
1. **All bottom text mirrored**, checked on the Gerber or in the top view.
2. **Unused-pin audit**: every floating pin justified from its datasheet (hard rule 5).
3. **DRC = 0.**
4. **PCB matches the schematic:** every pad net equals the verified schematic netlist, and ERC is clean.
5. **No dangling track ends.** DRC doesn't report them, so scan for them.
6. **Power-path voltage drops** within target (section 5).
7. **All designators visible** and not overlapping pads.
8. **BOM check:** every fitted part has a valid LCSC number, and hand-soldered parts are excluded from the BOM.
9. **Fresh dated backup.**
10. **Order checklist:**
   - reserve low-stock parts,
   - re-check stock,
   - order cables and antennas separately,
   - add placement notes for parts without a 3D model,
   - tick "confirm placement before production".

---

## Appendix — EasyEDA Pro / JLCONE automation notes
Lessons from scripting EasyEDA Pro (JLCONE) through its API:

**Silkscreen and labels**
- **Bottom-layer text mirroring:** JLCONE already mirrors bottom-layer text automatically. Create bottom text with `mirror = false`; setting `mirror: true` cancels the auto-mirror and the text prints backwards. Always confirm on the Gerber.
- **Hidden designators:** the auto-router can hide designators (`valueVisible: false`). Check visibility, not just position.

**Pads, vias and pours**
- **Per-pad thermal relief:** set `heatWelding: { connectionMethod: "Divergent", … }` and **always pass the pad's rotation in degrees in the same call**. Otherwise rotation can be rewritten in radians and square pads end up tilted. Afterwards, confirm every pad rotation is a multiple of 90°.
- **Via solder mask:** set it per via with `solderMaskExpansion: { topSolderMask, bottomSolderMask }` (in mil). A large negative value tents the via.
- **Copper-zone connection codes:** `0` = thermal, `1` = direct, `2` = none.
- **Pour priority:** it can't be changed through the API. Separate same-layer fills by their outline shape instead.
- **After a checkpoint restore, rebuild every copper pour.** A restore can leave a plane with no fill.

**DRC and stability**
- **"Connection Error" lists both ends of each missing link**, so the listed item is often fine and its neighbour is the broken one.
- **JLCONE stability:** every scripted change writes a checkpoint to JLCONE's local database (IndexedDB). Hundreds of small calls grew it to 20+ GB and crashed the app.
  Batch work into fewer, larger scripts. If it happens, close JLCONE, move that database folder aside and reopen; cloud projects are kept.
