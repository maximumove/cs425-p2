# Submission Report

- Submission generated at 10/07/2026 at 02:58:06

- Machine info: Linux runnervmmprz5 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

# Project 2: Reliable Data Transfer

- Name: Troy Berhow
- Email: troyberhow@u.boisestate.edu
- Class: CS525-001

## Known Bugs or Issues

N/A

## Experience

This project was the first were the use of AI for most of the project was activly encouraged, and with that being the case right out of the gate I could plan around it. I spend a few hours planning out how I wanted things to go, and then feed those into claude (with whom I have a pro subscription) and within 30 minutes the project was built. I even ran a manual test to make sure the unity tests were not baised, and it worked. Using AI the way I would in the industry was rather eye opening, and if anything excites me quite a lot. The actual coding was always the most annoying part of Computer Science, so offloading most of that made it much more fun.

As for the pre prompt planning, since the assignment was very clear in what it wanted, and the chapter of the book it covered had already been tested, and thus I knew it well, it was pretty easy overall. Most of it was spent looking at details and understanding why it worked rather than trying to figure out what would work. Again, I found this to be much more enjoyable. I encounted very little struggle with this assignment, which was delightful.

## Results

A 1 MiB file (`head -c 1048576 /dev/urandom > 1mib.bin`) sent through the relay
started with `--delay 50` (100 ms round trip), with the default 250 ms timeout. Each
combination was run three times, every received copy matched the original under
`cmp`, and each run was timed with `time`. `scripts/run-results.sh` reproduces it.

| Window | Loss | Corrupt | Dup | Run times (s)            | Mean time (s) | Throughput (KiB/s) |
|-------:|-----:|--------:|----:|--------------------------|--------------:|-------------------:|
| 1      | 0    | 0       | 0   | 103.831, 103.925, 105.764 | 104.507       | 9.80               |
| 16     | 0    | 0       | 0   | 6.654, 6.630, 6.632       | 6.639         | 154.25             |
| 1      | 0.05 | 0       | 0   | 132.144, 131.532, 129.076 | 130.917       | 7.82               |
| 16     | 0.05 | 0       | 0   | 24.188, 21.669, 23.396    | 23.084        | 44.36              |

1. Round-trip time at window 1, no loss

Mean time 104.507 s ÷ 1025 packets ≈ 102.0 ms per round trip. The relay's --delay 50 accounts for 100 ms of that, 50 ms each way. The other ~2 ms is overhead that the relay delay doesn't include:

- The relay itself. cs425_relay.py is a Python select loop that holds packets in a heap. It wakes up, compares the due time with time.monotonic(), and then sends. Each packet goes through it twice, once as DATA and once as the ACK, so its scheduling delay and timer lateness count twice per round trip. Python's per-packet processing time is added too.
- The endpoints. Each round trip includes four UDP send and receive system calls, plus the work in our own code: building the packet, computing the checksum, processing the ACK, and the sender's wait loop and OS scheduler waking it up.
- Fixed costs spread across all packets. Session setup (HELLO), the FIN exchange, reading the file and process startup are averaged into the per-packet figure.

All of these are small, but each one happens on every round trip, so they add up to roughly 2%.

2. Speedup at window 16

104.507 / 6.639 ≈ 15.7×, which is close to 16 but a little under. With no loss, Go-Back-N with window 16 sends 16 packets, and the ACKs come back about one RTT later. The sender's time is still almost entirely round trips. Only the number of packets moved per round trip has changed:

- ⌈1025 / 16⌉ = 65 rounds × 102 ms ≈ 6.63 s, which matches the measured 6.639 s almost exactly.
- It falls short of 16× because 1025 packets do not divide evenly by 16. The last round carries only 1 packet but still costs a full RTT, so the run takes 65 rounds instead of 64.06. 1025 / 65 ≈ 15.8×, which is the speedup measured.
- Putting 16 packets on the wire takes microseconds compared with a 100 ms RTT, so it costs almost nothing. The link is far from full: 154 KiB/s is well under the relay's 5000-packets-per-second limit. Only the RTT limits speed, so with no loss, throughput grows almost linearly with the window.

3. Why 5% loss hurts window 16 more

- Window 1: 104.5 → 130.9 s, +25%
- Window 16: 6.6 → 23.1 s, +248%

The difference is how much each timeout costs compared with the useful work in one round:

- Window 1. Only one packet is ever outstanding. If the DATA packet or its ACK is lost (about 1 − 0.95² ≈ 10% per attempt), the sender waits 250 ms and resends that one packet. That works out to about 110 timeouts × 0.25 s ≈ 27 s, which matches the 26 s the run gained. A normal round already costs 102 ms to move one packet, so an extra 250 ms now and then is a modest relative penalty.
- Window 16. A normal round moves 16 packets in 102 ms, so the 250 ms timeout leaves the pipe empty for about 2.5 rounds, roughly 40 packets' worth of time. Our sender then follows Go-Back-N (lab.c:231, "go back: resend the whole window") and resends every packet from base through next. That is up to 16 packets, including ones that had arrived fine but that the receiver discarded because they came in out of order. Those resent packets can also be lost, which causes another timeout. In addition, at least one of 16 packets is lost in 1 − 0.95¹⁶ ≈ 56% of rounds, so most rounds end in a timeout instead of finishing in 102 ms.

One factor helps window 16: cumulative ACKs mean a lost ACK is usually covered by a later one. That does not make up for the cost of each timeout, which is larger here and happens far more often.

In short, a timeout at window 1 loses one packet and a little time. At window 16 it costs a whole window of packets plus several rounds of time with nothing sent.

---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/lab.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/lab.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/lab.c.o build/tests/main.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
make[1]: Entering directory '/home/runner/work/cs425-p2/cs425-p2'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/lab.c.o build/debug-test/main.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs425-p2/cs425-p2'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
tests/lab-test.c:1086:test_checksum_rfc1071_example:PASS
tests/lab-test.c:1087:test_checksum_odd_length_pads_with_zero:PASS
tests/lab-test.c:1088:test_checksum_folds_carry:PASS
tests/lab-test.c:1089:test_checksum_empty:PASS
tests/lab-test.c:1090:test_checksum_catches_every_single_bit_flip:PASS
tests/lab-test.c:1091:test_encode_worked_example_hi:PASS
tests/lab-test.c:1092:test_encode_worked_example_ack3:PASS
tests/lab-test.c:1093:test_make_ack:PASS
tests/lab-test.c:1094:test_encode_full_payload_and_large_seq:PASS
tests/lab-test.c:1095:test_encode_rejects_bad_input:PASS
tests/lab-test.c:1096:test_decode_round_trip:PASS
tests/lab-test.c:1097:test_decode_too_short:PASS
tests/lab-test.c:1098:test_decode_length_mismatch:PASS
tests/lab-test.c:1099:test_decode_length_field_over_max:PASS
tests/lab-test.c:1100:test_decode_unknown_type_and_reserved:PASS
tests/lab-test.c:1101:test_decode_bad_checksum:PASS
tests/lab-test.c:1102:test_sender_init_counts_packets:PASS
tests/lab-test.c:1103:test_sender_init_rejects_bad_args:PASS
tests/lab-test.c:1104:test_sender_packet_cuts_file:PASS
tests/lab-test.c:1105:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:1106:test_sender_empty_file:PASS
tests/lab-test.c:1107:test_sender_window_full:PASS
tests/lab-test.c:1108:test_sender_stop_and_wait:PASS
tests/lab-test.c:1109:test_sender_cumulative_ack_slides_several:PASS
tests/lab-test.c:1110:test_sender_duplicate_ack_ignored:PASS
tests/lab-test.c:1111:test_sender_ack_beyond_next_ignored:PASS
tests/lab-test.c:1112:test_sender_timeout_resends_whole_window:PASS
tests/lab-test.c:1113:test_sender_gives_up_after_ten_timeouts:PASS
tests/lab-test.c:1114:test_sender_progress_resets_timeout_count:PASS
tests/lab-test.c:1115:test_sender_timer_stops_when_all_acked:PASS
tests/lab-test.c:1116:test_receiver_in_order:PASS
tests/lab-test.c:1117:test_receiver_duplicate:PASS
tests/lab-test.c:1118:test_receiver_beyond_gap:PASS
tests/lab-test.c:1119:test_receiver_fin_and_repeated_fin:PASS
tests/lab-test.c:1120:test_receiver_empty_file:PASS
tests/lab-test.c:1121:test_receiver_ignores_ack:PASS
tests/lab-test.c:1122:test_receiver_linger_then_done:PASS
tests/lab-test.c:1123:test_receiver_idle_gives_up:PASS
tests/lab-test.c:1124:test_transfer_clean_channel:PASS
tests/lab-test.c:1125:test_transfer_lossy_channel_many_seeds:PASS
tests/lab-test.c:1126:test_transfer_lossy_channel_windows_and_sizes:PASS
tests/lab-test.c:1127:test_transfer_dead_channel_gives_up:PASS
tests/lab-test.c:1128:test_usage_prints:PASS
tests/lab-test.c:1129:test_parse_args_send_defaults:PASS
tests/lab-test.c:1130:test_parse_args_send_all_options:PASS
tests/lab-test.c:1131:test_parse_args_recv:PASS
tests/lab-test.c:1132:test_parse_args_errors:PASS
tests/lab-test.c:1133:test_valid_session:PASS
tests/lab-test.c:1134:test_format_hello:PASS
tests/lab-test.c:1135:test_parse_reply:PASS

-----------------------
50 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
tests/lab-test.c:1086:test_checksum_rfc1071_example:PASS
tests/lab-test.c:1087:test_checksum_odd_length_pads_with_zero:PASS
tests/lab-test.c:1088:test_checksum_folds_carry:PASS
tests/lab-test.c:1089:test_checksum_empty:PASS
tests/lab-test.c:1090:test_checksum_catches_every_single_bit_flip:PASS
tests/lab-test.c:1091:test_encode_worked_example_hi:PASS
tests/lab-test.c:1092:test_encode_worked_example_ack3:PASS
tests/lab-test.c:1093:test_make_ack:PASS
tests/lab-test.c:1094:test_encode_full_payload_and_large_seq:PASS
tests/lab-test.c:1095:test_encode_rejects_bad_input:PASS
tests/lab-test.c:1096:test_decode_round_trip:PASS
tests/lab-test.c:1097:test_decode_too_short:PASS
tests/lab-test.c:1098:test_decode_length_mismatch:PASS
tests/lab-test.c:1099:test_decode_length_field_over_max:PASS
tests/lab-test.c:1100:test_decode_unknown_type_and_reserved:PASS
tests/lab-test.c:1101:test_decode_bad_checksum:PASS
tests/lab-test.c:1102:test_sender_init_counts_packets:PASS
tests/lab-test.c:1103:test_sender_init_rejects_bad_args:PASS
tests/lab-test.c:1104:test_sender_packet_cuts_file:PASS
tests/lab-test.c:1105:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:1106:test_sender_empty_file:PASS
tests/lab-test.c:1107:test_sender_window_full:PASS
tests/lab-test.c:1108:test_sender_stop_and_wait:PASS
tests/lab-test.c:1109:test_sender_cumulative_ack_slides_several:PASS
tests/lab-test.c:1110:test_sender_duplicate_ack_ignored:PASS
tests/lab-test.c:1111:test_sender_ack_beyond_next_ignored:PASS
tests/lab-test.c:1112:test_sender_timeout_resends_whole_window:PASS
tests/lab-test.c:1113:test_sender_gives_up_after_ten_timeouts:PASS
tests/lab-test.c:1114:test_sender_progress_resets_timeout_count:PASS
tests/lab-test.c:1115:test_sender_timer_stops_when_all_acked:PASS
tests/lab-test.c:1116:test_receiver_in_order:PASS
tests/lab-test.c:1117:test_receiver_duplicate:PASS
tests/lab-test.c:1118:test_receiver_beyond_gap:PASS
tests/lab-test.c:1119:test_receiver_fin_and_repeated_fin:PASS
tests/lab-test.c:1120:test_receiver_empty_file:PASS
tests/lab-test.c:1121:test_receiver_ignores_ack:PASS
tests/lab-test.c:1122:test_receiver_linger_then_done:PASS
tests/lab-test.c:1123:test_receiver_idle_gives_up:PASS
tests/lab-test.c:1124:test_transfer_clean_channel:PASS
tests/lab-test.c:1125:test_transfer_lossy_channel_many_seeds:PASS
tests/lab-test.c:1126:test_transfer_lossy_channel_windows_and_sizes:PASS
tests/lab-test.c:1127:test_transfer_dead_channel_gives_up:PASS
tests/lab-test.c:1128:test_usage_prints:PASS
tests/lab-test.c:1129:test_parse_args_send_defaults:PASS
tests/lab-test.c:1130:test_parse_args_send_all_options:PASS
tests/lab-test.c:1131:test_parse_args_recv:PASS
tests/lab-test.c:1132:test_parse_args_errors:PASS
tests/lab-test.c:1133:test_valid_session:PASS
tests/lab-test.c:1134:test_format_hello:PASS
tests/lab-test.c:1135:test_parse_reply:PASS

