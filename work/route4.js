// remote_v2: close the last 4 open connections only
drc({ trackWidthMm: 0.254, minTrackWidthMm: 0.2, clearanceMm: 0.2, edgeClearanceMm: 0.3, via: { diameterMm: 0.62, drillMm: 0.31 } });
onlyNets("VCC", "VBAT", "BTN_PWR", "BTN_OK");
powerNet("VBAT", { maxCurrentA: 0.8, priority: "critical" });
powerNet("VCC", { maxCurrentA: 0.5, priority: "high" });
runAll();
