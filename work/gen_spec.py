"""Single source of truth for the remote_v2 schematic (one page).

Generates spec/REMOTE.json (CircuitMod for extract_circuit_on_current_page) and spec/expected.json
(designator -> {pin: net}). Pins from datasheets (docs/datasheets): ATtiny3226 DS40002345B Table 3-1,
TPS631000 Table 4-1, AO3401A 1 G / 2 S / 3 D, TS-1187A 1,2 = one side / 3,4 = other.
Net "" = intentionally unconnected (see FIRMWARE/unused-pin audit).
"""
import json, os

DEV = {
    "ATTINY3226": "018ea453de8248c5adcf16338bd0025a",  # C3234898 ATTINY3226-SU SOIC-20W 1.27 mm. DNP: user hand-solders own chip, BOTTOM side
    "TPS631000":  "5694c9ba7a84493ba035806c6f65952f",  # C5219190 (Ext)
    "L1u":        "f831a951b8714a1cac0fb91e28a40e8a",  # C167203 FXL0420-1R0-M (Ext)
    "AO3401A":    "f58385f66b144586baef3753ba84f65d",  # C15127 (Basic)
    "TACT":       "38630614a58f4650bd5eefe9a5f79d9a",  # C318884 TS-1187A-B-A-B (Basic)
    "C100n":      "a299e4f29fd2469688f76621c3d59c4d",  # C14663
    "C10u":       "9f83e90cf78c4f3396a8045b3e521dd5",  # C19702 10uF 10V 0603
    "C22u":       "b9a0ddbe6f9a4bc4988e166e0381cb70",  # C59461 22uF 0603
    "C47n":       "4e4f04b8bec047d98e3a001ec0426079",  # C1622
    "R470k":      "54b4d7030704445385f5300a6f057f9a",  # C23178
    "R82k":       "e7430550b99b4e31a5c885b865dde147",  # C23254
    "R100k":      "c728373c421046ff8f40bf82a79ee519",  # C25803
    "R10k":       "b0602c6e20fc41faa226d9f001a9bead",  # C25804
    "R4k7":       "4c1efea8e0884946a0b9422079d22f25",  # C23162
    "R1k":        "2340647df7144fc5a30489ed06518eb5",  # C21190
    "R470":       "487bc79e6f384e22accab104e6107a75",  # C23179
    "R1M":        "60479e0364284da4a2720dc86428b721",  # C22935
    "R330k":      "a0420ebef65b40d0b64f64eaa00e77a1",  # C23137
    "R510k":      "b754a87e4c3d48f7ad70187a58d64d8e",  # C23192
    "HDR1x2":     "32d09cae88944ea8bd9e366e90edf831",  # C492401 (DNP)
    "HDR1x3":     "3c2517dd0d3741b3a6071ea8b3f9b7e9",  # C2937625 (DNP)
    "HDR1x4":     "f70eda5267b34d9799800488a27be0c6",  # C2691448 (DNP)
    "HDR1x5":     "b2a2f6c62e1345f99584f2356956658e",  # C492404 (DNP)
    "HC12":       {"uuid": "1d1b976b8289466bb9562f35bbf07810", "libraryUuid": "61d2498a443446bd998f15b4cbbb6009"},  # personal lib: 1x5 symbol + HC12_CASTELLATED_5P (5 SMD pads 2.032x1.778 mm, 2.54 pitch, copied from remote_1 A1)
    "LED3G":      "9de34034ae824cef84fe5114d84d3f82",  # C85161 Everlight 204-10SYGD/S530-E3 3 mm THT (DNP, hand-soldered)
}
VAL = {"C100n": "100nF", "C10u": "10uF", "C22u": "22uF", "C47n": "47nF", "R470k": "470k", "R82k": "82k",
       "R100k": "100k", "R10k": "10k", "R4k7": "4.7k", "R1k": "1k", "R470": "470", "L1u": "1uH", "R1M": "1M", "R330k": "330k", "R510k": "510k"}
PARTS = []

def part(des, dev, value, block, pins, dnp=False):
    PARTS.append(dict(des=des, dev=dev, value=value, block=block, pins=pins, dnp=dnp))

def two(des, dev, block, a, b):
    part(des, dev, VAL[dev], block, [(1, "1", a), (2, "2", b)])

