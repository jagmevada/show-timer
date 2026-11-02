// remote_v2: route everything except the hand-routed converter core (LX1/LX2/FB done by hand; VCC/VBAT/GND continue from hand copper). NO ground pour.
drc({ trackWidthMm: 0.254, minTrackWidthMm: 0.2, clearanceMm: 0.2, edgeClearanceMm: 0.3, via: { diameterMm: 0.62, drillMm: 0.31 } });
ignoreNets("LX1", "LX2", "FB");
powerNet("BAT_IN", { maxCurrentA: 0.8, priority: "critical" });
powerNet("VBAT", { maxCurrentA: 0.8, priority: "critical" });
powerNet("VCC", { maxCurrentA: 0.5, priority: "high" });
powerNet("GND", { maxCurrentA: 0.8, priority: "high" });
powerNet("VRF", { maxCurrentA: 0.25 });
powerNet("VOLED", { maxCurrentA: 0.1 });
runAll();
