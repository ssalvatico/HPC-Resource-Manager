ERLANG_DIR = erlang

erlang-build:
	$(MAKE) -C $(ERLANG_DIR) build

erlang-run:
	$(MAKE) -C $(ERLANG_DIR) run

erlang-clear:
	$(MAKE) -C $(ERLANG_DIR) clear