#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 5 || $# -gt 6 ]]; then
  echo "
  Uso: 
  Agarra la IP con export IP=\$(hostname -I | awk '{print \$1}')
  Luego: $0 \$IP <puerto> <cpu> <mem> <gpu> [threads]" >&2
  exit 2
fi

IP="$1"
PORT="$2"
CPU="$3"
MEM="$4"
GPU="$5"
THREADS="${6:-4}"
N_REQUESTS="${N_REQUESTS:-1}"
ERLANG_ENV="${ERLANG_ENV:-DEV}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CAGENT_DIR="$ROOT_DIR/c_agent"
ERLANG_DIR="$ROOT_DIR/erlang"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "El C Agent requiere Linux por epoll/timerfd" >&2
  exit 1
fi

cleanup() {
  if [[ -n "${C_PID:-}" ]] && kill -0 "$C_PID" 2>/dev/null; then
    kill "$C_PID" 2>/dev/null || true
    wait "$C_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

make -C "$CAGENT_DIR"
make -C "$ERLANG_DIR" build

"$CAGENT_DIR/c_agent" "$IP" "$PORT" "$CPU" "$GPU" "$MEM" "$THREADS" &
C_PID=$!

erl -pa "$ERLANG_DIR/ebin" -noshell \
  -eval "erlang_c_bridge:init(\"127.0.0.1\", $PORT, $N_REQUESTS, \"$ERLANG_ENV\"), timer:sleep(infinity)."
