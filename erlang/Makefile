EBIN = ebin

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
		-eval "erlang_c_bridge:init(), timer:sleep(infinity)"

run_test: build
	erl -pa $(EBIN) -noshell \
		-eval "os:putenv(\"ENV\", \"test\"), erlang_c_bridge:init(8000,3,true), timer:sleep(infinity)"

clear:
	@rm -rf $(EBIN)
	@rm -f *.dump
	@rm -f event_logger.txt

.PHONY: build run run_test clear