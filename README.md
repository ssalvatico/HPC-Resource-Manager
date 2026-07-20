# HPC Resource Manager — How to Run

---

## Quick Start

From the project root, compile both components:

```bash
make
```

This builds:

* The C agent executable (`c_agent/c_agent`)
* The C object files (`c_agent/c_build/`)
* The Erlang bytecode modules (`erlang/ebin/`)

---

## Building Components Individually

Compile only the C agent:

```bash
make c-build
```

Compile only the Erlang modules:

```bash
make erlang-build
```

---

## Running the C Agent

The C agent must be started before the Erlang scheduler.

From the `c_agent/` directory:

```bash
./c_agent <ip> <port> <cpu> <mem> <gpu> <threads>
```

**Note on ports:** each agent uses **two consecutive TCP ports**:

* `<port>` — public port, used for agent-to-agent communication.
* `<port> + 1` — local port, used exclusively by the Erlang scheduler.

Example:

```bash
./c_agent 192.168.1.11 4444 5 3 500 4
```

This starts the agent listening on `192.168.1.11:4444` (public) and
`192.168.1.11:4445` (Erlang interface).

---

## Running the Erlang Scheduler

Once the C agent is running, from the `erlang/` directory:

```bash
make run HOST=<c_agent_ip> PORT=<erlang_port>
```

Where `<erlang_port>` is the C agent's port **+ 1** (the local/Erlang port,
not the public one).

Example, connecting to the agent above:

```bash
make run HOST=192.168.1.11 PORT=4445
```

If `HOST` and `PORT` are omitted, defaults from `header.hrl` are used.

The scheduler connects to the C agent, retrieves the available nodes, and
begins generating and scheduling jobs.

### Connecting to a remote C agent

The Erlang scheduler can connect to a C agent running on a **different
machine** on the same network. To do so:

1. Confirm basic connectivity between machines:
```bash
   ping <c_agent_ip>
```
2. Confirm the target TCP port is reachable:
```bash
   nc -zv <c_agent_ip> <erlang_port>
```

---

## Provided script for testing

You can verify that no deadlock scenarios can occur with our implementation.

To run the test script:
```bash
./test_deadlock.sh
```

To run the test script providing parameters:
```bash
./test_deadlock.sh 127.0.0.1 8000 8001 4 8192 1 2 4096 0
```

---

## Project Structure

```text
c_agent/
    c_agent          Executable
    c_build/         Object files (.o)
    include/         Header files
    src/             C source code

erlang/
    ebin/            Compiled Erlang modules (.beam)
    *.erl            Erlang source files

docs/
    TP_Final.pdf
```

---

## Troubleshooting

* The C agent must be running before starting the Erlang scheduler.
* If the connection fails, the scheduler retries several times before aborting.
* Errors and events are written to:

```text
event_logger.txt
```

* Erlang crash dumps are generated automatically if the VM terminates unexpectedly.

### Firewall blocking inter-machine communication

If two C agents can discover each other via UDP (you see `[UDP] Received`
messages in both logs) but TCP connections between them fail or time out,
the local firewall is very likely blocking **incoming** TCP connections.

Symptoms:
* `JOB_REQUEST` involving a remote node results in `JOB_TIMEOUT`.
* `nc -zv <remote_ip> <port>` fails from the other machine, even though
  `ping` succeeds.

Fix — open the required TCP ports on **both** machines:

```bash
sudo ufw allow <public_port>/tcp
sudo ufw allow <public_port+1>/tcp   # Erlang port
sudo ufw enable
```

If the problem persists, temporarily disable the firewall to confirm it is
the cause:

```bash
sudo ufw disable
# test again
sudo ufw enable
```

If disabling the firewall does not resolve the issue, the network itself
(router/AP) may be blocking direct TCP connections between devices (client
isolation). In that case, testing on a single machine (using `127.0.0.1` or
a local LAN IP with different ports) or switching to a mobile hotspot is
recommended.

---

## Cleaning the Project

Remove all generated files:

```bash
make clear
```

This removes:

* `c_agent/c_build/`
* `c_agent/c_agent`
* `erlang/ebin/`
* Erlang crash dumps

To clean individual components:

```bash
make c-clear
make erlang-clear
```
