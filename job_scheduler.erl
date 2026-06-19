-module(job_scheduler).
-include("header.hrl").
-export([init/0, get_node_list/1]).

init() ->
  init(#{}).

%% i.e State = #{sender_pid => Pid, receiver_pid => Pid, JobId1 => [Resources], JobId2 => ...}
init(State) ->
  receive
    {receiver_pid, ReceiverId} ->
      State1 = maps:put(receiver_pid, ReceiverId, State),
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] receiver_pid received", none),
      init(State1);
    {sender_pid, SenderId} ->
      State1 = maps:put(sender_pid, SenderId, State),
      spawn(fun() -> request_nodes(SenderId) end),
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] sender_pid received", none),
      init(State1);
    {job_release, JobId} ->
      State1 = maps:remove(JobId, State),
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] releasing job", JobId),
      init(State1);
    {packet_received, Packet} ->
      case handle_packet(Packet) of 
        {node_records, NodeRecordList} ->
          JobId = erlang:unique_integer([positive]),
          {JobRequest, AvailableResources} = generate_job_request(JobId, NodeRecordList),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_request", JobId),
          SenderId = maps:get(sender_pid, State),
          SenderId ! {job_directive, JobId, JobRequest},
          init(State#{JobId => {pending, AvailableResources}});
        
        {job_granted, JobId} ->
          State1 = maps:update(JobId, fun update_job_state/2, State),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_granted", JobId),
          spawn(fun () -> simulate_load(JobId, State1, self()) end),
          init(State1);

        {job_denied, JobId} ->
          State1 = maps:remove(JobId, State),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_denied", JobId),
          init(State1);

        {job_timeout, JobId} ->
          State1 = maps:remove(JobId, State),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_timeout", JobId),
          init(State1);

        {skip, _} -> 
          event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, "unknown packet received", none),
          init(State)
      end;
      % event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] node list", none),
    {ok, get_nodes} ->
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] get_nodes sent succesfully", none),
      init(State)
  end.

request_nodes(SenderId) ->
  timer:sleep(5000),
  SenderId ! {get_nodes},
  request_nodes(SenderId).

handle_packet(<<"NODES ", Rest/binary>>) ->
    NodeList = get_node_list(Rest),
    NodeRecordList = lists:map(fun get_node/1, NodeList),
    {node_records, NodeRecordList};
  
handle_packet(<<"JOB_GRANTED ", Rest/binary>>) ->
    {job_granted, list_to_integer(string:trim(binary_to_list(Rest)))};

handle_packet(<<"JOB_DENIED ", Rest/binary>>) ->
    {job_denied, list_to_integer(string:trim(binary_to_list(Rest)))};

handle_packet(<<"JOB_TIMEOUT ", Rest/binary>>) ->
    {job_timeout, list_to_integer(string:trim(binary_to_list(Rest)))};

handle_packet(_) ->
    {skip, "Unknown packet"}.

get_node_list(Packet) ->
  String = binary:bin_to_list(Packet),
  TrimedString = string:trim(String),
  string:split(TrimedString, ";", all).

%% Auxiliar Functions to get resources from each Node.
get_node(Node) ->
    [Ip, Port | Resources] = string:split(Node, ":", all),
    % [{"cpu", 4}, {"gpu", 10}]
    ResourcePairs = to_pairs(Resources),
    lists:foldl(fun assign_resource/2 ,#node{ip = Ip, port = Port} ,ResourcePairs).

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


%% Auxiliar functions to generate a JOB_REQUEST

generate_job_request(JobId, NodeRecordList) ->
  %1. Choose how many nodes we use.
  N = rand:uniform(length(NodeRecordList)),
  %2. Pick N random nodes
  SelectedNodes = pick_random(N, NodeRecordList),
  %3. Pick random resources from SelectNodes
  % i.e [{"192.168.1.1",[{cpu,2}, {mem,100}]}, {"192.168.1.2", [{gpu,1}]} ]
  AvailableResources = lists:flatten(lists:filtermap(fun(SelectedNode) -> pick_resources(SelectedNode) end, SelectedNodes)),
  % i.e JOB REQUEST 1001 @192.168.1.2:cpu:2 @192.168.1.3:gpu:1
  JobRequest = "JOB_REQUEST " ++ integer_to_list(JobId),
  {lists:foldl(fun build_job_request/2, JobRequest, AvailableResources), AvailableResources}.

shuffle(NodeRecordList) ->
  % 1. Pair each element with a random float number 
  Pairs = [{rand:uniform(), X} || X <- NodeRecordList],
    
  % 2. Sort the pairs by their random float key
  SortedPairs = lists:keysort(1, Pairs),
    
  % 3. Extract the original elements back into a flat list
  [X || {_, X} <- SortedPairs].

pick_random(N, NodeRecordList) ->
  ShuffleList = shuffle(NodeRecordList),
  lists:sublist(ShuffleList, N).

pick_resources(SelectedNode) ->
  NodeResources = [{cpu, SelectedNode#node.cpu}, {mem, SelectedNode#node.mem}, {gpu, SelectedNode#node.gpu}],
  % Only consider available resources per Node
  AvailableResources = lists:filter(fun ({_, undefined}) -> false;
                                        ({_, _}) -> true end, NodeResources),
  case AvailableResources of
    [] -> false;
    _ ->
      % Choose how many resources pick
      N = rand:uniform(length(AvailableResources)),
      % shuffle and take N
      Shuffled = shuffle(AvailableResources),
      Selected = lists:sublist(Shuffled, N),
      % For each resource, pick an amount in range
      Resources = lists:map(fun({Res, Max}) ->
                              Amount = rand:uniform(Max),
                              % {cpu, 4}
                              {Res, Amount}
                            end, Selected),
      {true, {SelectedNode#node.ip, Resources}}
  end.

% i.e JOB REQUEST 1001 @192.168.1.2:cpu:2 @192.168.1.3:gpu:1
% i.e Resources = [{cpu,2}, {mem,100}]
build_job_request({Ip, Resources}, Acc) ->
  ResourceString = lists:foldl(fun({Name, Amount}, InnerAcc) -> InnerAcc ++ ":" ++ atom_to_list(Name) ++ ":" ++ integer_to_list(Amount) end, "", Resources),
  Acc ++ " @" ++ Ip  ++ ResourceString.

update_job_state({pending, AvailableResources}, granted) ->
  {granted, AvailableResources};

update_job_state({pending, AvailableResources}, denied) ->
  {denied, AvailableResources};

update_job_state({_, _}, _) ->
  throw({invalid_state, "Invalid Job state transition"}).                          

simulate_load(JobId, State1, InitPid) ->
  timer:sleep(rand:uniform(?WORK_TIME)),
  maps:get(sender_pid, State1) ! {job_directive, JobId, "JOB_RELEASE " ++ integer_to_list(JobId)},
  InitPid ! {job_release, JobId}.


  