-----------------------
50 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                    307      307   100%
------------------------------------------------------------------------------
TOTAL                                        307      307   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
tests/lab-test.c:1086:test_checksum_rfc1071_example:PASS
tests/lab-test.c:1087:test_checksum_odd_length_pads_with_zero:PASS
tests/lab-test.c:1088:test_checksum_folds_carry:PASS
tests/lab-test.c:1089:test_checksum_empty:PASS
tests/lab-test.c:1090:test_checksum_catches_every_single_bit_flip:PASS
tests/lab-test.c:1091:test_encode_worked_example_hi:PASS
tests/lab-test.c:1092:test_encode_worked_example_ack3:PASS
tests/lab-test.c:1093:test_make_ack:PASS
tests/lab-test.c:1094:test_encode_full_payload_and_large_seq:PASS
tests/lab-test.c:1095:test_encode_rejects_bad_input:PASS
tests/lab-test.c:1096:test_decode_round_trip:PASS
tests/lab-test.c:1097:test_decode_too_short:PASS
tests/lab-test.c:1098:test_decode_length_mismatch:PASS
tests/lab-test.c:1099:test_decode_length_field_over_max:PASS
tests/lab-test.c:1100:test_decode_unknown_type_and_reserved:PASS
tests/lab-test.c:1101:test_decode_bad_checksum:PASS
tests/lab-test.c:1102:test_sender_init_counts_packets:PASS
tests/lab-test.c:1103:test_sender_init_rejects_bad_args:PASS
tests/lab-test.c:1104:test_sender_packet_cuts_file:PASS
tests/lab-test.c:1105:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:1106:test_sender_empty_file:PASS
tests/lab-test.c:1107:test_sender_window_full:PASS
tests/lab-test.c:1108:test_sender_stop_and_wait:PASS
tests/lab-test.c:1109:test_sender_cumulative_ack_slides_several:PASS
tests/lab-test.c:1110:test_sender_duplicate_ack_ignored:PASS
tests/lab-test.c:1111:test_sender_ack_beyond_next_ignored:PASS
tests/lab-test.c:1112:test_sender_timeout_resends_whole_window:PASS
tests/lab-test.c:1113:test_sender_gives_up_after_ten_timeouts:PASS
tests/lab-test.c:1114:test_sender_progress_resets_timeout_count:PASS
tests/lab-test.c:1115:test_sender_timer_stops_when_all_acked:PASS
tests/lab-test.c:1116:test_receiver_in_order:PASS
tests/lab-test.c:1117:test_receiver_duplicate:PASS
tests/lab-test.c:1118:test_receiver_beyond_gap:PASS
tests/lab-test.c:1119:test_receiver_fin_and_repeated_fin:PASS
tests/lab-test.c:1120:test_receiver_empty_file:PASS
tests/lab-test.c:1121:test_receiver_ignores_ack:PASS
tests/lab-test.c:1122:test_receiver_linger_then_done:PASS
tests/lab-test.c:1123:test_receiver_idle_gives_up:PASS
tests/lab-test.c:1124:test_transfer_clean_channel:PASS
tests/lab-test.c:1125:test_transfer_lossy_channel_many_seeds:PASS
tests/lab-test.c:1126:test_transfer_lossy_channel_windows_and_sizes:PASS
tests/lab-test.c:1127:test_transfer_dead_channel_gives_up:PASS
tests/lab-test.c:1128:test_usage_prints:PASS
tests/lab-test.c:1129:test_parse_args_send_defaults:PASS
tests/lab-test.c:1130:test_parse_args_send_all_options:PASS
tests/lab-test.c:1131:test_parse_args_recv:PASS
tests/lab-test.c:1132:test_parse_args_errors:PASS
tests/lab-test.c:1133:test_valid_session:PASS
tests/lab-test.c:1134:test_format_hello:PASS
tests/lab-test.c:1135:test_parse_reply:PASS

-----------------------
50 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

#include "lab.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Layer 1: packets                                                         */
/* ------------------------------------------------------------------------ */

uint16_t rdt_checksum(const uint8_t *buf, size_t len)
{
  uint32_t sum = 0;
  size_t i = 0;
  for (; i + 1 < len; i += 2)
  {
    sum += (uint32_t)((uint32_t)buf[i] << 8 | buf[i + 1]);
  }
  if (i < len)
  {
    sum += (uint32_t)buf[i] << 8; /* pad the odd byte with a zero */
  }
  while (sum >> 16)
  {
    sum = (sum & 0xffffu) + (sum >> 16);
  }
  return (uint16_t)~sum;
}

size_t rdt_encode(const rdt_packet *pkt, uint8_t *buf, size_t cap)
{
  if (pkt == NULL || buf == NULL)
  {
    return 0;
  }
  if (pkt->type != RDT_DATA && pkt->type != RDT_ACK && pkt->type != RDT_FIN)
  {
    return 0;
  }
  if (pkt->length > RDT_MAX_PAYLOAD)
  {
    return 0;
  }
  size_t total = RDT_HEADER_LEN + pkt->length;
  if (total > cap)
  {
    return 0;
  }

  uint16_t zero = 0;
  uint32_t seq = htonl(pkt->seq);
  uint16_t length = htons(pkt->length);
  buf[0] = (uint8_t)pkt->type;
  buf[1] = 0;
  memcpy(buf + 2, &zero, sizeof zero);
  memcpy(buf + 4, &seq, sizeof seq);
  memcpy(buf + 8, &length, sizeof length);
  if (pkt->length > 0)
  {
    memcpy(buf + RDT_HEADER_LEN, pkt->payload, pkt->length);
  }

  uint16_t sum = htons(rdt_checksum(buf, total));
  memcpy(buf + 2, &sum, sizeof sum);
  return total;
}

bool rdt_decode(const uint8_t *buf, size_t len, rdt_packet *out)
{
  if (buf == NULL || out == NULL || len < RDT_HEADER_LEN)
  {
    return false;
  }

  uint16_t length;
  memcpy(&length, buf + 8, sizeof length);
  length = ntohs(length);
  /* Check the length field against what actually arrived before trusting
   * it for anything. */
  if (length > RDT_MAX_PAYLOAD || RDT_HEADER_LEN + length != len)
  {
    return false;
  }
  if (buf[0] > RDT_FIN || buf[1] != 0)
  {
    return false;
  }
  if (rdt_checksum(buf, len) != 0)
  {
    return false;
  }

  uint32_t seq;
  memcpy(&seq, buf + 4, sizeof seq);
  out->type = (rdt_type)buf[0];
  out->seq = ntohl(seq);
  out->length = length;
  if (length > 0)
  {
    memcpy(out->payload, buf + RDT_HEADER_LEN, length);
  }
  return true;
}

void rdt_make_ack(rdt_packet *pkt, uint32_t seq)
{
  pkt->type = RDT_ACK;
  pkt->seq = seq;
  pkt->length = 0;
}

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N sender                                                */
/* ------------------------------------------------------------------------ */

static void sender_report(const gbn_sender *s, uint32_t from, uint32_t to,
                          gbn_sender_action *act)
{
  act->send_from = from;
  act->send_to = to;
  act->timer_running = s->timer_running;
  act->deadline_ms = s->deadline_ms;
  act->status = s->status;
}

/* Send as much as the window allows. The FIN goes only once every DATA
 * packet is acknowledged. Returns the first packet newly sent; the caller
 * sends from there up to s->next. */
static uint32_t sender_fill(gbn_sender *s, uint64_t now_ms)
{
  uint32_t first = s->next;
  while (s->next < s->data_packets &&
         (uint64_t)s->next < (uint64_t)s->base + s->window)
  {
    s->next++;
  }
  if (s->next == s->data_packets && s->base == s->data_packets)
  {
    s->next++; /* the FIN */
  }
  if (s->next > first && !s->timer_running)
  {
    s->timer_running = true;
    s->deadline_ms = now_ms + s->timeout_ms;
  }
  return first;
}

bool gbn_sender_init(gbn_sender *s, const uint8_t *data, size_t size,
                     uint32_t window, uint64_t timeout_ms)
{
  if (s == NULL || (data == NULL && size > 0) || window < 1 ||
      window > GBN_MAX_WINDOW || timeout_ms == 0)
  {
    return false;
  }
  size_t packets = (size + RDT_MAX_PAYLOAD - 1) / RDT_MAX_PAYLOAD;
  if (packets >= UINT32_MAX)
  {
    return false; // GCOVR_EXCL_LINE: would need a 4 TiB file
  }
  memset(s, 0, sizeof *s);
  s->data = data;
  s->size = size;
  s->data_packets = (uint32_t)packets;
  s->window = window;
  s->timeout_ms = timeout_ms;
  s->status = GBN_RUNNING;
  return true;
}

void gbn_sender_start(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act)
{
  uint32_t first = sender_fill(s, now_ms);
  sender_report(s, first, s->next, act);
}

void gbn_sender_on_ack(gbn_sender *s, uint32_t ack_seq, uint64_t now_ms,
                       gbn_sender_action *act)
{
  if (s->status != GBN_RUNNING || ack_seq <= s->base || ack_seq > s->next)
  {
    /* duplicate, finished, or an ACK for something never sent */
    sender_report(s, s->next, s->next, act);
    return;
  }

  s->base = ack_seq;
  s->timeouts = 0;
  if (s->base == s->data_packets + 1)
  {
    /* the FIN is acknowledged */
    s->status = GBN_DONE;
    s->timer_running = false;
    sender_report(s, s->next, s->next, act);
    return;
  }

  if (s->base < s->next)
  {
    s->timer_running = true;
    s->deadline_ms = now_ms + s->timeout_ms;
  }
  else
  {
    s->timer_running = false;
  }
  uint32_t first = sender_fill(s, now_ms);
  sender_report(s, first, s->next, act);
}

void gbn_sender_on_tick(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act)
{
  if (s->status != GBN_RUNNING || !s->timer_running || now_ms < s->deadline_ms)
  {
    sender_report(s, s->next, s->next, act);
    return;
  }

  s->timeouts++;
  if (s->timeouts >= GBN_MAX_TIMEOUTS)
  {
    s->status = GBN_FAILED;
    s->timer_running = false;
    sender_report(s, s->next, s->next, act);
    return;
  }

  /* go back: resend the whole window */
  s->deadline_ms = now_ms + s->timeout_ms;
  sender_report(s, s->base, s->next, act);
}

bool gbn_sender_packet(const gbn_sender *s, uint32_t seq, rdt_packet *pkt)
{
  if (seq > s->data_packets)
  {
    return false;
  }
  if (seq == s->data_packets)
  {
    pkt->type = RDT_FIN;
    pkt->seq = seq;
    pkt->length = 0;
    return true;
  }
  size_t offset = (size_t)seq * RDT_MAX_PAYLOAD;
  size_t left = s->size - offset;
  size_t n = left < RDT_MAX_PAYLOAD ? left : RDT_MAX_PAYLOAD;
  pkt->type = RDT_DATA;
  pkt->seq = seq;
  pkt->length = (uint16_t)n;
  memcpy(pkt->payload, s->data + offset, n);
  return true;
}

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N receiver                                              */
/* ------------------------------------------------------------------------ */

