# HPC-Resource-Manager - How To Run


### Step 1 — Compile the modules
Run the following command to compile all Erlang source files:

*make*

This will generate `.beam` files for `erlang_c_bridge`, `event_logger` 
and `job_scheduler`.

### Step 2 — Start the C Agent + Erlang Agent
Before starting the scheduler, make sure the C agent is already running 
and listening on the host and port defined in `header.hrl`. 
The scheduler will try to connect to it automatically.

### Step 3 — Start the Erlang scheduler
Once the C agent is running, start the scheduler with:

*make run*

The scheduler will connect to the C agent, request the list of available 
nodes, and begin generating and sending job requests.

### Troubleshooting
If the connection fails, the scheduler will retry up to `?TRIES` times. 
If all attempts fail, it will log the error to `event_logger.txt` and exit.

### Clean up
To remove all compiled files and the event log, run:

*make clear*