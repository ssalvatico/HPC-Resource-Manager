ERLANG_DIR = erlang
C_DIR = c_agent

all: c-build erlang-build

run: c-run

runclient: erlang-run

clean: c-clean erlang-clean

clear: clean

c-build:
	$(MAKE) -C $(C_DIR)

c-run:
	$(MAKE) -C $(C_DIR) run

c-clean:
	$(MAKE) -C $(C_DIR) clear

erlang-build:
	$(MAKE) -C $(ERLANG_DIR) build

erlang-run:
	$(MAKE) -C $(ERLANG_DIR) run

erlang-clean:
	$(MAKE) -C $(ERLANG_DIR) clear

.PHONY: \
	all run clean clear \
	c-build c-run c-clean \
	erlang-build erlang-run erlang-clean