void gbn_receiver_init(gbn_receiver *r, uint64_t now_ms)
{
  memset(r, 0, sizeof *r);
  r->last_activity_ms = now_ms;
}

void gbn_receiver_on_packet(gbn_receiver *r, const rdt_packet *pkt,
                            uint64_t now_ms, gbn_receiver_action *act)
{
  memset(act, 0, sizeof *act);
  r->last_activity_ms = now_ms;
  if (pkt->type == RDT_ACK)
  {
    return; /* nobody should send us these */
  }

  if (!r->finished && pkt->seq == r->expected)
  {
    if (pkt->type == RDT_DATA)
    {
      act->deliver = pkt->payload;
      act->deliver_len = pkt->length;
    }
    else
    {
      act->fin = true;
      r->finished = true;
      r->linger_until_ms = now_ms + GBN_LINGER_MS;
    }
    r->expected++;
  }

  /* In order or not, the reply is the same: everything below expected. */
  act->send_ack = true;
  act->ack_seq = r->expected;
}

uint64_t gbn_receiver_deadline(const gbn_receiver *r)
{
  return r->finished ? r->linger_until_ms : r->last_activity_ms + GBN_IDLE_MS;
}

gbn_status gbn_receiver_on_tick(const gbn_receiver *r, uint64_t now_ms)
{
  if (now_ms < gbn_receiver_deadline(r))
  {
    return GBN_RUNNING;
  }
  return r->finished ? GBN_DONE : GBN_FAILED;
}

/* ------------------------------------------------------------------------ */
/* Layer 3 helpers                                                          */
/* ------------------------------------------------------------------------ */

void app_usage(FILE *out)
{
  fprintf(out,
          "Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]\n"
          "                  [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
          "       myapp recv -s <session> [-p port] <relay> <file>\n"
          "  -s <session>     session name shared by the sender and the receiver\n"
          "  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)\n"
          "  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)\n"
          "  -l <loss>        probability the relay drops a packet (default: 0)\n"
          "  -c <corrupt>     probability the relay flips a bit (default: 0)\n"
          "  -d <dup>         probability the relay duplicates a packet (default: 0)\n"
          "  -p <port>        relay port (default: 4250)\n"
          "  <relay>          host name or address of the relay\n"
          "  <file>           file to send, or file to write what is received\n");
}

bool app_valid_session(const char *session)
{
  if (session == NULL)
  {
    return false;
  }
  size_t n = strlen(session);
  if (n < 1 || n > APP_SESSION_MAX)
  {
    return false;
  }
  for (size_t i = 0; i < n; i++)
  {
    char c = session[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
    {
      return false;
    }
  }
  return true;
}

static bool parse_ulong(const char *s, unsigned long lo, unsigned long hi,
                        unsigned long *out)
{
  if (s == NULL || *s < '0' || *s > '9')
  {
    return false; /* rejects "", "-1", " 1", "+1" */
  }
  char *end;
  errno = 0;
  unsigned long v = strtoul(s, &end, 10);
  if (errno != 0 || *end != '\0' || v < lo || v > hi)
  {
    return false;
  }
  *out = v;
  return true;
}

static bool parse_prob(const char *s, double *out)
{
  if (s == NULL || ((*s < '0' || *s > '9') && *s != '.'))
  {
    return false; /* rejects "", "-0.1", "nan", "inf" */
  }
  char *end;
  errno = 0;
  double v = strtod(s, &end);
  if (errno != 0 || *end != '\0' || !(v >= 0.0 && v <= 0.5))
  {
    return false;
  }
  *out = v;
  return true;
}

int app_parse_args(int argc, char *argv[], app_options *opts, FILE *err)
{
  opts->mode = APP_SEND;
  opts->session = NULL;
  opts->window = 8;
  opts->timeout_ms = 250;
  opts->loss = 0.0;
  opts->corrupt = 0.0;
  opts->dup = 0.0;
  opts->port = "4250";
  opts->relay = NULL;
  opts->file = NULL;

  if (argc < 2)
  {
    app_usage(err);
    return APP_EXIT_USAGE;
  }
  const char *optstring;
  if (strcmp(argv[1], "send") == 0)
  {
    opts->mode = APP_SEND;
    optstring = ":s:w:T:l:c:d:p:";
  }
  else if (strcmp(argv[1], "recv") == 0)
  {
    opts->mode = APP_RECV;
    optstring = ":s:p:";
  }
  else
  {
    fprintf(err, "myapp: unknown mode '%s'\n", argv[1]);
    app_usage(err);
    return APP_EXIT_USAGE;
  }

  /* Parse argv[1..] so the mode plays the role of the program name. */
  int sub_argc = argc - 1;
  char **sub_argv = argv + 1;
#ifdef __GLIBC__
  optind = 0; /* full reset, so the parser can run more than once */
#else
  optind = 1;
#endif
  opterr = 0;
  int c;
  unsigned long ul;
  while ((c = getopt(sub_argc, sub_argv, optstring)) != -1)
  {
    switch (c)
    {
    case 's':
      opts->session = optarg;
      break;
    case 'w':
      if (!parse_ulong(optarg, 1, GBN_MAX_WINDOW, &ul))
      {
        fprintf(err, "myapp: window must be 1 to %u: '%s'\n", GBN_MAX_WINDOW, optarg);
        return APP_EXIT_USAGE;
      }
      opts->window = (uint32_t)ul;
      break;
    case 'T':
      if (!parse_ulong(optarg, 1, 3600000, &ul))
      {
        fprintf(err, "myapp: timeout must be a positive number of ms: '%s'\n", optarg);
        return APP_EXIT_USAGE;
      }
      opts->timeout_ms = (uint32_t)ul;
      break;
    case 'l':
    case 'c':
    case 'd':
    {
      double *p = c == 'l' ? &opts->loss : c == 'c' ? &opts->corrupt : &opts->dup;
      if (!parse_prob(optarg, p))
      {
        fprintf(err, "myapp: -%c must be a probability from 0 to 0.5: '%s'\n", c, optarg);
        return APP_EXIT_USAGE;
      }
      break;
    }
    case 'p':
      if (!parse_ulong(optarg, 1, 65535, &ul))
      {
        fprintf(err, "myapp: port must be 1 to 65535: '%s'\n", optarg);
        return APP_EXIT_USAGE;
      }
      opts->port = optarg;
      break;
    case ':':
      fprintf(err, "myapp: option -%c needs an argument\n", optopt);
      app_usage(err);
      return APP_EXIT_USAGE;
    default:
      fprintf(err, "myapp: unknown option -%c for %s\n", optopt, argv[1]);
      app_usage(err);
      return APP_EXIT_USAGE;
    }
  }

  if (opts->session == NULL)
  {
    fprintf(err, "myapp: -s <session> is required\n");
    app_usage(err);
    return APP_EXIT_USAGE;
  }
  if (!app_valid_session(opts->session))
  {
    fprintf(err, "myapp: session must be 1 to 32 characters from a-z, 0-9 and -: '%s'\n",
            opts->session);
    return APP_EXIT_USAGE;
  }
  if (sub_argc - optind != 2)
  {
    fprintf(err, "myapp: expected <relay> <file>\n");
    app_usage(err);
    return APP_EXIT_USAGE;
  }
  opts->relay = sub_argv[optind];
  opts->file = sub_argv[optind + 1];
  return 0;
}

/* Write a probability as a plain decimal, e.g. 0.1, 0.05, 0. */
static void format_prob(double p, char *buf, size_t cap)
{
  snprintf(buf, cap, "%.6f", p);
  char *end = buf + strlen(buf) - 1;
  while (*end == '0')
  {
    *end-- = '\0';
  }
  if (*end == '.')
  {
    *end = '\0';
  }
}

size_t app_format_hello(const app_options *opts, char *buf, size_t cap)
{
  int n;
  if (opts->mode == APP_RECV)
  {
    n = snprintf(buf, cap, "HELLO %s recv", opts->session);
  }
  else
  {
    char l[32], c[32], d[32];
    format_prob(opts->loss, l, sizeof l);
    format_prob(opts->corrupt, c, sizeof c);
    format_prob(opts->dup, d, sizeof d);
    n = snprintf(buf, cap, "HELLO %s send %s %s %s", opts->session, l, c, d);
  }
  if (n < 0 || (size_t)n >= cap)
  {
    return 0;
  }
  return (size_t)n;
}

int app_parse_reply(const uint8_t *buf, size_t len, char *reason, size_t cap)
{
  if (cap > 0)
  {
    reason[0] = '\0';
  }
  /* Accept a trailing newline or whitespace in case the relay sends one. */
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' '))
  {
    len--;
  }
  if (len == 2 && memcmp(buf, "OK", 2) == 0)
  {
    return 0;
  }
  if (len >= 3 && memcmp(buf, "ERR", 3) == 0 && (len == 3 || buf[3] == ' '))
  {
    size_t start = len > 3 ? 4 : 3;
    size_t n = len - start;
    if (cap > 0)
    {
      if (n >= cap)
      {
        n = cap - 1;
      }
      memcpy(reason, buf + start, n);
      reason[n] = '\0';
    }
    return 1;
  }
  return -1;
}

```

### lab.h

```c

#ifndef LAB_H
#define LAB_H

/*
 * Reliable file transfer over UDP with Go-Back-N.
 *
 * The program is split into three layers:
 *
 *   1. Packets: checksum, encode, decode/validate. Pure functions.
 *   2. Go-Back-N state machines: a sender and a receiver. They are fed
 *      events (packet arrived, timer expired) together with the current time
 *      in milliseconds and answer with what to do next. They never touch a
 *      socket, a clock, or a file.
 *   3. I/O: the socket, the relay hello, the poll loop, the clock, and the
 *      file. That layer lives in main.c and is the only code that knows any
 *      of them exist. The pure helpers it needs (argument parsing, building
 *      the hello, reading the relay's reply) are declared at the end of this
 *      header so they can be unit tested.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------------ */
/* Layer 1: packets                                                         */
/* ------------------------------------------------------------------------ */

#define RDT_HEADER_LEN 10u
#define RDT_MAX_PAYLOAD 1024u
#define RDT_MAX_PACKET (RDT_HEADER_LEN + RDT_MAX_PAYLOAD)

typedef enum
{
  RDT_DATA = 0,
  RDT_ACK = 1,
  RDT_FIN = 2
} rdt_type;

/** A decoded packet. The payload is copied in so the struct owns it. */
typedef struct
{
  rdt_type type;
  uint32_t seq;
  uint16_t length;
  uint8_t payload[RDT_MAX_PAYLOAD];
} rdt_packet;

/**
 * @brief RFC 1071 Internet checksum.
 *
 * Returns the one's complement of the one's complement sum of @p buf taken as
 * big-endian 16-bit words. An odd length is padded with one zero byte for the
 * calculation only. Summing a packet that already holds a correct checksum
 * yields 0xffff, so this function returns 0 for such a packet.
 *
 * @param buf bytes to sum (may be NULL when len is 0)
 * @param len number of bytes
 * @return the checksum in host byte order
 */
uint16_t rdt_checksum(const uint8_t *buf, size_t len);

/**
 * @brief Encode a packet into wire format, filling in the checksum.
 *
 * @param pkt the packet; pkt->length bytes of pkt->payload are written
 * @param buf output buffer
 * @param cap capacity of @p buf
 * @return the number of bytes written, or 0 if the packet is invalid (bad
 *         type, length over RDT_MAX_PAYLOAD) or does not fit in @p cap
 */
size_t rdt_encode(const rdt_packet *pkt, uint8_t *buf, size_t cap);

