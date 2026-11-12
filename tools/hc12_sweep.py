"""Old-vs-new HC-12 compatibility sweep, each direction tested separately.

Both boards run the bridge firmware (src/bridge.cpp): the panel (OLD module, `pio run -e bridge_panel`, PC on its
mon port) and a remote (NEW module, `pio run -e bridge`, PC on J5). The OLD module stays on the field settings
(FU3, 2400, C012 = address 2). The NEW module is stepped through modes, bauds and channels; for each setting one
packet goes new->old and one old->new.
  python hc12_sweep.py NEWPORT OLDPORT [query|modes|channels|all|live[:seconds]]
  live = both on the field settings, one packet each way per second (for tuning the new module's crystal).
2026-11-13 result: old and new link at every air rate except FU3 at 1200/2400 (5k air, narrowest filter):
the new batch's crystal is off frequency.
Needs pyserial (the PlatformIO python has it: ~/.platformio/penv/Scripts/python.exe).
"""
import sys
import time

import serial

BAUDS = [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
FIELD = (3, 2400, 12)  # FU, baud, channel of the panels
MSG = b"Uu12345\n"


class Hc12:
    def __init__(self, name, port):
        self.name, self.s = name, serial.Serial(port, 115200, timeout=0.05)

    def bridge(self, c):
        self.s.write(b"\x00" + c.encode())
        time.sleep(0.02)

    def read(self, t):
        d, end = b"", time.time() + t
        while time.time() < end:
            d += self.s.read(256)
        return d

    def at(self, c, wait=0.3, show=False):
        self.s.reset_input_buffer()
        self.s.write(c.encode())
        r = self.read(wait)
        if show:
            print(f"  {self.name} {c:9} -> {r!r}")
        return r

    def setup(self, fu, baud, ch, query=False):
        """Cold start in AT mode (always 9600), settings, leave AT mode (applies them), UART to the new baud."""
        self.bridge("p"), self.bridge("s"), self.bridge("4")
        time.sleep(1.0)  # really off (main.cpp waits 1 s too)
        self.bridge("P")
        time.sleep(0.5)
        ok = b"OK" in self.at("AT", show=query)
        for c in (f"AT+FU{fu}", f"AT+B{baud}", f"AT+C{ch:03d}", "AT+P8"):
            ok &= b"OK" in self.at(c, show=query)
        if query:
            for c in ("AT+RX", "AT+V", "AT+U"):
                self.at(c, 0.6, show=True)
        self.bridge("S")
        time.sleep(0.3)
        self.bridge(str(BAUDS.index(baud) + 1))
        self.s.reset_input_buffer()
        return ok


def send(a, b):
    """a sends MSG, b listens 1.5 s. Returns what b heard."""
    b.s.reset_input_buffer()
    a.s.write(MSG)
    return b.read(1.5)


def main():
    new, old = Hc12("NEW", sys.argv[1]), Hc12("OLD", sys.argv[2])
    what = sys.argv[3] if len(sys.argv) > 3 else "all"
    for h in (new, old):
        h.s.write(b"\x00?")
        assert b"BRIDGE" in h.read(0.5), f"{h.name}: no bridge on {h.s.port}"
    print("OLD on the field settings:")
    old.setup(*FIELD, query=True)
    hits = []

    def test(fu, baud, ch, query=False):
        ok = new.setup(fu, baud, ch, query)
        n2o, o2n = send(new, old), send(old, new)
        tag = f"NEW FU{fu} B{baud:<6} C{ch:03d}"
        res = [("new->old", n2o), ("old->new", o2n)]
        print(f"{tag} setup {'ok' if ok else 'FAILED'}  " + "  ".join(f"{d}: {r!r}" for d, r in res))
        hits.extend(f"{tag} {d} {'OK' if r == MSG else 'partial'} {r!r}" for d, r in res if r)

    if what in ("query", "all"):
        print("== NEW on the field settings (baseline)")
        test(*FIELD, query=True)
    if what in ("modes", "all"):
        print("== NEW: every mode x baud, on C012")
        for fu, bauds in ((3, BAUDS), (1, BAUDS), (2, BAUDS[:3]), (4, BAUDS[:1])):
            for b in bauds:
                test(fu, b, 12)
    if what in ("channels", "all"):
        print("== NEW: channels 1..100, FU3 2400 (frequency offset?)")
        for ch in range(1, 101):
            test(3, 2400, ch)
    if what.startswith("live"):  # live[:seconds]: both on the field settings, one packet each way per second
        new.setup(*FIELD)
        t0 = time.time()
        while time.time() - t0 < float(what.partition(":")[2] or 120):
            n2o, o2n = send(new, old), send(old, new)
            print(f"{time.time() - t0:5.0f}s  new->old {'OK ' if n2o == MSG else '-- '}  old->new "
                  f"{'OK ' if o2n == MSG else '-- '} {n2o!r} {o2n!r}", flush=True)
    print("\nHITS (anything received):", *(hits or ["none"]), sep="\n  ")


if __name__ == "__main__":
    main()
