-module(event_logger).
-include("header.hrl").
-export([init/1, init/0]).

init() ->
  {ok,  Fd} = file:open("event_logger.txt",[raw, append, {encoding, "utf-8"}]),
  init(Fd).
init(Fd) ->
  receive
    #logInfo{status = S, src_method = M, detail = D, job_involved = J, timestamp = T} ->
      LogMsg = unicode:characters_to_binary(  io_lib:format("~p ~p ~p ~p ~p~n", [S,M,D,J,T])  ),
        file:write(Fd, LogMsg)
  end,
  init(Fd).