/**
 * @brief Validate a received datagram and decode it.
 *
 * The datagram is rejected unless it is at least RDT_HEADER_LEN bytes, its
 * length field matches its size and is at most RDT_MAX_PAYLOAD, its type is
 * DATA, ACK or FIN, its reserved byte is 0, and its checksum verifies.
 *
 * @param buf the datagram
 * @param len its size, as returned by recvfrom
 * @param out filled in only when the datagram is valid
 * @return true if the datagram is a valid packet
 */
bool rdt_decode(const uint8_t *buf, size_t len, rdt_packet *out);

/**
 * @brief Build an ACK packet.
 * @param pkt output
 * @param seq index of the next packet the receiver expects
 */
void rdt_make_ack(rdt_packet *pkt, uint32_t seq);

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N state machines                                        */
/* ------------------------------------------------------------------------ */

#define GBN_MAX_WINDOW 64u
#define GBN_MAX_TIMEOUTS 10
#define GBN_LINGER_MS 2000u
#define GBN_IDLE_MS 30000u

typedef enum
{
  GBN_RUNNING = 0, /* keep going */
  GBN_DONE,        /* transfer finished successfully */
  GBN_FAILED       /* gave up */
} gbn_status;

/** What the sender wants done after an event. */
typedef struct
{
  /** Send packets send_from .. send_to - 1, in order (empty if equal). */
  uint32_t send_from;
  uint32_t send_to;
  /** Whether the timer is running, and when it expires. */
  bool timer_running;
  uint64_t deadline_ms;
  gbn_status status;
} gbn_sender_action;

/**
 * Go-Back-N sender. The whole file is held in memory (files are at most
 * 16 MiB), so the copy of every unacknowledged packet is simply the file
 * itself: any packet can be rebuilt with gbn_sender_packet.
 */
typedef struct
{
  const uint8_t *data;
  size_t size;
  uint32_t data_packets; /* number of DATA packets; the FIN's seq */
  uint32_t window;
  uint64_t timeout_ms;
  uint32_t base; /* oldest unacknowledged packet */
  uint32_t next; /* next packet never sent */
  bool timer_running;
  uint64_t deadline_ms;
  int timeouts; /* consecutive timeouts without progress */
  gbn_status status;
} gbn_sender;

/**
 * @brief Initialise a sender for a file held in memory.
 * @param s the sender
 * @param data file contents (may be NULL when size is 0)
 * @param size file size in bytes
 * @param window window size in packets, 1 to GBN_MAX_WINDOW
 * @param timeout_ms retransmission timeout
 * @return true on success, false on a bad argument
 */
bool gbn_sender_init(gbn_sender *s, const uint8_t *data, size_t size,
                     uint32_t window, uint64_t timeout_ms);

/**
 * @brief Start the transfer: fill the window.
 * @param s the sender
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_sender_start(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act);

/**
 * @brief Feed the sender an ACK.
 *
 * An ACK with seq > base slides the window to seq; anything else is a
 * duplicate and is ignored. An ACK for a packet never sent is ignored too.
 *
 * @param s the sender
 * @param ack_seq the ACK's seq field
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_sender_on_ack(gbn_sender *s, uint32_t ack_seq, uint64_t now_ms,
                       gbn_sender_action *act);

/**
 * @brief Tell the sender time has passed.
 *
 * If the timer is running and has expired, this is a timeout: everything
 * from base to next - 1 is resent and the timer restarts, unless this is the
 * GBN_MAX_TIMEOUTS-th timeout in a row, in which case the sender gives up.
 * Otherwise nothing happens.
 *
 * @param s the sender
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_sender_on_tick(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act);

/**
 * @brief Build packet @p seq: DATA for seq < data_packets, FIN at
 *        data_packets.
 * @param s the sender
 * @param seq packet index
 * @param pkt output
 * @return false if seq is past the FIN
 */
bool gbn_sender_packet(const gbn_sender *s, uint32_t seq, rdt_packet *pkt);

/** What the receiver wants done after an event. */
typedef struct
{
  /** Send an ACK carrying ack_seq. */
  bool send_ack;
  uint32_t ack_seq;
  /** Append these bytes to the file (deliver_len may be 0). Points into the
   *  packet passed in, so it is valid as long as that packet is. */
  const uint8_t *deliver;
  size_t deliver_len;
  /** The FIN arrived just now: the file is complete and can be closed. */
  bool fin;
} gbn_receiver_action;

/** Go-Back-N receiver. */
typedef struct
{
  uint32_t expected;        /* next packet wanted */
  bool finished;            /* FIN received */
  uint64_t last_activity_ms;/* last valid packet, or start */
  uint64_t linger_until_ms; /* when finished: when to exit */
} gbn_receiver;

/**
 * @brief Initialise a receiver.
 * @param r the receiver
 * @param now_ms current time, which starts the idle timer
 */
void gbn_receiver_init(gbn_receiver *r, uint64_t now_ms);

/**
 * @brief Feed the receiver a valid packet.
 *
 * In-order DATA is delivered and acknowledged; any other DATA is discarded
 * and the current ACK repeated. The FIN at the expected seq completes the
 * file and starts the linger; a repeated FIN is answered with the same ACK.
 * ACK packets are ignored.
 *
 * @param r the receiver
 * @param pkt a packet that passed rdt_decode
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_receiver_on_packet(gbn_receiver *r, const rdt_packet *pkt,
                            uint64_t now_ms, gbn_receiver_action *act);

/**
 * @brief When the receiver next needs to be woken up.
 * @param r the receiver
 * @return the end of the linger if finished, else the idle deadline
 */
uint64_t gbn_receiver_deadline(const gbn_receiver *r);

/**
 * @brief Tell the receiver time has passed.
 * @param r the receiver
 * @param now_ms current time
 * @return GBN_DONE once the linger is over, GBN_FAILED after GBN_IDLE_MS
 *         with nothing valid arriving, else GBN_RUNNING
 */
gbn_status gbn_receiver_on_tick(const gbn_receiver *r, uint64_t now_ms);

/* ------------------------------------------------------------------------ */
/* Layer 3 helpers: pure pieces of the I/O layer                            */
/* ------------------------------------------------------------------------ */

#define APP_EXIT_OK 0
#define APP_EXIT_USAGE 1
#define APP_EXIT_FAIL 2

#define APP_SESSION_MAX 32u
#define APP_HELLO_MAX 128u

typedef enum
{
  APP_SEND,
  APP_RECV
} app_mode;

typedef struct
{
  app_mode mode;
  const char *session;
  uint32_t window;
  uint32_t timeout_ms;
  double loss;
  double corrupt;
  double dup;
  const char *port; /* decimal string, for getaddrinfo */
  const char *relay;
  const char *file;
} app_options;

/**
 * @brief Print the usage message.
 * @param out where to print it
 */
void app_usage(FILE *out);

/**
 * @brief Parse the command line. argv[1] is the mode, the rest is parsed
 *        with getopt. Error messages go to @p err.
 * @param argc argument count
 * @param argv argument vector (getopt may permute it)
 * @param opts output, filled with defaults first
 * @param err where to report problems
 * @return 0 if the options are valid, APP_EXIT_USAGE if not
 */
int app_parse_args(int argc, char *argv[], app_options *opts, FILE *err);

/**
 * @brief Check a session name: 1 to 32 characters from a-z, 0-9 and '-'.
 * @param session the name
 * @return true if valid
 */
bool app_valid_session(const char *session);

/**
 * @brief Build the hello datagram (no newline, no terminator on the wire).
 * @param opts parsed options
 * @param buf output; NUL terminated
 * @param cap capacity of @p buf
 * @return the number of bytes to send, or 0 if it does not fit
 */
size_t app_format_hello(const app_options *opts, char *buf, size_t cap);

/**
 * @brief Interpret the relay's reply to a hello.
 * @param buf the datagram (not NUL terminated)
 * @param len its size
 * @param reason output for the text after "ERR " (NUL terminated)
 * @param cap capacity of @p reason
 * @return 0 for OK, 1 for ERR, -1 for anything else
 */
int app_parse_reply(const uint8_t *buf, size_t len, char *reason, size_t cap);

#endif // LAB_H

```

### main.c

```c

/*
 * Layer 3: the I/O. The socket, the relay hello, the poll loop, the clock and
 * the file live here and nowhere else. Each loop reads one event, hands it to
 * the state machine in lab.c, and carries out what comes back.
 */
#define _POSIX_C_SOURCE 200809L

#include "lab.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

#define HELLO_ATTEMPTS 5
#define HELLO_WAIT_MS 1000

typedef struct
{
  int fd;
  struct sockaddr_storage relay;
  socklen_t relay_len;
} relay_conn;

static uint64_t now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static int ms_until(uint64_t deadline)
{
  uint64_t now = now_ms();
  if (deadline <= now)
  {
    return 0;
  }
  uint64_t left = deadline - now;
  return left > 60000 ? 60000 : (int)left;
}

static bool same_addr(const struct sockaddr_storage *a, socklen_t alen,
                      const struct sockaddr_storage *b, socklen_t blen)
{
  return alen == blen && memcmp(a, b, alen) == 0;
}

/* Receive one datagram from the relay. Returns its size, or -1 when the
 * datagram should be ignored (error, or from someone else). */
static ssize_t relay_recv(relay_conn *rc, uint8_t *buf, size_t cap)
{
  struct sockaddr_storage from;
  socklen_t from_len = sizeof from;
  memset(&from, 0, sizeof from);
  ssize_t n = recvfrom(rc->fd, buf, cap, 0, (struct sockaddr *)&from, &from_len);
  if (n < 0 || !same_addr(&from, from_len, &rc->relay, rc->relay_len))
  {
    return -1;
  }
  return n;
}

static void relay_send(relay_conn *rc, const rdt_packet *pkt)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = rdt_encode(pkt, buf, sizeof buf);
  if (n > 0)
  {
    /* A failed send is just a lost packet; the timer recovers it. */
    (void)sendto(rc->fd, buf, n, 0, (struct sockaddr *)&rc->relay, rc->relay_len);
  }
}

/* Resolve the relay and open the one socket used for the whole run. */
static int relay_open(relay_conn *rc, const char *host, const char *port)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo *res;
  int rv = getaddrinfo(host, port, &hints, &res);
  if (rv != 0)
  {
    fprintf(stderr, "myapp: cannot resolve %s: %s\n", host, gai_strerror(rv));
    return -1;
  }
  rc->fd = -1;
  for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next)
  {
    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0)
    {
      continue;
    }
    rc->fd = fd;
    memcpy(&rc->relay, ai->ai_addr, ai->ai_addrlen);
    rc->relay_len = ai->ai_addrlen;
    break;
  }
  freeaddrinfo(res);
  if (rc->fd < 0)
  {
    perror("myapp: socket");
    return -1;
  }
  return 0;
}

/* Say hello and wait for OK, retrying once a second up to five times. */
static int relay_hello(relay_conn *rc, const app_options *opts)
{
  char hello[APP_HELLO_MAX];
  size_t len = app_format_hello(opts, hello, sizeof hello);
  for (int attempt = 0; attempt < HELLO_ATTEMPTS; attempt++)
  {
    if (sendto(rc->fd, hello, len, 0, (struct sockaddr *)&rc->relay, rc->relay_len) < 0)
    {
      perror("myapp: sendto");
    }
    uint64_t deadline = now_ms() + HELLO_WAIT_MS;
    struct pollfd pfd = {.fd = rc->fd, .events = POLLIN};
    int wait;
    while ((wait = ms_until(deadline)) > 0)
    {
      if (poll(&pfd, 1, wait) <= 0)
      {
        continue;
      }
      uint8_t buf[256];
      ssize_t n = relay_recv(rc, buf, sizeof buf);
      if (n < 0)
      {
        continue;
      }
      char reason[200];
      int r = app_parse_reply(buf, (size_t)n, reason, sizeof reason);
      if (r == 0)
      {
        return 0;
      }
      if (r == 1)
      {
        fprintf(stderr, "myapp: relay refused: %s\n", reason);
        return -1;
      }
    }
  }
  fprintf(stderr, "myapp: no reply from relay %s port %s after %d attempts\n",
          opts->relay, opts->port, HELLO_ATTEMPTS);
  return -1;
}

