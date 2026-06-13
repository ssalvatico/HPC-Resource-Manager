-module(job_scheduler).
-include("header.hrl").
-import(event_logger, [log_event/5]).
-export([init/0, get_node_list/1]).

init() ->
  init(#{}).

init(State) ->
  receive
    {receiver_pid, ReceiverId} ->
      State1 = maps:put(receiver_pid, ReceiverId, State),
      %% erlang_c_bridge:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] receiver_pid received", none),
      io:fwrite("[job_scheduler] receiver_pid received ~n"),
      init(State1);
    {sender_pid, SenderId} ->
      State1 = maps:put(sender_pid, SenderId, State),
      SenderId ! {get_nodes},
      %% erlang_c_bridge:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] sender received", none),
      io:fwrite("[job_scheduler] sender_pid received~n"),
      init(State1);
    {packet_received, Packet} ->
      handle_packet(Packet);
      %% erlang_c_bridge:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] node list", none),
    {ok, get_nodes} ->
      %% erlang_c_bridge:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] get_nodes sent succesfully", none),
      io:fwrite("[job_scheduler] get_nodes sent successfully~n"),
      init(State)
  end.

get_node_list(Packet) ->
  String = binary:bin_to_list(Packet),
  TrimedString = string:trim(String),
  string:split(TrimedString, ";", all).


handle_packet(<<"NODES ", Rest/binary>>) ->
    NodeList = get_node_list(Rest),
    NodeRecordList = lists:map(fun get_node/1, NodeList),
    io:fwrite("Node Record List ~p ~n", [NodeRecordList]);
  
handle_packet(<<"JOB_GRANTED ", Rest/binary>>) ->
    ok;

handle_packet(<<"JOB_DENIED ", Rest/binary>>) ->
    ok;

handle_packet(<<"JOB_TIMEOUT ", Rest/binary>>) ->
    ok.



%% Auxiliar Functions to get resources from each Node.

get_node(Node) ->
    NodeRecord = #node{},
    [Ip, Port | Resources] = string:split(Node, ":", all),
    % [{"cpu", 4}, {"gpu", 10}]
    ResourcePairs = to_pairs(Resources),
    NodeRecord2 = lists:foldl(fun assign_resource/2 ,NodeRecord,ResourcePairs),
    NodeRecord2#node{ip = Ip, port = Port}.

    
to_pairs([Key, Value | Rest]) ->
    [{Key, list_to_integer(Value)} | to_pairs(Rest)];
to_pairs([]) ->
    [].

assign_resource({"cpu", Amount}, NodeRecord) ->
    NodeRecord#node{cpu = Amount};
assign_resource({"gpu", Amount}, NodeRecord) ->
    NodeRecord#node{gpu = Amount}; 
assign_resource({"mem", Amount}, NodeRecord) ->
    NodeRecord#node{mem = Amount}.


%%


