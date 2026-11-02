// remote_v2 full routing, 2 layers, NO ground pour (user: keep HC-12 antenna clear; GND routed as tracks)
drc({ trackWidthMm: 0.254, minTrackWidthMm: 0.2, clearanceMm: 0.2, edgeClearanceMm: 0.3, via: { diameterMm: 0.62, drillMm: 0.31 } });
powerNet("BAT_IN", { maxCurrentA: 0.8, priority: "critical" });
powerNet("VBAT", { maxCurrentA: 0.8, priority: "critical" });
powerNet("LX1", { maxCurrentA: 1.2, priority: "critical", viaPreference: "forbid" });
powerNet("LX2", { maxCurrentA: 1.2, priority: "critical", viaPreference: "forbid" });
powerNet("VCC", { maxCurrentA: 0.5, priority: "high" });
powerNet("GND", { maxCurrentA: 0.8, priority: "high" });
powerNet("VRF", { maxCurrentA: 0.25 });
powerNet("VOLED", { maxCurrentA: 0.1 });
signalNet("FB", { priority: "critical", viaPreference: "avoid" });
runAll();