# ---- power: battery -> reverse FET -> TPS631000 (always on) -> VCC 3.37 V
part("J1", "HDR1x2", "BAT", "Battery", [(1, "1", "BAT_IN"), (2, "2", "GND")], dnp=True)
part("Q1", "AO3401A", "AO3401A", "Battery", [(1, "G", "Q1_G"), (2, "S", "VBAT"), (3, "D", "BAT_IN")])
two("R1", "R100k", "Battery", "Q1_G", "GND")
part("U2", "TPS631000", "TPS631000", "Buck-boost", [
    (1, "VOUT", "VCC"), (2, "LX2", "LX2"), (3, "LX1", "LX1"), (4, "VIN", "VBAT"),
    (5, "EN", "EN"), (6, "MODE", "GND"), (7, "GND", "GND"), (8, "FB", "FB")])
# hard power switch (user): EN -> J6 -> VBAT; R29 pull-down so EN never floats (switch open = converter off, VCC = 0)
part("J6", "HDR1x2", "PWR_SW", "Power switch", [(1, "1", "VBAT"), (2, "2", "EN")], dnp=True)
two("R29", "R510k", "Power switch", "EN", "GND")   # 510k (user): 100k would leak 32 uA with switch closed; 510k = 6 uA
part("L1", "L1u", "1uH", "Buck-boost", [(1, "1", "LX1"), (2, "2", "LX2")])
two("R2", "R470k", "Buck-boost", "VCC", "FB")     # 0.5 * (1 + 470/82) = 3.37 V
two("R3", "R82k", "Buck-boost", "FB", "GND")
two("C1", "C10u", "Buck-boost", "VBAT", "GND")
two("C2", "C100n", "Buck-boost", "VBAT", "GND")
two("C3", "C22u", "Buck-boost", "VCC", "GND")
two("C4", "C22u", "Buck-boost", "VCC", "GND")
two("C5", "C100n", "Buck-boost", "VCC", "GND")

# ---- load switches (low = on; 100k pull-up = off in reset/sleep; 10k + 47n soft-start)
def switch(q, rpu, rs, cg, cout, cdev, gnet, en, out, block):
    part(q, "AO3401A", "AO3401A", block, [(1, "G", gnet), (2, "S", "VCC"), (3, "D", out)])
    two(rpu, "R100k", block, "VCC", gnet)
    two(rs, "R10k", block, en, gnet)
    two(cg, "C47n", block, "VCC", gnet)
    two(cout, cdev, block, out, "GND")
switch("Q2", "R4", "R5", "C6", "C7", "C22u", "RF_G", "RF_EN", "VRF", "HC-12 switch")
switch("Q3", "R6", "R7", "C8", "C9", "C10u", "OLED_G", "OLED_EN", "VOLED", "OLED switch")

# ---- MCU
part("U1", "ATTINY3226", "ATtiny3226", "MCU", [  # DNP (user part), see DEV note
    (1, "VDD", "VCC"), (2, "PA4", "BTN_UP"), (3, "PA5", "BTN_DOWN"), (4, "PA6", "BTN_PWR"),
    (5, "PA7", "VBAT_SNS"), (6, "PB5", "RF_EN"), (7, "PB4", "OLED_EN"), (8, "PB3", "HC_RX"),
    (9, "PB2", "HC_TX"), (10, "PB1", "SDA"), (11, "PB0", "SCL"), (12, "PC0", "BTN_LEFT"),
    (13, "PC1", "HC_SET"), (14, "PC2", "BTN_RIGHT"), (15, "PC3", "BTN_OK"), (16, "PA0", "UPDI"),
    (17, "PA1", "DBG_TX"), (18, "PA2", "DBG_RX"), (19, "PA3", "LED_A"), (20, "GND", "GND")], dnp=True)
two("C10", "C100n", "MCU", "VCC", "GND")
two("C11", "C22u", "MCU", "VCC", "GND")
# VBAT sense (user): 330k/1M divider, ADC on internal 2.5 V ref; 3.3 V -> 2.48 V (full scale 3.32 V). VBAT = Vadc * 1.33. 2.4 uA leakage
two("R8", "R330k", "Battery sense", "VBAT", "VBAT_SNS")
two("R22", "R1M", "Battery sense", "VBAT_SNS", "GND")
two("C12", "C100n", "Battery sense", "VBAT_SNS", "GND")

# ---- 3 mm status LED (on = remote on). PA3 high -> 1k -> LED -> GND, ~1.3 mA
two("R15", "R1k", "Status LED", "LED_A", "LED_R")
part("D1", "LED3G", "LED", "Status LED", [(1, "A", "LED_R"), (2, "K", "GND")], dnp=True)   # symbol probe: 1 = A, 2 = K

