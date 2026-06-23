MODULES = erlang_c_bridge event_logger job_scheduler
BEAMS = $(addsuffix .beam,$(MODULES))

build: $(BEAMS)

%.beam: %.erl header.hrl
	erlc -o . $<

run: build
	erl -pa . -noshell \
	-eval "erlang_c_bridge:init(), timer:sleep(infinity)"

run_test: build
	erl -pa . -noshell \
	    -eval "os:putenv(\"ENV\", \"test\"), erlang_c_bridge:init(8000, 3, true), timer:sleep(infinity)"

clear:
	rm -f $(BEAMS)
	rm -f event_logger.txt
	rm -f *.dump

.PHONY: build run run_test clear
