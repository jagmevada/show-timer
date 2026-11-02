# Working with Jagme on Bhojanshala / wscale PCBs (standing preferences)

These preferences are collected from the user's instructions and corrections (2026-10). They apply to every board.
The full design rules live in the `pcb-design-rules` skill (a copy sits in each design folder at `.claude/skills/`).

## How the user works
- Tool chain: JLCONE (built-in EasyEDA Pro) + the easyeda-copilot MCP plugin. JLCONE has ONE project open at a time.
  Never open the same project in EasyEDA Pro at the same time.
- The user sometimes works remotely ("rc mode") and cannot click EasyEDA dialogs then. **Import Changes always needs the user's click.**
- **Process:** section by section.
  1. Build a connection table from the datasheet pin tables.
  2. The user reviews it.
  3. Draw one page.
  4. Script-diff the netlist and run ERC.
  5. Only then move to the next section.
- **Verify every IC/MCU pin against the datasheet** before wiring and before ordering. A swapped SDA/SCL pair nearly cost a ~$350 order.
- "**commit**" = save an EasyEDA checkpoint AND export a backup to `wscale/PCB/backups/<board>_<date>_<tag>.epro2`.
  Back up before every risky edit too.
- **Backups must be `.epro2`.** `sys_FileManager.getProjectFile()` output is the V3 format. JLCONE imports a file named `.epro` as the OLD format and creates an EMPTY project.
  The API import (`importProjectByProjectFile`) does nothing in JLCONE. The user imports `.epro2` files by hand.
- Schematics:
  - **NEVER run beautify.** The user arranges schematics by hand.
  - Never delete zero-length wires: where a flag or port touches a pin, they ARE the connection.
  - Use one short, intuitive name per net, set by one net port. Never put an explicit net on wires.
  - Dragging a net label in JLCONE detaches it. After any manual edit by the user, export the netlist and re-verify.
- Don't stall. When a step is blocked, finish everything else and report clearly.

## Parts and cost
- **JLC charges $3 per Extended part number.** Use JLC Basic parts first, then the cheapest. Swap Extended R/C for Basic drop-ins and check stock.
  - Resistors: 0603 1% UNI-ROYAL 0603WAF…, e.g. 47R C23182, 49.9R C23185, 4.7k C23162, 10k C25804, 15k C22809, 100k C25803.
  - 100 nF: C14663. 1 µF: C15849. 10 µF 0805: C440198 (50 V X5R, Basic). 10 µF 0603: C19702.
- **Plastic through-hole header connectors** (XH, PH, A2002/PZ200 2 mm, 2.54 mm pin headers, terminal blocks):
  - Always DNP in the design (keep the footprint).
  - **At order time, ALWAYS remind the user** to buy them loose from LCSC and hand-solder.
    Give a table: designator, LCSC number, part number, qty per board × boards, minimum order quantity, orientation.
  - Timing: AFTER paying the JLC PCB+PCBA order, put them in the LCSC cart and choose "combine with JLCPCB order" at checkout. The JLC order must be paid but not packed, with the same account, currency and address.
  - Never add them to the JLC PCBA BOM.
- JLC's "won't be assembled / designators don't exist in the BOM" warning just lists the DNP parts. It is expected; check it matches the DNP list.
- Assembly is TOP side only. Bottom-side parts are never fitted.
- Preferred building blocks:
  - USB-C input OVP: ETA7014 (C7465500).
  - 1S charger with power path: ETA6003 (C5455585).
  - Buck-boost: TPS631000 (C5219190).
  - Inductors: FXL0420 1 µH (C167203) / 1.5 µH (C167205).
  - USB-C connector: TYPE-C-31-M-12 (C165948).
- External-supply boards use this input chain: B5819W Schottky FIRST → 0.1 Ω shunt + INA180A3 → one PTC per regulator branch → LDO.
- Off-board power switch: 2-pin 2.0 mm header + series R + RC debounce. The ESD5Z is optional/DNP when R+C already protect the pin.

## Layout
- All parts on top. Connectors on board edges as the user groups them. Make the board as small as possible.
- Mounting holes: plated, with no parts and no vias within a 6 mm diameter.
- Size high-current tracks by IPC-2221 and report path R and drop per rail (≤30 mV target).
- Silkscreen finishing:
  - Bottom pin letters (G T R, 5 G, 3 G, V G S D, + −, K A). T/R are from the MCU's side.
  - Connector names on the bottom.
  - Big "WWW.DADABHAGWAN.ORG" on the bottom.
  - Mirrored bottom text, alignMode centre.
- Corner fillets on request. Check copper clearance first (the counter took R3; the RS485 only R1).
- Before calling anything done: DRC 0, netlist check, dead-end scan, power-path check, `.epro2` backup, update the board's CLAUDE.md.

## Panels (shelved 2026-10-11)
- The user ordered each design separately because there was little price difference. The tooling for a mixed-design mouse-bite panel is in `design/panel_trial/`.
- EasyEDA's panel tool only repeats one design.
