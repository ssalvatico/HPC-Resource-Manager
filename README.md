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

From the project root:

```bash
make run
```

This executes:

```text
c_agent/c_agent 127.0.0.1 8000 4 8192 1
```

Alternatively, from the `c_agent/` directory:

```bash
./c_agent <ip> <port> <threads> <memory> <gpu>
```

---

## Running the Erlang Scheduler

Once the C agent is running:

```bash
make runclient
```

The scheduler connects to the C agent, retrieves the available nodes, and begins generating and scheduling jobs.

To execute the scheduler in test mode:

```bash
cd erlang
make run_test
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