static uint8_t *read_file(const char *path, size_t *size)
{
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    perror(path);
    return NULL;
  }
  size_t cap = 65536, len = 0;
  uint8_t *data = malloc(cap);
  while (data != NULL)
  {
    size_t n = fread(data + len, 1, cap - len, f);
    len += n;
    if (n == 0)
    {
      break;
    }
    if (len == cap)
    {
      cap *= 2;
      uint8_t *bigger = realloc(data, cap);
      if (bigger == NULL)
      {
        free(data);
      }
      data = bigger;
    }
  }
  if (data == NULL || ferror(f))
  {
    fprintf(stderr, "myapp: cannot read %s\n", path);
    free(data);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *size = len;
  return data;
}

static void send_range(relay_conn *rc, const gbn_sender *s, const gbn_sender_action *act)
{
  rdt_packet pkt;
  for (uint32_t seq = act->send_from; seq < act->send_to; seq++)
  {
    if (gbn_sender_packet(s, seq, &pkt))
    {
      relay_send(rc, &pkt);
    }
  }
}

static int run_sender(relay_conn *rc, const app_options *opts)
{
  size_t size = 0;
  uint8_t *data = read_file(opts->file, &size);
  if (data == NULL)
  {
    return APP_EXIT_FAIL;
  }
  gbn_sender s;
  if (!gbn_sender_init(&s, data, size, opts->window, opts->timeout_ms))
  {
    fprintf(stderr, "myapp: cannot send %s\n", opts->file);
    free(data);
    return APP_EXIT_FAIL;
  }
  if (relay_hello(rc, opts) != 0)
  {
    free(data);
    return APP_EXIT_FAIL;
  }

  gbn_sender_action act;
  gbn_sender_start(&s, now_ms(), &act);
  send_range(rc, &s, &act);

  struct pollfd pfd = {.fd = rc->fd, .events = POLLIN};
  while (act.status == GBN_RUNNING)
  {
    int wait = act.timer_running ? ms_until(act.deadline_ms) : 1000;
    if (poll(&pfd, 1, wait) > 0)
    {
      uint8_t buf[RDT_MAX_PACKET + 1];
      rdt_packet pkt;
      ssize_t n = relay_recv(rc, buf, sizeof buf);
      if (n >= 0 && rdt_decode(buf, (size_t)n, &pkt) && pkt.type == RDT_ACK)
      {
        gbn_sender_on_ack(&s, pkt.seq, now_ms(), &act);
        send_range(rc, &s, &act);
      }
    }
    if (act.status == GBN_RUNNING)
    {
      gbn_sender_on_tick(&s, now_ms(), &act);
      send_range(rc, &s, &act);
    }
  }

  free(data);
  if (act.status == GBN_FAILED)
  {
    fprintf(stderr, "myapp: giving up after %d timeouts in a row with no progress\n",
            GBN_MAX_TIMEOUTS);
    return APP_EXIT_FAIL;
  }
  return APP_EXIT_OK;
}

static int run_receiver(relay_conn *rc, const app_options *opts)
{
  FILE *out = fopen(opts->file, "wb");
  if (out == NULL)
  {
    perror(opts->file);
    return APP_EXIT_FAIL;
  }
  if (relay_hello(rc, opts) != 0)
  {
    fclose(out);
    return APP_EXIT_FAIL;
  }

  gbn_receiver r;
  gbn_receiver_init(&r, now_ms());
  struct pollfd pfd = {.fd = rc->fd, .events = POLLIN};
  gbn_status status = GBN_RUNNING;
  int rv = APP_EXIT_OK;
  while (status == GBN_RUNNING)
  {
    if (poll(&pfd, 1, ms_until(gbn_receiver_deadline(&r))) > 0)
    {
      uint8_t buf[RDT_MAX_PACKET + 1];
      rdt_packet pkt;
      ssize_t n = relay_recv(rc, buf, sizeof buf);
      if (n >= 0 && rdt_decode(buf, (size_t)n, &pkt))
      {
        gbn_receiver_action act;
        gbn_receiver_on_packet(&r, &pkt, now_ms(), &act);
        if (act.deliver_len > 0 && fwrite(act.deliver, 1, act.deliver_len, out) != act.deliver_len)
        {
          perror(opts->file);
          rv = APP_EXIT_FAIL;
          break;
        }
        if (act.fin)
        {
          int closed = fclose(out);
          out = NULL;
          if (closed != 0)
          {
            perror(opts->file);
            rv = APP_EXIT_FAIL;
            break;
          }
        }
        if (act.send_ack)
        {
          rdt_packet ack;
          rdt_make_ack(&ack, act.ack_seq);
          relay_send(rc, &ack);
        }
      }
    }
    status = gbn_receiver_on_tick(&r, now_ms());
  }

  if (out != NULL)
  {
    fclose(out);
  }
  if (status == GBN_FAILED)
  {
    fprintf(stderr, "myapp: nothing arrived for %u seconds, giving up\n", GBN_IDLE_MS / 1000);
    rv = APP_EXIT_FAIL;
  }
  return rv;
}

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    app_usage(stdout);
    return APP_EXIT_OK;
  }
  app_options opts;
  if (app_parse_args(argc, argv, &opts, stderr) != 0)
  {
    return APP_EXIT_USAGE;
  }

  relay_conn rc;
  if (relay_open(&rc, opts.relay, opts.port) != 0)
  {
    return APP_EXIT_FAIL;
  }
  int rv = opts.mode == APP_SEND ? run_sender(&rc, &opts) : run_receiver(&rc, &opts);
  close(rc.fd);
  return rv;
}

```

## Tests Files
### lab-test.c

```c

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "harness/unity.h"
#include "../src/lab.h"

void setUp(void) {}

void tearDown(void) {}

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */

static size_t encode_data(uint32_t seq, const char *payload, uint8_t *buf)
{
  rdt_packet p;
  p.type = RDT_DATA;
  p.seq = seq;
  p.length = (uint16_t)strlen(payload);
  memcpy(p.payload, payload, p.length);
  return rdt_encode(&p, buf, RDT_MAX_PACKET);
}

static rdt_packet make_pkt(rdt_type type, uint32_t seq, const char *payload)
{
  rdt_packet p;
  memset(&p, 0, sizeof p);
  p.type = type;
  p.seq = seq;
  p.length = (uint16_t)strlen(payload);
  memcpy(p.payload, payload, p.length);
  return p;
}

static void fill_pattern(uint8_t *buf, size_t n, uint32_t seed)
{
  for (size_t i = 0; i < n; i++)
  {
    seed = seed * 1103515245u + 12345u;
    buf[i] = (uint8_t)(seed >> 16);
  }
}

/* ------------------------------------------------------------------------ */
/* Layer 1: checksum                                                        */
/* ------------------------------------------------------------------------ */

void test_checksum_rfc1071_example(void)
{
  const uint8_t bytes[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
  /* the sum is 0xddf2, so the checksum is its complement */
  TEST_ASSERT_EQUAL_HEX16(0x220d, rdt_checksum(bytes, sizeof bytes));
}

void test_checksum_odd_length_pads_with_zero(void)
{
  const uint8_t odd[] = {0x12, 0x34, 0x56};
  const uint8_t padded[] = {0x12, 0x34, 0x56, 0x00};
  /* 0x1234 + 0x5600 = 0x6834 */
  TEST_ASSERT_EQUAL_HEX16((uint16_t)~0x6834u, rdt_checksum(odd, sizeof odd));
  TEST_ASSERT_EQUAL_HEX16(rdt_checksum(padded, sizeof padded), rdt_checksum(odd, sizeof odd));
}

void test_checksum_folds_carry(void)
{
  const uint8_t bytes[] = {0xff, 0xff, 0x00, 0x02};
  /* 0xffff + 0x0002 = 0x10001 -> fold -> 0x0002 */
  TEST_ASSERT_EQUAL_HEX16((uint16_t)~0x0002u, rdt_checksum(bytes, sizeof bytes));
}

void test_checksum_empty(void)
{
  TEST_ASSERT_EQUAL_HEX16(0xffff, rdt_checksum(NULL, 0));
}

void test_checksum_catches_every_single_bit_flip(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(2, "Hi!", buf);
  TEST_ASSERT_EQUAL_HEX16(0, rdt_checksum(buf, n));
  for (size_t bit = 0; bit < n * 8; bit++)
  {
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    TEST_ASSERT_NOT_EQUAL(0, rdt_checksum(buf, n));
    rdt_packet p;
    TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
  }
}

/* ------------------------------------------------------------------------ */
/* Layer 1: encode                                                          */
/* ------------------------------------------------------------------------ */

void test_encode_worked_example_hi(void)
{
  const uint8_t expected[] = {0x00, 0x00, 0x96, 0x91, 0x00, 0x00, 0x00,
                              0x02, 0x00, 0x03, 0x48, 0x69, 0x21};
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(2, "Hi!", buf);
  TEST_ASSERT_EQUAL_size_t(sizeof expected, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, n);
}

void test_encode_worked_example_ack3(void)
{
  const uint8_t expected[] = {0x01, 0x00, 0xfe, 0xfc, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
  rdt_packet p;
  rdt_make_ack(&p, 3);
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = rdt_encode(&p, buf, sizeof buf);
  TEST_ASSERT_EQUAL_size_t(10, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, n);
}

void test_make_ack(void)
{
  rdt_packet p;
  memset(&p, 0xaa, sizeof p);
  rdt_make_ack(&p, 77);
  TEST_ASSERT_EQUAL(RDT_ACK, p.type);
  TEST_ASSERT_EQUAL_UINT32(77, p.seq);
  TEST_ASSERT_EQUAL_UINT16(0, p.length);
}

void test_encode_full_payload_and_large_seq(void)
{
  rdt_packet p;
  p.type = RDT_DATA;
  p.seq = 0x01020304;
  p.length = RDT_MAX_PAYLOAD;
  fill_pattern(p.payload, RDT_MAX_PAYLOAD, 9);
  uint8_t buf[RDT_MAX_PACKET];
  TEST_ASSERT_EQUAL_size_t(RDT_MAX_PACKET, rdt_encode(&p, buf, sizeof buf));
  /* network byte order */
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[7]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[8]);
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[9]);
}

void test_encode_rejects_bad_input(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  rdt_packet p = make_pkt(RDT_DATA, 0, "abc");
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(NULL, buf, sizeof buf));
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, NULL, sizeof buf));
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, buf, 12)); /* needs 13 */
  TEST_ASSERT_EQUAL_size_t(13, rdt_encode(&p, buf, 13));
  p.length = RDT_MAX_PAYLOAD + 1;
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, buf, sizeof buf));
  p.length = 0;
  p.type = (rdt_type)3;
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, buf, sizeof buf));
}

/* ------------------------------------------------------------------------ */
/* Layer 1: decode                                                          */
/* ------------------------------------------------------------------------ */

