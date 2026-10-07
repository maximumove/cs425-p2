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
