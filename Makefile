ERLANG_DIR = erlang
C_DIR = c_agent

c-build:
	$(MAKE) -C $(C_DIR) all

c-run:
	$(MAKE) -C $(C_DIR) run

c-clear:
	$(MAKE) -C $(C_DIR) clear

erlang-build:
	$(MAKE) -C $(ERLANG_DIR) build

erlang-run:
	$(MAKE) -C $(ERLANG_DIR) run

erlang-clear:
	$(MAKE) -C $(ERLANG_DIR) clear


.PHONY: c-build c-run c-clean erlang-build erlang-run erlang-clear