void test_decode_round_trip(void)
{
  rdt_packet in, out;
  in.type = RDT_DATA;
  in.seq = 123456;
  in.length = 700;
  fill_pattern(in.payload, in.length, 3);
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = rdt_encode(&in, buf, sizeof buf);
  TEST_ASSERT_TRUE(rdt_decode(buf, n, &out));
  TEST_ASSERT_EQUAL(RDT_DATA, out.type);
  TEST_ASSERT_EQUAL_UINT32(123456, out.seq);
  TEST_ASSERT_EQUAL_UINT16(700, out.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(in.payload, out.payload, 700);

  rdt_packet fin = make_pkt(RDT_FIN, 9, "");
  n = rdt_encode(&fin, buf, sizeof buf);
  TEST_ASSERT_TRUE(rdt_decode(buf, n, &out));
  TEST_ASSERT_EQUAL(RDT_FIN, out.type);
  TEST_ASSERT_EQUAL_UINT32(9, out.seq);
}

void test_decode_too_short(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(0, "", buf);
  rdt_packet p;
  TEST_ASSERT_EQUAL_size_t(10, n);
  TEST_ASSERT_TRUE(rdt_decode(buf, n, &p));
  for (size_t len = 0; len < RDT_HEADER_LEN; len++)
  {
    TEST_ASSERT_FALSE(rdt_decode(buf, len, &p));
  }
  TEST_ASSERT_FALSE(rdt_decode(NULL, 10, &p));
  TEST_ASSERT_FALSE(rdt_decode(buf, n, NULL));
}

void test_decode_length_mismatch(void)
{
  uint8_t buf[RDT_MAX_PACKET + 4];
  size_t n = encode_data(1, "hello", buf);
  rdt_packet p;
  TEST_ASSERT_FALSE(rdt_decode(buf, n - 1, &p)); /* truncated */
  buf[n] = 0;
  buf[n + 1] = 0;
  TEST_ASSERT_FALSE(rdt_decode(buf, n + 2, &p)); /* trailing bytes */
}

void test_decode_length_field_over_max(void)
{
  /* A length field of 1025 in a datagram that is really 1035 bytes long,
   * with a correct checksum: still rejected. */
  uint8_t buf[RDT_MAX_PACKET + 1];
  memset(buf, 0, sizeof buf);
  buf[8] = 0x04;
  buf[9] = 0x01;
  uint16_t sum = rdt_checksum(buf, sizeof buf);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)sum;
  rdt_packet p;
  TEST_ASSERT_FALSE(rdt_decode(buf, sizeof buf, &p));
}

void test_decode_unknown_type_and_reserved(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  rdt_packet p;

  /* Rewrite the header and fix the checksum so only the field is wrong. */
  size_t n = encode_data(1, "x", buf);
  buf[0] = 3;
  buf[2] = buf[3] = 0;
  uint16_t sum = rdt_checksum(buf, n);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)sum;
  TEST_ASSERT_EQUAL_HEX16(0, rdt_checksum(buf, n));
  TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));

  n = encode_data(1, "x", buf);
  buf[1] = 1;
  buf[2] = buf[3] = 0;
  sum = rdt_checksum(buf, n);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)sum;
  TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));
}

void test_decode_bad_checksum(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(2, "Hi!", buf);
  rdt_packet p;
  buf[12] ^= 0x01;
  TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));
}

/* ------------------------------------------------------------------------ */
/* Layer 2: sender                                                          */
/* ------------------------------------------------------------------------ */

static uint8_t file_buf[64 * 1024];

void test_sender_init_counts_packets(void)
{
  gbn_sender s;
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 2500, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(3, s.data_packets);
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 2048, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(2, s.data_packets);
  TEST_ASSERT_TRUE(gbn_sender_init(&s, NULL, 0, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(0, s.data_packets);
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 1, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(1, s.data_packets);
}

void test_sender_init_rejects_bad_args(void)
{
  gbn_sender s;
  TEST_ASSERT_FALSE(gbn_sender_init(NULL, file_buf, 10, 4, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, NULL, 10, 4, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, file_buf, 10, 0, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, file_buf, 10, 65, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, file_buf, 10, 4, 0));
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 10, 64, 1));
}

void test_sender_packet_cuts_file(void)
{
  /* Worked example 1: 2500 bytes -> 1024, 1024, 452, FIN 3 */
  fill_pattern(file_buf, 2500, 1);
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 2500, 4, 250);
  rdt_packet p;
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 0, &p));
  TEST_ASSERT_EQUAL(RDT_DATA, p.type);
  TEST_ASSERT_EQUAL_UINT16(1024, p.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(file_buf, p.payload, 1024);
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 1, &p));
  TEST_ASSERT_EQUAL_UINT16(1024, p.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(file_buf + 1024, p.payload, 1024);
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 2, &p));
  TEST_ASSERT_EQUAL_UINT32(2, p.seq);
  TEST_ASSERT_EQUAL_UINT16(452, p.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(file_buf + 2048, p.payload, 452);
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 3, &p));
  TEST_ASSERT_EQUAL(RDT_FIN, p.type);
  TEST_ASSERT_EQUAL_UINT32(3, p.seq);
  TEST_ASSERT_EQUAL_UINT16(0, p.length);
  TEST_ASSERT_FALSE(gbn_sender_packet(&s, 4, &p));
}

void test_sender_exact_multiple_of_1024(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 3072, 8, 250);
  rdt_packet p;
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 2, &p));
  TEST_ASSERT_EQUAL(RDT_DATA, p.type);
  TEST_ASSERT_EQUAL_UINT16(1024, p.length); /* no empty DATA at the end */
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 3, &p));
  TEST_ASSERT_EQUAL(RDT_FIN, p.type);

  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(3, a.send_to); /* DATA only; FIN waits */
  gbn_sender_on_ack(&s, 3, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(3, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_to); /* now the FIN */
  gbn_sender_on_ack(&s, 4, 20, &a);
  TEST_ASSERT_EQUAL(GBN_DONE, a.status);
}

void test_sender_empty_file(void)
{
  gbn_sender s;
  gbn_sender_init(&s, NULL, 0, 8, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_to);
  rdt_packet p;
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 0, &p));
  TEST_ASSERT_EQUAL(RDT_FIN, p.type);
  TEST_ASSERT_EQUAL_UINT32(0, p.seq);
  TEST_ASSERT_TRUE(a.timer_running);
  /* the FIN is retransmitted like any other packet */
  gbn_sender_on_tick(&s, 250, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_to);
  gbn_sender_on_ack(&s, 1, 300, &a);
  TEST_ASSERT_EQUAL(GBN_DONE, a.status);
  TEST_ASSERT_FALSE(a.timer_running);
}

void test_sender_window_full(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 10 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 1000, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_to);
  TEST_ASSERT_TRUE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT64(1250, a.deadline_ms);
  TEST_ASSERT_EQUAL_UINT32(0, s.base);
  TEST_ASSERT_EQUAL_UINT32(4, s.next);

  /* nothing more can go until an ACK moves base */
  gbn_sender_on_tick(&s, 1100, &a);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
  TEST_ASSERT_EQUAL(GBN_RUNNING, a.status);

  /* an ACK for one packet opens room for exactly one more */
  gbn_sender_on_ack(&s, 1, 1100, &a);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(5, a.send_to);
  TEST_ASSERT_EQUAL_UINT64(1350, a.deadline_ms); /* restarted */
}

void test_sender_stop_and_wait(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 3000, 1, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_to);
  gbn_sender_on_ack(&s, 1, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(2, a.send_to);
  gbn_sender_on_tick(&s, 110, &a); /* timeout resends only packet 1 */
  TEST_ASSERT_EQUAL_UINT32(1, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(2, a.send_to);
}

void test_sender_cumulative_ack_slides_several(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 20 * 1024, 6, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_to);
  /* Worked example 3: ACKs 1..4 were lost, ACK 5 covers them all. */
  gbn_sender_on_ack(&s, 5, 50, &a);
  TEST_ASSERT_EQUAL_UINT32(5, s.base);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(11, a.send_to);
  TEST_ASSERT_TRUE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT64(300, a.deadline_ms);
}

void test_sender_duplicate_ack_ignored(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 6 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 2, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(2, s.base);
  TEST_ASSERT_EQUAL_UINT64(260, a.deadline_ms);
  uint32_t next = s.next;
  for (int i = 0; i < 3; i++)
  {
    gbn_sender_on_ack(&s, 2, 100, &a);
    TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
    TEST_ASSERT_EQUAL_UINT64(260, a.deadline_ms); /* timer not restarted */
  }
  gbn_sender_on_ack(&s, 1, 100, &a); /* older still */
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
  TEST_ASSERT_EQUAL_UINT32(2, s.base);
  TEST_ASSERT_EQUAL_UINT32(next, s.next);
}

void test_sender_ack_beyond_next_ignored(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 10 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 9, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(0, s.base);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
}

void test_sender_timeout_resends_whole_window(void)
{
  /* The diagram: window 4, six packets, DATA 2 lost. */
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 6 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 1, 100, &a);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(5, a.send_to);
  gbn_sender_on_ack(&s, 2, 110, &a);
  TEST_ASSERT_EQUAL_UINT32(5, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_to);
  gbn_sender_on_ack(&s, 2, 200, &a);
  gbn_sender_on_ack(&s, 2, 210, &a);
  gbn_sender_on_ack(&s, 2, 220, &a);
  gbn_sender_on_tick(&s, 359, &a); /* not yet */
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
  gbn_sender_on_tick(&s, 360, &a);
  TEST_ASSERT_EQUAL_UINT32(2, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_to);
  TEST_ASSERT_EQUAL_UINT64(610, a.deadline_ms);
  gbn_sender_on_ack(&s, 6, 450, &a);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(7, a.send_to); /* the FIN */
}

void test_sender_gives_up_after_ten_timeouts(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 4 * 1024, 2, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  uint64_t now = 0;
  for (int i = 1; i < GBN_MAX_TIMEOUTS; i++)
  {
    now += 100;
    gbn_sender_on_tick(&s, now, &a);
    TEST_ASSERT_EQUAL(GBN_RUNNING, a.status);
    TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
    TEST_ASSERT_EQUAL_UINT32(2, a.send_to);
  }
  now += 100;
  gbn_sender_on_tick(&s, now, &a);
  TEST_ASSERT_EQUAL(GBN_FAILED, a.status);
  TEST_ASSERT_FALSE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);

  /* a failed sender stays failed */
  gbn_sender_on_ack(&s, 2, now, &a);
  TEST_ASSERT_EQUAL(GBN_FAILED, a.status);
}

void test_sender_progress_resets_timeout_count(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 8 * 1024, 2, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  uint64_t now = 0;
  for (int i = 1; i < GBN_MAX_TIMEOUTS; i++)
  {
    now += 100;
    gbn_sender_on_tick(&s, now, &a);
  }
  gbn_sender_on_ack(&s, 1, now, &a); /* progress */
  TEST_ASSERT_EQUAL_INT(0, s.timeouts);
  for (int i = 1; i < GBN_MAX_TIMEOUTS; i++)
  {
    now += 100;
    gbn_sender_on_tick(&s, now, &a);
    TEST_ASSERT_EQUAL(GBN_RUNNING, a.status);
  }
}

void test_sender_timer_stops_when_all_acked(void)
{
  /* window 2 over 2 DATA: when both are acked the FIN goes out, and the
   * timer is restarted for it rather than left stopped */
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 2048, 2, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 2, 50, &a);
  TEST_ASSERT_TRUE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT64(150, a.deadline_ms);
  /* tick with no timer due does nothing */
  gbn_sender_on_tick(&s, 60, &a);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
}

/* ------------------------------------------------------------------------ */
/* Layer 2: receiver                                                        */
/* ------------------------------------------------------------------------ */

void test_receiver_in_order(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p = make_pkt(RDT_DATA, 0, "abc");
  gbn_receiver_on_packet(&r, &p, 10, &a);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(3, a.deliver_len);
  TEST_ASSERT_EQUAL_MEMORY("abc", a.deliver, 3);
  TEST_ASSERT_FALSE(a.fin);
}

void test_receiver_duplicate(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p = make_pkt(RDT_DATA, 0, "abc");
  gbn_receiver_on_packet(&r, &p, 10, &a);
  gbn_receiver_on_packet(&r, &p, 20, &a);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  TEST_ASSERT_EQUAL_UINT32(1, r.expected);
}

void test_receiver_beyond_gap(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p0 = make_pkt(RDT_DATA, 0, "a");
  rdt_packet p2 = make_pkt(RDT_DATA, 2, "c");
  rdt_packet fin = make_pkt(RDT_FIN, 3, "");
  gbn_receiver_on_packet(&r, &p0, 10, &a);
  gbn_receiver_on_packet(&r, &p2, 20, &a);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  /* a FIN beyond the gap does not end anything */
  gbn_receiver_on_packet(&r, &fin, 30, &a);
  TEST_ASSERT_FALSE(a.fin);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_FALSE(r.finished);
}

