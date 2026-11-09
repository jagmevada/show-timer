// Host check that our frames decode exactly as the panel decodes them.
// Run: g++ -std=c++11 -Isrc test/test_protocol.cpp -o test_protocol && ./test_protocol
#include <cassert>
#include <cstdio>
#include "protocol.h"

// Copied from the old remote / panel (jj-timer-remote timer-boilerplate): bit-field layout.
typedef struct {
  unsigned int parity : 1;
  unsigned int address : 4;
  unsigned int command : 3;
} rxbyte;
typedef union {
  unsigned char byte;
  rxbyte fields;
} rxdata;

int main() {
  for (uint8_t a = 1; a <= 15; a++)
    for (uint8_t c = 0; c < 8; c++) {
      rxdata old = {};  // old remote's encoder
      old.fields.address = a;
      old.fields.command = c;
      old.fields.parity = parity8(old.byte);
      assert(cmdByte(c, a) == old.byte);

      rxdata r = {};  // panel's decoder
      r.byte = cmdByte(c, a);
      assert(parity8(r.byte) == 0 && r.fields.address == a && r.fields.command == c);
    }
  for (uint8_t m = 0; m < 100; m++)
    for (uint8_t s = 0; s < 60; s++) {
      uint8_t f[3];
      assert(frame(f, CMD_SET_RESUME, 2, m, s) == 3 && frame(f, CMD_EEPROM, 2, m, s) == 3);
      assert(parity8(f[1]) == 1 && parity8(f[2]) == 1);                  // panel's payload check
      assert((f[1] & 0x7f) * 60 + (f[2] & 0x7f) == m * 60 + s);          // panel's time decode
    }
  uint8_t f[3];
  assert(frame(f, CMD_HOLD, 2, 1, 2) == 1 && frame(f, CMD_RESTART, 2, 1, 2) == 1);
  printf("protocol OK, addresses 1..15. Address 2: HOLD=0x%02X SET=0x%02X RESTART=0x%02X EEPROM=0x%02X\n",
         cmdByte(CMD_HOLD, 2), cmdByte(CMD_SET_RESUME, 2), cmdByte(CMD_RESTART, 2), cmdByte(CMD_EEPROM, 2));
}
