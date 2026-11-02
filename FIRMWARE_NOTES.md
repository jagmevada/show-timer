# remote_v2: firmware notes (what the hardware leaves to software, and why)

MCU: ATtiny3226-SU (megaTinyCore or bare AVR). VCC = 3.37 V, always on while J6 is closed.
Firmware is **not written yet**.

## 1. Pin map (verified against DS40002345B Table 3-1, SOIC-20, and the live netlist)
| Pin | Port | Net | Dir | Use |
|---|---|---|---|---|
| 1 | VDD | VCC | — | |
| 2 | PA4 | BTN_UP | in | active LOW (10k ext pull-up, 1k + 100 nF RC) |
| 3 | PA5 | BTN_DOWN | in | active LOW |
| 4 | PA6 | BTN_PWR | in | active LOW. **Fully-async pin: wakes from power-down** |
| 5 | PA7 | VBAT_SNS | analog | AIN7, divider 330k/1M |
| 6 | PB5 | RF_EN | out | **LOW = HC-12 powered** (Q2) |
| 7 | PB4 | OLED_EN | out | **LOW = OLED powered** (Q3) |
| 8 | PB3 | HC_RX | in | **USART0 RxD, DEFAULT route** ← HC-12 TXD (pin 4) via 1k |
| 9 | PB2 | HC_TX | out | **USART0 TxD, DEFAULT route** → HC-12 RXD (pin 3) via 1k |
| 10 | PB1 | SDA | i/o | TWI0 default |
| 11 | PB0 | SCL | out | TWI0 default |
| 12 | PC0 | BTN_LEFT | in | active LOW |
| 13 | PC1 | HC_SET | out | HC-12 SET via 1k (LOW = AT command mode) |
| 14 | PC2 | BTN_RIGHT | in | active LOW |
| 15 | PC3 | BTN_OK | in | active LOW |
| 16 | PA0 | UPDI | — | programming (keep the UPDI fuse) |
| 17 | PA1 | DBG_TX | out | USART1 TxD default → J5 pin 2 "T" (to the USB-UART's RX) |
| 18 | PA2 | DBG_RX | in | USART1 RxD default ← J5 pin 3 "R" |
| 19 | PA3 | LED_A | out | HIGH = status LED on |
| 20 | GND | GND | — | |
PORTMUX: USARTROUTEA = 0 (both default). TWI0 default. No crystal: PB2/PB3 are USART0, not TOSC.

## 2. Power states (user concept)
- **ON:** hold PWR 1 s → OLED on (PB4 LOW) → LED on (PA3 HIGH) → show the countdown. HC-12 stays **off**.
- **Command** (start / stop / pause / set): PB5 LOW → wait ~80 ms (HC-12 boot) → UART send `cmd + seq` → wait for the panel ACK ≤ 300 ms, retry ×3 → PB5 HIGH.
  - OLED shows ✓ or ✗.
  - **Before PB5 HIGH (off):** drive PB2 (TX) and PC1 (SET) LOW, or make them inputs, so the unpowered HC-12 isn't fed through its pins (the 1k resistors limit it, but don't rely on that).
- **OLED blanking:** during the long countdown (~20 min) turn the OLED off (SSD1306 display-off, or PB4 HIGH with SDA/SCL LOW first). Any button wakes it.
- **OFF:** hold PWR 1 s again, or **auto-off after 5 h**:
  1. OLED off, then I2C pins LOW, then PB4 HIGH.
  2. HC-12 off (PB5 HIGH).
  3. LED off.
  4. PA6 interrupt on falling edge (async).
  5. SLEEP_MODE_PWR_DOWN.
  - Expected ~15 µA total (TPS631000 Iq dominates). The BOD should be disabled or at the lowest level in sleep.
- **J6:** a hardware cut. The firmware can't see it; with J6 open nothing runs.

## 3. Panel protocol (to define with the panel firmware)
- The remote sends start/stop/pause/set with a sequence number; the panel replies with an ACK.
- **The ACK should carry the panel's remaining seconds.** The remote adopts it, which corrects the internal-oscillator drift (±2 %, ~1 min/h) at no extra traffic.
- HC-12 mode FU3 (default) is fine: the radio is only on per command. Both ends must use the same channel and baud. Configure once via SET (PC1 LOW → AT commands).

## 4. Buttons
- Hardware: 1k + 100 nF + 10k pull-up per button. Pressed ≤ 0.31 V, release ~1 ms. Still add ~20–30 ms software debounce.
- PWR: 1 s hold = toggle on/off. Short press while on = screen on/off (user: "power button is just screen off and screen on").
- No internal pull-ups needed (external 10k fitted). They are harmless if enabled.

## 5. Battery gauge
- ADC: AIN7, VREF = internal 2.5 V (needs VDD ≥ 2.7 V: OK, VCC = 3.37 V), long SAMPLEN (source impedance ~250 k), average 4–8 readings.
- **VBAT = Vadc × (330k + 1M) / 1M = Vadc × 1.33.** 12-bit: VBAT = raw × 2.5 / 4095 × 1.33. Full scale 3.32 V.
- 2×AAA alkaline: 3.2 V fresh, ~2.4 V "low" warning, ~2.0 V empty (the converter keeps working down to ~1.6 V).

## 6. Bring-up checks (firmware side)
1. Program via UPDI (J4: V G U; power the board from the battery with J6 closed).
2. Debug print on USART1 (J5 T/R, 3.3 V logic).
3. Toggle PB5 / PB4 and measure VRF / VOLED = 3.37 V ↔ 0 V.
4. I2C scan: OLED at 0x3C. HC-12: AT → OK (PC1 LOW).
5. Buttons: each reads LOW when pressed.