void test_receiver_fin_and_repeated_fin(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p0 = make_pkt(RDT_DATA, 0, "a");
  rdt_packet fin = make_pkt(RDT_FIN, 1, "");
  gbn_receiver_on_packet(&r, &p0, 10, &a);
  gbn_receiver_on_packet(&r, &fin, 100, &a);
  TEST_ASSERT_TRUE(a.fin);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(2, a.ack_seq);
  TEST_ASSERT_EQUAL_UINT64(100 + GBN_LINGER_MS, gbn_receiver_deadline(&r));

  /* Worked example 5: ACK 2 was lost and the FIN comes again. */
  gbn_receiver_on_packet(&r, &fin, 400, &a);
  TEST_ASSERT_FALSE(a.fin); /* the file is not closed twice */
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(2, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  /* the linger is not extended */
  TEST_ASSERT_EQUAL_UINT64(100 + GBN_LINGER_MS, gbn_receiver_deadline(&r));

  /* a stray DATA after the FIN is just acked */
  gbn_receiver_on_packet(&r, &p0, 500, &a);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  TEST_ASSERT_EQUAL_UINT32(2, a.ack_seq);
}

void test_receiver_empty_file(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet fin = make_pkt(RDT_FIN, 0, "");
  gbn_receiver_on_packet(&r, &fin, 5, &a);
  TEST_ASSERT_TRUE(a.fin);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
}

void test_receiver_ignores_ack(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet ack;
  rdt_make_ack(&ack, 0);
  gbn_receiver_on_packet(&r, &ack, 5, &a);
  TEST_ASSERT_FALSE(a.send_ack);
  TEST_ASSERT_FALSE(a.fin);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
}

void test_receiver_linger_then_done(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet fin = make_pkt(RDT_FIN, 0, "");
  gbn_receiver_on_packet(&r, &fin, 1000, &a);
  TEST_ASSERT_EQUAL(GBN_RUNNING, gbn_receiver_on_tick(&r, 1000 + GBN_LINGER_MS - 1));
  TEST_ASSERT_EQUAL(GBN_DONE, gbn_receiver_on_tick(&r, 1000 + GBN_LINGER_MS));
}

void test_receiver_idle_gives_up(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 500);
  TEST_ASSERT_EQUAL_UINT64(500 + GBN_IDLE_MS, gbn_receiver_deadline(&r));
  TEST_ASSERT_EQUAL(GBN_RUNNING, gbn_receiver_on_tick(&r, 500 + GBN_IDLE_MS - 1));
  TEST_ASSERT_EQUAL(GBN_FAILED, gbn_receiver_on_tick(&r, 500 + GBN_IDLE_MS));

  /* a valid packet pushes the deadline back */
  gbn_receiver_action a;
  rdt_packet p = make_pkt(RDT_DATA, 5, "z");
  gbn_receiver_on_packet(&r, &p, 20000, &a);
  TEST_ASSERT_EQUAL_UINT64(20000 + GBN_IDLE_MS, gbn_receiver_deadline(&r));
  TEST_ASSERT_EQUAL(GBN_RUNNING, gbn_receiver_on_tick(&r, 30500));
}

/* ------------------------------------------------------------------------ */
/* Layer 2: a whole transfer through a damaging in-memory channel           */
/* ------------------------------------------------------------------------ */

#define SIM_QUEUE 4096
#define SIM_DELAY_MS 20

typedef struct
{
  uint64_t due;
  size_t len;
  uint8_t bytes[RDT_MAX_PACKET];
} sim_dgram;

/* One direction of the channel. The delay is constant, so it is a FIFO. */
typedef struct
{
  sim_dgram q[SIM_QUEUE];
  size_t head, count;
  double loss, corrupt, dup;
  uint32_t rng;
  unsigned dropped, corrupted, duplicated;
} sim_chan;

static uint32_t sim_rand(uint32_t *state)
{
  /* xorshift32 */
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static double sim_uniform(uint32_t *state)
{
  return (double)sim_rand(state) / 4294967296.0;
}

static void sim_enqueue(sim_chan *c, const uint8_t *bytes, size_t len, uint64_t due)
{
  TEST_ASSERT_TRUE_MESSAGE(c->count < SIM_QUEUE, "simulated channel overflowed");
  sim_dgram *d = &c->q[(c->head + c->count) % SIM_QUEUE];
  d->due = due;
  d->len = len;
  memcpy(d->bytes, bytes, len);
  c->count++;
}

/* Hand a packet to the channel, damaging it the way the relay does. */
static void sim_send(sim_chan *c, const rdt_packet *pkt, uint64_t now)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t len = rdt_encode(pkt, buf, sizeof buf);
  TEST_ASSERT_NOT_EQUAL(0, len);
  if (sim_uniform(&c->rng) < c->loss)
  {
    c->dropped++;
    return;
  }
  if (sim_uniform(&c->rng) < c->corrupt)
  {
    uint32_t bit = sim_rand(&c->rng) % (uint32_t)(len * 8);
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    c->corrupted++;
    sim_enqueue(c, buf, len, now + SIM_DELAY_MS);
    return;
  }
  if (sim_uniform(&c->rng) < c->dup)
  {
    c->duplicated++;
    sim_enqueue(c, buf, len, now + SIM_DELAY_MS);
  }
  sim_enqueue(c, buf, len, now + SIM_DELAY_MS);
}

static bool sim_pop(sim_chan *c, uint64_t now, sim_dgram *out)
{
  if (c->count == 0 || c->q[c->head].due > now)
  {
    return false;
  }
  *out = c->q[c->head];
  c->head = (c->head + 1) % SIM_QUEUE;
  c->count--;
  return true;
}

static void sim_send_range(sim_chan *c, const gbn_sender *s,
                           const gbn_sender_action *a, uint64_t now)
{
  rdt_packet p;
  for (uint32_t seq = a->send_from; seq < a->send_to; seq++)
  {
    TEST_ASSERT_TRUE(gbn_sender_packet(s, seq, &p));
    sim_send(c, &p, now);
  }
}

static sim_chan to_recv, to_send;
static uint8_t sim_in[200 * 1024];
static uint8_t sim_out[200 * 1024];

/* Run one transfer. Returns the sender's final status; the receiver's output
 * is in sim_out, *out_len bytes long. */
static gbn_status sim_transfer(size_t size, uint32_t window, double rate,
                               uint32_t seed, size_t *out_len, bool *closed)
{
  memset(&to_recv, 0, sizeof to_recv);
  memset(&to_send, 0, sizeof to_send);
  to_recv.loss = to_recv.corrupt = to_recv.dup = rate;
  to_send.loss = to_send.corrupt = to_send.dup = rate;
  to_recv.rng = seed * 2654435761u + 1;
  to_send.rng = seed * 40503u + 7;
  fill_pattern(sim_in, size, seed);

  gbn_sender s;
  gbn_receiver r;
  gbn_sender_action sa;
  TEST_ASSERT_TRUE(gbn_sender_init(&s, sim_in, size, window, 100));
  gbn_receiver_init(&r, 0);

  uint64_t now = 0;
  *out_len = 0;
  *closed = false;
  gbn_sender_start(&s, now, &sa);
  sim_send_range(&to_recv, &s, &sa, now);

  for (int steps = 0; sa.status == GBN_RUNNING && steps < 1000000; steps++)
  {
    /* jump straight to the next event */
    uint64_t next = UINT64_MAX;
    if (to_recv.count > 0)
      next = to_recv.q[to_recv.head].due;
    if (to_send.count > 0 && to_send.q[to_send.head].due < next)
      next = to_send.q[to_send.head].due;
    if (sa.timer_running && sa.deadline_ms < next)
      next = sa.deadline_ms;
    TEST_ASSERT_TRUE_MESSAGE(next != UINT64_MAX, "simulation stalled");
    now = next;

    sim_dgram d;
    rdt_packet p;
    while (sim_pop(&to_recv, now, &d))
    {
      if (!rdt_decode(d.bytes, d.len, &p))
        continue;
      gbn_receiver_action ra;
      gbn_receiver_on_packet(&r, &p, now, &ra);
      TEST_ASSERT_FALSE_MESSAGE(*closed && ra.deliver_len > 0, "data after FIN");
      memcpy(sim_out + *out_len, ra.deliver, ra.deliver_len);
      *out_len += ra.deliver_len;
      if (ra.fin)
        *closed = true;
      if (ra.send_ack)
      {
        rdt_packet ack;
        rdt_make_ack(&ack, ra.ack_seq);
        sim_send(&to_send, &ack, now);
      }
    }
    while (sim_pop(&to_send, now, &d) && sa.status == GBN_RUNNING)
    {
      if (!rdt_decode(d.bytes, d.len, &p) || p.type != RDT_ACK)
        continue;
      gbn_sender_on_ack(&s, p.seq, now, &sa);
      sim_send_range(&to_recv, &s, &sa, now);
    }
    if (sa.status == GBN_RUNNING)
    {
      gbn_sender_on_tick(&s, now, &sa);
      sim_send_range(&to_recv, &s, &sa, now);
    }
  }
  return sa.status;
}

static void check_transfer(size_t size, uint32_t window, double rate, uint32_t seed)
{
  size_t out_len;
  bool closed;
  gbn_status st = sim_transfer(size, window, rate, seed, &out_len, &closed);
  char msg[128];
  snprintf(msg, sizeof msg, "size %zu window %u rate %.2f seed %u", size, window, rate, seed);
  TEST_ASSERT_EQUAL_MESSAGE(GBN_DONE, st, msg);
  TEST_ASSERT_TRUE_MESSAGE(closed, msg);
  TEST_ASSERT_EQUAL_size_t_MESSAGE(size, out_len, msg);
  if (size > 0)
  {
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(sim_in, sim_out, size, msg);
  }
}

void test_transfer_clean_channel(void)
{
  check_transfer(2500, 4, 0.0, 1);
  check_transfer(0, 4, 0.0, 1);
  check_transfer(8192, 1, 0.0, 1);
}

void test_transfer_lossy_channel_many_seeds(void)
{
  /* 20% loss, 20% corruption and 20% duplication in each direction */
  for (uint32_t seed = 1; seed <= 20; seed++)
  {
    check_transfer(100 * 1024 + 321, 8, 0.2, seed);
  }
  /* the channel really did damage things */
  TEST_ASSERT_TRUE(to_recv.dropped > 0 && to_recv.corrupted > 0 && to_recv.duplicated > 0);
  TEST_ASSERT_TRUE(to_send.dropped > 0 && to_send.corrupted > 0 && to_send.duplicated > 0);
}

void test_transfer_lossy_channel_windows_and_sizes(void)
{
  const uint32_t windows[] = {1, 2, 16, 64};
  const size_t sizes[] = {0, 1, 1024, 4096, 50000};
  for (size_t w = 0; w < sizeof windows / sizeof windows[0]; w++)
  {
    for (size_t z = 0; z < sizeof sizes / sizeof sizes[0]; z++)
    {
      check_transfer(sizes[z], windows[w], 0.2, (uint32_t)(w * 10 + z + 100));
    }
  }
}

void test_transfer_dead_channel_gives_up(void)
{
  size_t out_len;
  bool closed;
  /* rate 1.0: everything is dropped */
  gbn_status st = sim_transfer(5000, 4, 1.0, 3, &out_len, &closed);
  TEST_ASSERT_EQUAL(GBN_FAILED, st);
  TEST_ASSERT_EQUAL_size_t(0, out_len);
}

/* ------------------------------------------------------------------------ */
/* Layer 3 helpers                                                          */
/* ------------------------------------------------------------------------ */

static FILE *devnull;

