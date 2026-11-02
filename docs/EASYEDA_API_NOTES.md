# EasyEDA PCB Design — Working Notes

This project uses EasyEDA Pro Desktop + the `easyeda-copilot` MCP bridge for
schematic/PCB automation. These are hard-won gotchas from real sessions, not
generic advice — follow them to avoid re-discovering the same bugs.

## Pin assignments — verify against the datasheet, always
- Before wiring any MCU/IC pin function (I2C, UART, SPI, ADC, timers),
  look it up in the datasheet's pin-multiplexing table for the exact package
  (e.g. ATtiny3224 SOIC-14 Table 3-1: pin 8 = PB1 = **SDA**, pin 9 = PB0 =
  **SCL**, pin 11 = PA1 = TXD1, pin 12 = PA2 = RXD1). Never infer the mapping
  from the order of words in a request ("pins 9 & 8 are SCL, SDA" means
  9=SCL, 8=SDA). A swapped I2C pair was caught only at pre-order review.
- Re-check every MCU pin against the datasheet in the final review before
  any order, and list the pin map in the project documentation.

## Sourcing & parts
- Every component must be a real, JLCPCB-orderable part: verify an LCSC
  `part_uuid`/C-number via `component_search` before using it, and confirm the
  footprint name matches what JLCPCB expects (e.g. `R0805`, `C0603`).
- **Prefer JLCPCB Basic parts, then the cheapest option.** Extended parts
  add a per-part-type setup fee on every assembly order, which outweighs
  unit-price differences. Also check stock before recommending a part.
- **Resistors:** 0603 by default, always 1% tolerance (cheap 1% parts are
  readily available, e.g. UNI-ROYAL `0603WAF…` series).
- **Capacitors:** 0603 by default, X7R dielectric, and pick the highest
  voltage rating available at the lowest cost.
- Deviate from these defaults only when the user asks for a specific part
  or size.
- ERC "Component attributes does not match the Supplier Part" usually means
  the component's `supplierId` holds the sub-part name (e.g.
  `0603WAJ0510T5E.1`) instead of a real LCSC C-number, with blank
  attributes. This breaks JLCPCB BOM matching. Fix via API: read the real
  record with `eda.lib_Device.get(lcscPartUuid, "lcsc")`, check
  `property.manufacturerId` matches, then `sch_PrimitiveComponent.modify(id,
  {manufacturer, manufacturerId, supplier, supplierId, otherProperty})`,
  copying only attribute keys (never Symbol/Footprint/3D Model/Designator).
  Get the lcsc part_uuid from `component_search` by MPN.

## Standard power-input protection — reuse on every board (user decision 2026-10-05)
Proven on RS485-LoadCell-Node. The order matters:
1. Connector VIN → **B5819W Schottky (C8598, Basic, SOD-123)** for reverse
   polarity. It must come FIRST, ahead of everything else.
2. → **0.1 Ω 0603 1% shunt (0603WAF100LT5E, C111027)** with an **INA180A3
   (C48533470, SOT-23-5, TI pinout A: 1 OUT, 2 GND, 3 IN+, 4 IN−, 5 VS)**. IN+
   goes on the diode side and IN− on the load side. Gain is 100, so
   I = Vout / 10 A. INA180 inputs are rated only GND−0.3 V…26 V, so never put
   it ahead of the diode.
3. → VIN_PROT → **one PTC fuse per regulator branch: BSMD0805-050-15V
   (C883109, 0805, 500 mA hold / 1 A trip, 15 V, ~0.1 Ω)**. This way one
   shorted branch can't pull down a shared bus.
4. → each **SE8533K2-HF LDO (C115013, SOT-89, GIO: 1 GND, 2 VIN, 3 VOUT;
   36 V max, 200 mA)**, with its input cap placed after the fuse.
- Optional: a VIN_PROT sense divider (27 k / 16 k + 100 nF) to an ADC pin.
- Survives a reversed supply. The max supply is 26 V, set by the INA180.
- Re-verify every pin against the datasheet for each new board.

