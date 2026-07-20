#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CAGENT_DIR="$ROOT_DIR/c_agent"
ERLANG_DIR="$ROOT_DIR/erlang"

IP="${1:-127.0.0.1}"
PORT_A="${2:-8000}"
PORT_B="${3:-8001}"
CPU_A="${4:-4}"
MEM_A="${5:-8192}"
GPU_A="${6:-1}"
CPU_B="${7:-2}"
MEM_B="${8:-4096}"
GPU_B="${9:-0}"
THREADS="${10:-4}"
N_REQUESTS="${N_REQUESTS:-1}"
ERLANG_ENV="${ERLANG_ENV:-DEV}"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "El C Agent requiere Linux por epoll/timerfd" >&2
  exit 1
fi

cleanup() {
  echo "Cleaning up..."
  [[ -n "${C_PID_A:-}" ]] && kill "$C_PID_A" 2>/dev/null || true
  [[ -n "${C_PID_B:-}" ]] && kill "$C_PID_B" 2>/dev/null || true
  wait 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "Compiling..."
make -C "$CAGENT_DIR"
make -C "$ERLANG_DIR" build

echo "Starting C Agent A on $IP:$PORT_A (public) / $IP:$((PORT_A + 1)) (erlang)..."
"$CAGENT_DIR/c_agent" "$IP" "$PORT_A" "$CPU_A" "$MEM_A" "$GPU_A" "$THREADS" &
C_PID_A=$!

echo "Starting C Agent B on $IP:$PORT_B (public) / $IP:$((PORT_B + 1)) (erlang)..."
"$CAGENT_DIR/c_agent" "$IP" "$PORT_B" "$CPU_B" "$MEM_B" "$GPU_B" "$THREADS" &
C_PID_B=$!

echo "Waiting for agents to discover each other..."
sleep 3

echo "Starting Erlang scheduler, connecting to $IP:$((PORT_A + 1))..."
erl -pa "$ERLANG_DIR/ebin" -noshell \
  -eval "erlang_c_bridge:init(\"$IP\", $((PORT_A + 1)), $N_REQUESTS, \"$ERLANG_ENV\"), timer:sleep(infinity)."