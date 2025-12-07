#!/usr/bin/env bash
set -euo pipefail

BIN="ets_server"
PID="$(pgrep "$BIN" || true)"
if [[ -z "$PID" ]]; then
  echo "Server '$BIN' not running."
  exit 1
fi

sudo perf record -F 200 -p "$PID" --call-graph lbr -- sleep 10
sudo perf script > out.perf

FG="$HOME/FlameGraph"
"$FG/stackcollapse-perf.pl" out.perf > out.folded
"$FG/flamegraph.pl" out.folded > flamegraph.svg

echo "Generated flamegraph.svg"
