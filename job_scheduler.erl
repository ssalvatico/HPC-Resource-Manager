-module(job_scheduler).
-include("header.hrl").
-export([init/0]).

init() ->
  init(#{}).

init(State) ->
  receive
    {receiver_pid, ReceiverId} ->
      State1 = maps:put(receiver_pid, ReceiverId, State),
      io:fwrite("[job_scheduler] receiver_pid received ~n"),
      init(State1);
    {sender_pid, SenderId} ->
      State1 = maps:put(sender_pid, SenderId, State),
      SenderId ! {get_nodes},
      io:fwrite("[job_scheduler] sender_pid received~n"),
      init(State1);
    {packet_received, Packet} ->
      NodeList = get_node_list(Packet),
      io:fwrite("[job_scheduler] node list ~p ~n", [NodeList]);

    {ok, get_nodes} ->
      io:fwrite("[job_scheduler] get_nodes sent successfully~n"),
      init(State)
  end.

get_node_list(Packet) ->
  String = binary:bin_to_list(Packet),
  TrimedString = string:trim(String),
  string:split(TrimedString, ";", all).