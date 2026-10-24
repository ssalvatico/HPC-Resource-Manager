ERLANG_DIR = erlang
C_DIR = c_agent

PORT ?= 8000
HOST ?= 127.0.0.1
ENV ?= DEV

.PHONY: all run runclient clear c-build c-run c-clear erlang-build erlang-run erlang-clear

all: c-build erlang-build

run: c-run

runclient: erlang-run

clear: c-clear erlang-clear

c-build:
	$(MAKE) -C $(C_DIR)

c-run:
	$(MAKE) -C $(C_DIR) PORT=$(PORT) run

c-clear:
	$(MAKE) -C $(C_DIR) clear

erlang-build:
	$(MAKE) -C $(ERLANG_DIR) build

erlang-run: erlang-build
	HOST=$(HOST) PORT=$$(( $(PORT) + 1 )) ENV=$(ENV) $(MAKE) -C $(ERLANG_DIR) run

erlang-clear:
	$(MAKE) -C $(ERLANG_DIR) clear