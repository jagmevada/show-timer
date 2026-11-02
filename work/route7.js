// remote_v2: reconnect VCC only (after removing a via that crowded LX2)
drc({ trackWidthMm: 0.254, minTrackWidthMm: 0.2, clearanceMm: 0.2, edgeClearanceMm: 0.3, via: { diameterMm: 0.62, drillMm: 0.31 } });
onlyNets("VCC");
powerNet("VCC", { maxCurrentA: 0.5, priority: "high", viaPreference: "avoid" });
runAll();
