// Panel link protocol. MUST stay byte-identical to the panel receiver:
// github.com/jagmevada/jj-timer-remote, branch timer-boilerplate, timer/src/functions.hpp (recieveprocess).
// HC-12: channel 10 + address (011), 2400 baud, power P8. The panel echoes the command byte as the ACK.
#pragma once
#include <stdint.h>

enum { CMD_RESTART = 0, CMD_RESUME = 1, CMD_HOLD = 2, CMD_SET_RESUME = 3, CMD_STATUS = 4, CMD_NEWDATA = 5, CMD_EEPROM = 6 };
// Per set (10 panel+remote sets, 1..10): the panel shows its address as 00:NN at power-up. Flash a remote for it with
//   PLATFORMIO_BUILD_FLAGS=-DREMOTE_ADDR=2 pio run -t upload      (no flag = 1; same for -e panel_sim)
#ifndef REMOTE_ADDR
#define REMOTE_ADDR 1
#endif
static_assert(REMOTE_ADDR >= 1 && REMOTE_ADDR <= 15, "address is 4 bits");
const uint8_t MYADDRESS = REMOTE_ADDR;
const uint8_t RF_CH = 10 + MYADDRESS;  // HC-12 channel, as the panel and old remote derive it (address 5 -> 015)

inline uint8_t parity8(uint8_t d) {
  d ^= d >> 4;
  d ^= d >> 2;
  d ^= d >> 1;
  return d & 1;
}

// Command byte: bit 0 = even-parity bit, bits 1-4 = address, bits 5-7 = command.
inline uint8_t cmdByte(uint8_t cmd) {
  uint8_t b = (uint8_t)(cmd << 5) | (uint8_t)(MYADDRESS << 1);
  return b | parity8(b);
}

// Payload byte (minutes or seconds, 0..99): bit 7 makes the byte odd parity.
inline uint8_t payByte(uint8_t v) { return v | (uint8_t)(!parity8(v) << 7); }

// SET_RESUME and EEPROM carry minutes then seconds. Returns the frame length (1 or 3).
inline uint8_t frame(uint8_t *buf, uint8_t cmd, uint8_t minutes, uint8_t seconds) {
  buf[0] = cmdByte(cmd);
  if (cmd != CMD_SET_RESUME && cmd != CMD_EEPROM) return 1;
  buf[1] = payByte(minutes);
  buf[2] = payByte(seconds);
  return 3;
}
