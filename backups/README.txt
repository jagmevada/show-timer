Deployed panel (address 2) read over UPDI on 2026-11-03, before reflashing.
Fuses: 00 00 01 FF FF F6 04 00 00 (OSCCFG 01 = 16 MHz osc, for the 8 MHz build).
Restore: pymcuprog write --erase -t uart -u COM3 -d attiny3226 -c 115200 -f panel_addr2_deployed_flash_2026-11-03.hex
         pymcuprog write -m eeprom -t uart -u COM3 -d attiny3226 -c 115200 -f panel_addr2_deployed_eeprom_2026-11-03.hex
