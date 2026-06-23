#!/usr/bin/env bash

# This test does not try to force a deadlock.
# Tests should pass, not be written to fail on purpose.
# The idea is to run two copies at the same time and watch resources get
# reserved and released without deadlock. That does not prove deadlock is
# impossible forever, but it gives a useful signal about the behavior.

set -euo pipefail

if [[ $# -ne 5 ]]; then
  echo "Uso: $0 <ip> <puerto> <cpu> <mem> <gpu>" >&2
  exit 2
fi

IP="$1"
PORT="$2"
CPU="$3"
MEM="$4"
GPU="$5"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CAGENT_DIR="$ROOT_DIR/c_agent"
ERLANG_DIR="$ROOT_DIR/erlang"

cleanup() {
  if [[ -n "${C_PID:-}" ]] && kill -0 "$C_PID" 2>/dev/null; then
    kill "$C_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

make -C "$CAGENT_DIR" clear
make -C "$CAGENT_DIR" build
make -C "$ERLANG_DIR" clear
make -C "$ERLANG_DIR" build

"$CAGENT_DIR/c_agent" "$IP" "$PORT" "$CPU" "$GPU" "$MEM" &
C_PID=$!

erl -pa "$ERLANG_DIR" -noshell -eval "erlang_c_bridge:init($PORT, 1, false), timer:sleep(infinity)"
