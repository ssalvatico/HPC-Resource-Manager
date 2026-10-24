EBIN = ebin

HOST ?= localhost
PORT ?= 8001
ENV ?= DEV

MODULES = \
	erlang_c_bridge \
	event_logger \
	job_scheduler

.PHONY: build run clear

build: $(EBIN) $(MODULES:%=$(EBIN)/%.beam)

$(EBIN):
	mkdir -p $(EBIN)

$(EBIN)/%.beam: %.erl header.hrl | $(EBIN)
	erlc -o $(EBIN) $<

run: build
	HOST=$(HOST) PORT=$(PORT) ENV=$(ENV) erl -pa $(EBIN) -noshell \
	-eval "erlang_c_bridge:init(), timer:sleep(infinity)."

clear:
	@rm -rf $(EBIN)
	@rm -f *.dump event_logger.txt

.PHONY: build run clear