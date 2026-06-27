EBIN = ebin

HOST=127.0.0.1
PORT=8000
ENV=DEV

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
	HOST=$(HOST) PORT=$(PORT) \
	erl -pa $(EBIN) -noshell \
		-eval "erlang_c_bridge:init(), timer:sleep(infinity)"

run_test: build
	ENV=TEST
	erl -pa $(EBIN) -noshell \
		-eval "erlang_c_bridge:init(8000,3,true), timer:sleep(infinity)"

clear:
	rm -rf $(EBIN)
	rm -f *.dump
	rm event_logger.txt


.PHONY: build run run_test clear