## Instance / bridge management
- Call `list_easyeda_instances` before assuming which window has which
  document open — schematic and PCB are often in separate windows, and the
  active one can change without notice (user switching tabs, a window
  reconnecting after a crash).
- If DRC/`inspect_net`/`check_pcb_drc` start timing out but simple calls
  (`get_current_project_info`, `list_easyeda_instances`) still respond fast,
  the bridge connection is healthy — it's that specific check hanging. Don't
  spam retries; try `sync_current_document` once, otherwise ask the user to
  check the app directly.
- `execute_js` auto-checkpoints before running, but the checkpoint ID isn't
  always surfaced on error. Call `save_checkpoint_for_current_page` explicitly
  before any batch of risky edits so you have a named, recoverable point.

## Schematic editing reliability
- **`setState_X()` + `.done()` on component/pin/net-port objects is
  unreliable** — it sometimes reports success but doesn't persist, or
  corrupts the value (e.g. a net silently becoming `"NET?"`). Always re-read
  fresh after using this pattern to confirm it actually stuck.
- **Prefer the static `modify(id, {...})` method** wherever the property is
  supported (e.g. `sch_PrimitiveComponent.modify(id, {designator: "R13"})`).
  It reliably persists where `setState`+`done()` does not.
- **But `sch_PrimitiveComponent.modify()` wipes supplier data** unless you pass
  it: any call (even just `{addIntoBom:false}` or `{designator}`) resets
  `supplierId` to `<MPN>.1` and blanks the attributes. Always include
  `manufacturer, manufacturerId, supplier, supplierId, otherProperty` (read
  them first, or re-fetch via `lib_Device.get`) and re-verify after. The PCB
  `pcb_PrimitiveComponent.modify()` does not have this problem.
- `modify()` does **not** support changing a net-port's `net` property —
  there's no `net` field in its schema. To change a net port's net: delete it
  and recreate via `createNetPort(direction, net, x, y, rotation, mirror)`,
  which is fast and doesn't hit the device-fetch hang below.
- **Never pass `libraryUuid: "lcsc"` to native `eda.*` calls.** `"lcsc"` is an
  MCP-side alias (used by `component_search`); native
  `sch_PrimitiveComponent.create()` doesn't resolve it and hangs forever
  instead of throwing. The extension itself translates it first:
  ```js
  const lib = await eda.lib_LibrariesList.getSystemLibraryUuid(); // LCSC system lib
  const c = await eda.sch_PrimitiveComponent.create(
    { libraryUuid: lib, uuid: partUuid }, x, y, undefined, rot, false, true, true);
  ```
  (fallback library the extension tries: `f5af0881d090439f925343ec8aedf154`).
  Wrap create() in a timeout race and check for orphans if it ever times out.
- Net-stub wiring technique: a short (~2 unit) wire tagged with an explicit
  net name merges into that net regardless of physical touching. Keep stubs
  short — long or "smart-routed" stubs risk crossing and merging unrelated
  nets.
- After any topology change (rewiring, pin reassignment), re-verify with the
  **full netlist export** (`sch_ManufactureData.getNetlistFile`), not just
  `get_schematic`'s per-pin `signal_name` — that field can be inconsistent
  between reads when multiple labels/text objects sit near the same pin.
- Never place a net flag/port pin directly on a component pin. EasyEDA ERC
  treats overlapping pin endpoints as a **fatal error** ("endpoints overlap
  and are not connected"), which also makes netlist export silently return
  nothing. Always join them with a short wire.
- Let net ports name the net; create stub wires *without* an explicit `net`
  argument. A wire's explicit net masks a detached port (the pin still shows
  the right name while the port is floating) and causes "multiple net names"
  warnings. `modify()` cannot move net flags/ports — delete and recreate.
- If the user tidies ports/flags in the UI, wires may not follow. After any
  concurrent editing, re-read port positions before assuming connectivity.
- Never run `beautify_schematic_on_current_page` unless explicitly asked —
  it can wipe a user's manual layout/labels.

