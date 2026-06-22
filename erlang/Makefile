EBIN = ebin

$(EBIN):
	mkdir -p $(EBIN)

$(EBIN)/erlang_c_bridge.beam: header.hrl erlang_c_bridge.erl | $(EBIN)
	erlc -o $(EBIN) erlang_c_bridge.erl

$(EBIN)/event_logger.beam: header.hrl event_logger.erl | $(EBIN)
	erlc -o $(EBIN) event_logger.erl

$(EBIN)/job_scheduler.beam: header.hrl job_scheduler.erl | $(EBIN)
	erlc -o $(EBIN) job_scheduler.erl

build: $(EBIN)/erlang_c_bridge.beam \
	$(EBIN)/event_logger.beam \
	$(EBIN)/job_scheduler.beam

run: build
	erl -pa $(EBIN) -noshell \
	-eval "erlang_c_bridge:init(), timer:sleep(infinity)"

run_test: build
	erl -pa $(EBIN) -noshell \
	    -eval "os:putenv(\"ENV\", \"test\"), erlang_c_bridge:init(8000, 3, true), timer:sleep(infinity)"

clear:
	rm -rf $(EBIN)
	rm -f event_logger.txt

.PHONY: build run run_test clear