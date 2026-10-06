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