## Schematic ↔ PCB sync
- `import_pcb_changes` can report "no changes" even when components are
  genuinely missing from the PCB, if their internal PCB-link is stale. Fix:
  toggle the component's `addIntoPcb` off then on via `modify()`, then
  re-run the import.
- **Import Changes reads the *saved* schematic.** Save the schematic
  (`save_doc`) before importing, or part swaps won't reach the PCB — and an
  import can even revert PCB edits back to the stale saved schematic. Once
  saved, Import Changes correctly re-links swapped devices ("Modify Device")
  without moving parts. Never have the project open in two apps (e.g.
  EasyEDA Pro + JLCONE) at once — whichever saves last wins.
- After every import, diff MPN + LCSC # per designator between schematic and
  PCB, and run DRC (footprint swaps can nudge pads under tracks).
- Newly-imported components often land in a far-off staging area (way
  outside the board outline) — always check the component bounding box after
  an import and reposition before assuming placement is done.

## PCB primitives
- All PCB coordinates in the native API are in **mil**, regardless of the
  app's display unit setting (which is a UI-only preference with no
  scriptable toggle). `get_current_pcb` normalizes reads to mm for
  convenience only.
- Board outline must be a single closed `pcb_PrimitivePolyline` (via
  `pcb_MathPolygon.createPolygon()` + `pcb_PrimitivePolyline.create(net, 11,
  polygon, lineWidth, lock)`) — not separate `PCB_PrimitiveLine` segments.
  The router's outline reader only recognizes the polyline form.
- **The point array must explicitly close the loop** (repeat the first
  point at the end) — otherwise one edge silently goes missing from the
  rendered outline.
- **Removing a footprint's silkscreen outline** (no per-component toggle exists):
  `eda.lib_Footprint.openInEditor(fpUuid, <PROJECT uuid>)` (the project uuid,
  not the library uuid from `lib_Footprint.get`, which throws `[object Object]`).
  Confirm the editor is active (`pcb_PrimitiveComponent.getAll()` is empty and
  the pad count matches) before deleting anything. Then delete the
  polylines/lines/arcs/fills on layers 3/4, keep the strings (name and
  designator placeholders), and save with `eda.pcb_Document.save()`
  (`save_doc` rejects doc type 4). The PCB instances update automatically.
  The copilot `preview_pcb` doesn't render footprint silkscreen, so ask the
  user to confirm visually.
- True NPTH holes: `pcb_PrimitivePad.create()` with `metallization: false`,
  pad shape diameter equal to hole diameter (no annular ring), layer
  `MULTI` (12).

## Routing & DRC
- Auto-routing corner style is **hard-limited to 45°/90°** — no arc/free
  option exists in either the native autorouter or the copilot's routing
  DSL. Arc corners are manual-routing-only.
- DRC rule changes (track width, clearance) go through the routing DSL's
  `drc()` / `netClass()` / `signalNet()` / `powerNet()` statements ending in
  `applyDrcRules()` — not any method on the native `PCB_Drc` class (which
  only handles net classes, differential pairs, and pad-pair groups).
- `powerNet()`/`signalNet()` rule overrides apply per **net class** — a
  change to one net can silently affect others grouped in the same class.
  Always re-check with `get_pcb_drc_rules` after applying.
- If a GND (or any) plane pour shows isolated-island DRC errors: 
  `clearRouting({nets: [netName], items: ["zones"]})` then re-`plane()` with
  `zone: { removeIslandsBelowMm2: N }`, terminated with `runCopper()`. The
  exact area threshold may not be honored, but islands get removed regardless.

## General collaboration
- Checkpoint before any batch of destructive/risky edits, and tell the user
  what you're about to do before doing it.
- If placement looks wrong (out of board, huge bounding box vs. target
  size), say so and ask before routing — don't try to route around a bad
  layout.
- When the user is actively working in the same document (parallel
  schematic/PCB windows), re-verify state before big changes — manual drags
  and additions won't show up until you re-read.