# ---- buttons (internal pull-ups; TS-1187A: A-B joined, C-D joined)
# hardware debounce (user, 2026-10-11): switch -> 1k -> MCU pin, 100 nF MCU pin -> GND; MCU internal pull-up (20-50k).
# 1k, not 10k: pressed level = VDD*Rs/(Rs+Rp) must be < VIL 0.3*VDD; 10k with Rp=20k gave 1.12 V > 1.01 V (review 2026-10-11)
for i, (sw, net) in enumerate([("SW1", "BTN_PWR"), ("SW2", "BTN_UP"), ("SW3", "BTN_LEFT"), ("SW4", "BTN_OK"), ("SW5", "BTN_RIGHT"), ("SW6", "BTN_DOWN")]):
    sn = "BTNS_" + net.split("_")[1]
    part(sw, "TACT", net.split("_")[1], "Buttons", [(1, "A", sn), (2, "B", sn), (3, "C", "GND"), (4, "D", "GND")])
    two(f"R{16 + i}", "R1k", "Buttons", sn, net)
    two(f"C{13 + i}", "C100n", "Buttons", net, "GND")
    two(f"R{23 + i}", "R10k", "Buttons", "VCC", net)   # external pull-up (user): defined level without firmware; pressed = VCC*1k/11k = 0.31 V

# ---- HC-12 (module pins: 1 VCC, 2 GND, 3 RXD in, 4 TXD out, 5 SET)
part("J2", "HC12", "HC-12", "HC-12", [(1, "1", "VRF"), (2, "2", "GND"), (3, "3", "HC12_RXD"), (4, "4", "HC12_TXD"), (5, "5", "HC12_SET")], dnp=True)
two("R9", "R1k", "HC-12", "HC_TX", "HC12_RXD")
two("R10", "R1k", "HC-12", "HC12_TXD", "HC_RX")
two("R11", "R1k", "HC-12", "HC_SET", "HC12_SET")

# ---- OLED 0.96" I2C (old JP1 order)
part("J3", "HDR1x4", "OLED", "OLED", [(1, "1", "SDA"), (2, "2", "SCL"), (3, "3", "VOLED"), (4, "4", "GND")], dnp=True)
two("R12", "R4k7", "OLED", "VOLED", "SDA")
two("R13", "R4k7", "OLED", "VOLED", "SCL")

# ---- programming / debug
part("J4", "HDR1x3", "UPDI", "Prog", [(1, "1", "VCC"), (2, "2", "GND"), (3, "3", "UPDI_J")], dnp=True)
two("R14", "R470", "Prog", "UPDI_J", "UPDI")
part("J5", "HDR1x4", "DEBUG", "Prog", [(1, "1", "VCC"), (2, "2", "DBG_TX"), (3, "3", "DBG_RX"), (4, "4", "GND")], dnp=True)

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    os.makedirs(os.path.join(here, "spec"), exist_ok=True)
    seen = set()
    for p in PARTS:
        assert p["des"] not in seen, "duplicate designator " + p["des"]
        seen.add(p["des"])
    expected = {p["des"]: {str(n): net for n, _, net in p["pins"]} for p in PARTS}
    meta = {p["des"]: {"value": p["value"], "dnp": p["dnp"], "dev": p["dev"]} for p in PARTS}
    json.dump({"pins": expected, "meta": meta}, open(os.path.join(here, "spec", "expected.json"), "w"), indent=1)
    add = [{"designator": p["des"], "value": p["value"], "block_name": p["block"], "search_query": p["value"],
            "part_uuid": DEV[p["dev"]], "pins": [{"pin_number": n, "name": nm, "signal_name": net} for n, nm, net in p["pins"]]}
           for p in PARTS]
    json.dump({"add_components": add}, open(os.path.join(here, "spec", "REMOTE.json"), "w"), indent=1)
    nets = {}
    for d, pins in expected.items():
        for n, net in pins.items():
            if net: nets.setdefault(net, []).append(f"{d}.{n}")
    single = sorted(k for k, v in nets.items() if len(v) == 1)
    assert not single, "single-pin nets: " + str(single)
    print("parts:", len(PARTS), "nets:", len(nets), "DNP:", [p["des"] for p in PARTS if p["dnp"]])

if __name__ == "__main__":
    main()
