// remote_v2 route9: re-route the J4/J5 header nets (+VCC reconnect) after clearance errors
drc({ trackWidthMm: 0.254, minTrackWidthMm: 0.2, clearanceMm: 0.2, edgeClearanceMm: 0.3, via: { diameterMm: 0.62, drillMm: 0.31 } });
onlyNets("UPDI_J", "DBG_TX", "DBG_RX", "VCC");
powerNet("VCC", { maxCurrentA: 0.5, priority: "high", viaPreference: "avoid" });
signalNet("UPDI_J", { viaPreference: "avoid" });
signalNet("DBG_TX", { viaPreference: "avoid" });
signalNet("DBG_RX", { viaPreference: "avoid" });
runAll();
