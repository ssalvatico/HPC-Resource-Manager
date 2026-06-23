### HPC-Resource-Manager — How To Run

---
## Quick start (recommended)

From the project root, compile both components with:

`make`

Then start the C agent and Erlang scheduler separately (see below).


---
## Step 1 — Compile

To compile both components at once:

`make`

To compile each component individually:

`make c-build` — compiles the C agent into `c_agent/c_build`

`make erlang-build` — compiles Erlang modules into `erlang/ebin/`


---
## Step 2 — Start the C agent

The C agent must be running before the Erlang scheduler starts.
From the `c_agent/` directory:

`make run`

This starts the agent on port `8000` listening on `127.0.0.1`.
To use a different port or IP, run directly:

`./c_agent <port> <ip>`


---
## Step 3 — Start the Erlang scheduler

Once the C agent is running, from the `erlang/` directory:

`make run`

The scheduler will connect to the C agent, request the list of available
nodes, and begin generating and sending job requests.

To run in test mode:

`make run_test`


---
## Troubleshooting

If the connection fails, the scheduler will retry up to `?TRIES` times.
If all attempts fail, it will log the error to `erlang/event_logger.txt` and exit.

Make sure the C agent is running **before** starting the Erlang scheduler.


---
## Clean up

To remove all compiled files and logs:

`make clear`

To clean each component individually:

`make c-clear` — removes `c_agent/c_build/` and the `c_agent` binary

`make erlang-clear` — removes `erlang/ebin/`, `event_logger.txt`, and any crash dumps
