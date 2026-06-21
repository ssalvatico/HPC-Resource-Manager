
build: erlang_c_bridge.beam event_logger.beam job_scheduler.beam 

erlang_c_bridge.beam: header.hrl erlang_c_bridge.erl
	erlc erlang_c_bridge.erl

event_logger.beam: header.hrl event_logger.erl
	erlc event_logger.erl

job_scheduler.beam: header.hrl job_scheduler.erl
	erlc job_scheduler.erl

run:
	erl -noshell -eval "erlang_c_bridge:init(), timer:sleep(infinity)"
	

clear:
	rm -f *.beam
	rm ./event_logger.txt
	clear
