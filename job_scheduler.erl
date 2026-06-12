-module(job_scheduler).
-include("header.hrl").
-export([init/0]).

init() ->
  receive
    {receiver_pid, ReceiverId} ->
      io:fwrite("receiver_pid received"),
      init();
    {sender_pid, SenderPid} ->
      io:fwrite("sender_pid received"),
      init()
  end.