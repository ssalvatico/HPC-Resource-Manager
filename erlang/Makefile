EBIN = ebin

HOST?=$(shell hostname -I | awk '{print $$1}')
PORT?=8001
N_REQUESTS?=1
ENV?=DEV

MODULES = \
	erlang_c_bridge \
	event_logger \
	job_scheduler

$(EBIN):
	mkdir -p $(EBIN)

build: $(EBIN) $(MODULES:%=$(EBIN)/%.beam)

$(EBIN)/%.beam: %.erl header.hrl
	erlc -o $(EBIN) $<


run: build
	erl -pa $(EBIN) -noshell \
	-eval "erlang_c_bridge:init(\"$(HOST)\", $(PORT), $(N_REQUESTS), \"$(ENV)\"), timer:sleep(infinity)."

run_test: build
	erl -pa $(EBIN) -noshell \
	-eval "erlang_c_bridge:init(\"$(HOST)\", $(PORT), ?N_REQUESTS, \"TEST\"), timer:sleep(infinity)."
clear:
	@rm -rf $(EBIN)
	@rm -f *.dump
	@rm -f event_logger.txt

.PHONY: build run run_test clear