ERLANG_DIR = erlang
C_DIR = c_agent
PORT?=8000

all: c-build erlang-build

run: c-run

runclient: erlang-run

clear: c-clear erlang-clear
	@echo "C files deleted"
	@echo "Erlang files deleted"

c-build:
	$(MAKE) -C $(C_DIR)

c-run:
	$(MAKE) -C $(C_DIR) PORT=$(PORT) run

c-clear:
	$(MAKE) -C $(C_DIR) clear

erlang-build:
	$(MAKE) -C $(ERLANG_DIR) build

erlang-run:
	$(MAKE) -C $(ERLANG_DIR) $(( $(PORT) + 1 )) run

erlang-clear:
	$(MAKE) -C $(ERLANG_DIR) clear

.PHONY: all run runclient clear \
	c-build c-run c-clear \
	erlang-build erlang-run erlang-clear