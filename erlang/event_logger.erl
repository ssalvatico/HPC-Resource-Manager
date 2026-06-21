-module(event_logger).
-include("header.hrl").
-export([init/1, init/0, log_event/4]).




%%% Entry point. Opens the log file in append mode and delegates to init/1.
%%% Creates event_logger.txt if it does not exist.
init() ->
    {ok,  Fd} = file:open("event_logger.txt", [raw, append, {encoding, "utf-8"}]),
    init(Fd).



%%% Main loop. Waits for #logInfo{} records and writes them to the log file.
%%% Params: Fd (file descriptor opened by init/0)
%%% Runs indefinitely.
init(Fd) ->
    receive
        #logInfo{status = S, src_method = M, detail = D, job_involved = J, timestamp = T} ->
            LogMsg = unicode:characters_to_binary(  io_lib:format("~w ~w ~p ~w ~w~n", [S,M,D,J,T])  ),
            file:write(Fd, LogMsg)
    end,
    init(Fd).



%%% Builds a #logInfo{} record and sends it to the logger process.
%%% Params: Status (ok | error), SrcMethod ({Module, Function}), 
%%% Detail (atom | string), JobInvolved (job_id | none)
log_event(Status, SrcMethod, Detail, JobInvolved) ->
    eventLoggerId ! #logInfo{status = Status,
                            src_method = SrcMethod,
                            detail = Detail,
                            job_involved = JobInvolved,
                            timestamp = ?TIMESTAMP
                        },
    ok.
