# show-timer

Wireless countdown timer for shows: a big LED **panel** and a handheld **remote**, linked by an HC-12 radio.
There are 10 sets in use, on addresses 1 to 10.

**Everything lives here now.** One branch per part:

| Branch | What | Status |
|---|---|---|
| `panel` | Panel (receiver) firmware in `timer/`, the code on the panels. `remote/` is the 2023 remote it shipped with (reference only). | Deployed. Git history from 2023 kept. |
| `remote` | remote_v2 firmware (ATtiny3226, new PCB). Also `panel_sim`, which turns a remote_v2 board into a panel for bench tests, and `bringup/` test firmware. | Working against a real panel, 2026-11 |
| `pcb` | remote_v2 PCB: EasyEDA project, Gerber / BOM / CPL, design notes, datasheets. Start at `CLAUDE.md`. | Boards built, bring-up 2026-11 |

## Radio protocol (do not change: 10 panels are in the field)

- HC-12: channel = 10 + address (address 2 = `AT+C012`), 2400 baud, power P8.
- Command byte:
  - bit 0 = even parity, bits 1–4 = address, bits 5–7 = command.
  - Commands: RESTART 0, RESUME 1, HOLD 2, SET_RESUME 3, STATUS 4, NEWDATA 5, EEPROM 6.
- SET_RESUME and EEPROM add 2 bytes, minutes then seconds, each with bit 7 = odd parity. Send all 3 bytes in one burst: the panel reads the payload 10 ms after the command byte.
- ACK: the panel sends the command byte back, up to ~1 s later.
- At power-up the panel shows its address as `00:NN`. On the first command it loads its saved time and starts counting.

**Address per set:** panel `timer/src/functions.hpp` `MYADDRESS`; remote `src/protocol.h`.
Flash a remote for a set without editing source:
`PLATFORMIO_BUILD_FLAGS=-DREMOTE_ADDR=2 pio run -t upload`

## Where the old code was (don't develop there)

| Old place | What it was |
|---|---|
| jagmevada/jj-timer-remote, branch `timer-boilerplate` (fork of dbf-jjd/jj-timer-remote) | Source of the `panel` branch: commit 78cb7f6, 2023-11-06, PR #9 "Final timer code". |
| dbf-jjd/jj-timer-remote `main`, `refactor/simplify-readability` | Same panel code without the last fix ("pause on next show"). |
| jj-timer-remote `remote-rf-testing`, `remote-down-counter`, `timer`, fork `main` | Older: an obsolete protocol (address 10, channel 030) or no radio yet. |
| jagmevada/timer_remote | Eagle hardware for the 2024 remote board (no code). |
| jagmevada/timer_remote_code | 2024–25 firmware for that remote board. The newest build (2025-11) was never committed. |
| Local copies under D:\Project\JJ116 | Older panel commits. |