static int parse(app_options *o, int argc, ...)
{
  static char storage[24][64];
  static char *argv[25];
  va_list ap;
  va_start(ap, argc);
  for (int i = 0; i < argc; i++)
  {
    snprintf(storage[i], sizeof storage[i], "%s", va_arg(ap, const char *));
    argv[i] = storage[i];
  }
  va_end(ap);
  argv[argc] = NULL;
  return app_parse_args(argc, argv, o, devnull);
}

void test_usage_prints(void)
{
  char buf[2048] = {0};
  FILE *f = fmemopen(buf, sizeof buf - 1, "w");
  TEST_ASSERT_NOT_NULL(f);
  app_usage(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "Usage: myapp send -s <session>"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "myapp recv -s <session> [-p port] <relay> <file>"));
}

void test_parse_args_send_defaults(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 6, "myapp", "send", "-s", "jdoe-1", "127.0.0.1", "in.bin"));
  TEST_ASSERT_EQUAL(APP_SEND, o.mode);
  TEST_ASSERT_EQUAL_STRING("jdoe-1", o.session);
  TEST_ASSERT_EQUAL_UINT32(8, o.window);
  TEST_ASSERT_EQUAL_UINT32(250, o.timeout_ms);
  TEST_ASSERT_TRUE(o.loss == 0.0);
  TEST_ASSERT_EQUAL_STRING("4250", o.port);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", o.relay);
  TEST_ASSERT_EQUAL_STRING("in.bin", o.file);
}

void test_parse_args_send_all_options(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 18, "myapp", "send", "-s", "a", "-w", "16", "-T", "100",
                                 "-l", "0.1", "-c", "0.05", "-d", "0.5", "-p", "20000",
                                 "localhost", "f"));
  TEST_ASSERT_EQUAL_UINT32(16, o.window);
  TEST_ASSERT_EQUAL_UINT32(100, o.timeout_ms);
  TEST_ASSERT_TRUE(o.loss == 0.1);
  TEST_ASSERT_TRUE(o.corrupt == 0.05);
  TEST_ASSERT_TRUE(o.dup == 0.5);
  TEST_ASSERT_EQUAL_STRING("20000", o.port);
  TEST_ASSERT_EQUAL_STRING("localhost", o.relay);
}

void test_parse_args_recv(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "recv", "-s", "x", "-p", "4251", "h", "out"));
  TEST_ASSERT_EQUAL(APP_RECV, o.mode);
  TEST_ASSERT_EQUAL_STRING("4251", o.port);
  TEST_ASSERT_EQUAL_STRING("out", o.file);
  /* the parser can run again after a previous run */
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 6, "myapp", "recv", "-s", "y", "h", "out"));
  TEST_ASSERT_EQUAL_STRING("y", o.session);
}

void test_parse_args_errors(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 1, "myapp"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 2, "myapp", "bogus"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 4, "myapp", "send", "h", "f")); /* no -s */
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 5, "myapp", "send", "-s", "a", "h"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 7, "myapp", "send", "-s", "a", "h", "f", "x"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 3, "myapp", "send", "-s")); /* missing arg */
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 7, "myapp", "send", "-s", "a", "-z", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 6, "myapp", "send", "-s", "Bad!", "h", "f"));
  /* recv does not take sender options */
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "recv", "-s", "a", "-w", "4", "h", "f"));

  const char *bad_windows[] = {"0", "65", "-1", "abc", "4x", ""};
  for (size_t i = 0; i < sizeof bad_windows / sizeof bad_windows[0]; i++)
  {
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-w", bad_windows[i], "h", "f"));
  }
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "send", "-s", "a", "-w", "64", "h", "f"));
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "send", "-s", "a", "-w", "1", "h", "f"));

  const char *bad_probs[] = {"0.51", "1", "-0.1", "nan", "inf", "0.1x", ""};
  for (size_t i = 0; i < sizeof bad_probs / sizeof bad_probs[0]; i++)
  {
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-l", bad_probs[i], "h", "f"));
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-c", bad_probs[i], "h", "f"));
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-d", bad_probs[i], "h", "f"));
  }
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "send", "-s", "a", "-T", "0", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "send", "-s", "a", "-p", "0", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "send", "-s", "a", "-p", "65536", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "recv", "-s", "a", "-p", "x", "h", "f"));
}

void test_valid_session(void)
{
  TEST_ASSERT_TRUE(app_valid_session("jdoe-1"));
  TEST_ASSERT_TRUE(app_valid_session("a"));
  TEST_ASSERT_TRUE(app_valid_session("0123456789abcdefghijklmnopqrstuv")); /* 32 */
  TEST_ASSERT_FALSE(app_valid_session("0123456789abcdefghijklmnopqrstuvw")); /* 33 */
  TEST_ASSERT_FALSE(app_valid_session(""));
  TEST_ASSERT_FALSE(app_valid_session(NULL));
  TEST_ASSERT_FALSE(app_valid_session("JDoe"));
  TEST_ASSERT_FALSE(app_valid_session("a b"));
  TEST_ASSERT_FALSE(app_valid_session("a_b"));
}

void test_format_hello(void)
{
  app_options o;
  char buf[APP_HELLO_MAX];
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 6, "myapp", "recv", "-s", "jdoe-1", "h", "f"));
  TEST_ASSERT_EQUAL_size_t(17, app_format_hello(&o, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("HELLO jdoe-1 recv", buf);

  TEST_ASSERT_EQUAL_INT(0, parse(&o, 10, "myapp", "send", "-s", "jdoe-1", "-l", "0.1",
                                 "-c", "0.05", "h", "f"));
  app_format_hello(&o, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("HELLO jdoe-1 send 0.1 0.05 0", buf);

  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "send", "-s", "a", "-d", ".5", "h", "f"));
  app_format_hello(&o, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("HELLO a send 0 0 0.5", buf);

  /* does not fit */
  TEST_ASSERT_EQUAL_size_t(0, app_format_hello(&o, buf, 5));
}

void test_parse_reply(void)
{
  char reason[64];
  TEST_ASSERT_EQUAL_INT(0, app_parse_reply((const uint8_t *)"OK", 2, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(0, app_parse_reply((const uint8_t *)"OK\n", 3, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR no receiver", 15, reason,
                                           sizeof reason));
  TEST_ASSERT_EQUAL_STRING("no receiver", reason);
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR", 3, reason, sizeof reason));
  TEST_ASSERT_EQUAL_STRING("", reason);
  /* reason is truncated to fit */
  char small[5];
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR session in use", 18, small,
                                           sizeof small));
  TEST_ASSERT_EQUAL_STRING("sess", small);
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR x", 5, NULL, 0));
  /* anything else, including a stray data packet, is not a reply */
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply((const uint8_t *)"OKAY", 4, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply((const uint8_t *)"ERROR", 5, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply((const uint8_t *)"", 0, reason, sizeof reason));
  uint8_t ack[RDT_MAX_PACKET];
  rdt_packet p;
  rdt_make_ack(&p, 3);
  size_t n = rdt_encode(&p, ack, sizeof ack);
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply(ack, n, reason, sizeof reason));
}

int main(void)
{
  devnull = fopen("/dev/null", "w");
  UNITY_BEGIN();
  RUN_TEST(test_checksum_rfc1071_example);
  RUN_TEST(test_checksum_odd_length_pads_with_zero);
  RUN_TEST(test_checksum_folds_carry);
  RUN_TEST(test_checksum_empty);
  RUN_TEST(test_checksum_catches_every_single_bit_flip);
  RUN_TEST(test_encode_worked_example_hi);
  RUN_TEST(test_encode_worked_example_ack3);
  RUN_TEST(test_make_ack);
  RUN_TEST(test_encode_full_payload_and_large_seq);
  RUN_TEST(test_encode_rejects_bad_input);
  RUN_TEST(test_decode_round_trip);
  RUN_TEST(test_decode_too_short);
  RUN_TEST(test_decode_length_mismatch);
  RUN_TEST(test_decode_length_field_over_max);
  RUN_TEST(test_decode_unknown_type_and_reserved);
  RUN_TEST(test_decode_bad_checksum);
  RUN_TEST(test_sender_init_counts_packets);
  RUN_TEST(test_sender_init_rejects_bad_args);
  RUN_TEST(test_sender_packet_cuts_file);
  RUN_TEST(test_sender_exact_multiple_of_1024);
  RUN_TEST(test_sender_empty_file);
  RUN_TEST(test_sender_window_full);
  RUN_TEST(test_sender_stop_and_wait);
  RUN_TEST(test_sender_cumulative_ack_slides_several);
  RUN_TEST(test_sender_duplicate_ack_ignored);
  RUN_TEST(test_sender_ack_beyond_next_ignored);
  RUN_TEST(test_sender_timeout_resends_whole_window);
  RUN_TEST(test_sender_gives_up_after_ten_timeouts);
  RUN_TEST(test_sender_progress_resets_timeout_count);
  RUN_TEST(test_sender_timer_stops_when_all_acked);
  RUN_TEST(test_receiver_in_order);
  RUN_TEST(test_receiver_duplicate);
  RUN_TEST(test_receiver_beyond_gap);
  RUN_TEST(test_receiver_fin_and_repeated_fin);
  RUN_TEST(test_receiver_empty_file);
  RUN_TEST(test_receiver_ignores_ack);
  RUN_TEST(test_receiver_linger_then_done);
  RUN_TEST(test_receiver_idle_gives_up);
  RUN_TEST(test_transfer_clean_channel);
  RUN_TEST(test_transfer_lossy_channel_many_seeds);
  RUN_TEST(test_transfer_lossy_channel_windows_and_sizes);
  RUN_TEST(test_transfer_dead_channel_gives_up);
  RUN_TEST(test_usage_prints);
  RUN_TEST(test_parse_args_send_defaults);
  RUN_TEST(test_parse_args_send_all_options);
  RUN_TEST(test_parse_args_recv);
  RUN_TEST(test_parse_args_errors);
  RUN_TEST(test_valid_session);
  RUN_TEST(test_format_hello);
  RUN_TEST(test_parse_reply);
  int rv = UNITY_END();
  fclose(devnull);
  return rv;
}

```

## Scripts Files
### run-results.sh

```c

#!/usr/bin/env bash
# Runs the README Results experiments: a 1 MiB file through the relay with
# --delay 50 (100 ms round trip), default 250 ms timeout, three runs per
# combination, every copy checked with cmp and every run timed with time.
set -u
cd "$(dirname "$0")/.."

APP=./build/release/myapp
IN=1mib.bin
OUT=$(mktemp -d)
RUNS=3

[[ -f $IN ]] || head -c 1048576 /dev/urandom > "$IN"
[[ -x $APP ]] || make release >/dev/null

python3 cs425_relay.py --delay 50 &
RELAY=$!
trap 'kill $RELAY 2>/dev/null; rm -rf "$OUT"' EXIT
sleep 1

TIMEFORMAT=%R
echo "window,loss,corrupt,dup,run,seconds,cmp"
for combo in "1 0 0 0" "16 0 0 0" "1 0.05 0 0" "16 0.05 0 0"; do
  read -r w l c d <<<"$combo"
  for run in $(seq 1 $RUNS); do
    session="res-w$w-l${l/./}-r$run-$$"
    $APP recv -s "$session" 127.0.0.1 "$OUT/out.bin" &
    recv=$!
    sleep 0.2
    secs=$( { time $APP send -s "$session" -w "$w" -l "$l" -c "$c" -d "$d" \
              127.0.0.1 "$IN" >/dev/null 2>&1; } 2>&1 )
    wait $recv
    if cmp -s "$IN" "$OUT/out.bin"; then ok=same; else ok=DIFFER; fi
    echo "$w,$l,$c,$d,$run,$secs,$ok"
    rm -f "$OUT/out.bin"
  done
done

```

Report generated on 10/07/2026 at 02:58:08


---

## End of Report

SHA-256 Hash of the report: 8a05cafa68301fcd0b5bd68d56629df3037cae40212c7adfa3a96cfb0a9f5a4e

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: maximumove/cs425-p2
- The repository visibility is public.
- The workflow was triggered by maximumove
