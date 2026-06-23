-module(job_scheduler).
-include("header.hrl").
-export([init/2, get_node_list/1]).


init(NRequests, Test) when is_integer(NRequests) and is_boolean(Test) ->
  init(#{}, NRequests, Test).
%% i.e State = #{sender_pid => Pid, receiver_pid => Pid, JobId1 => [Resources], JobId2 => ...}
init(State, NRequests, Test) when is_integer(NRequests) and is_boolean(Test) ->
  receive
    {receiver_pid, ReceiverId} ->
      State1 = maps:put(receiver_pid, ReceiverId, State),
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] receiver_pid received", none),
      init(State1, NRequests, Test);
    
    {sender_pid, SenderId} ->
      State1 = maps:put(sender_pid, SenderId, State),
      spawn(fun() -> request_nodes(SenderId) end),
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] sender_pid received", none),
      init(State1, NRequests, Test);
    
    {job_release, JobId} ->
      State1 = maps:remove(JobId, State),
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] releasing job", JobId),
      init(State1, NRequests, Test);
    
    {packet_received, Packet} ->
      case handle_packet(Packet) of 
        {node_records, NodeRecordList} ->
          NewState = lists:foldl(fun(_N, StateAcc) ->
            JobId = erlang:unique_integer([positive]),
            {JobRequest, AvailableResources} = generate_job_request(JobId, NodeRecordList, Test),
            event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_request", JobId),
            SenderId = maps:get(sender_pid, State),
            SenderId ! {job_directive, JobId, JobRequest},
            StateAcc#{JobId => {pending, AvailableResources}}
          end, State, lists:seq(1, NRequests)),
          init(NewState, NRequests, Test);
        
        {job_granted, JobId} ->
          State1 = maps:update_with(JobId, fun ({pending, Resources}) -> {granted, Resources};
                                               ({_, _}) ->
                                                  event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, "Invalid state transition", JobId),
                                                  throw("Invalid state transition") end, State),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_granted", JobId),
          spawn(fun () -> simulate_load(JobId, State1, self()) end),
          init(State1, NRequests, Test);

        {job_denied, JobId} ->
          State1 = maps:remove(JobId, State),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_denied", JobId),
          init(State1, NRequests, Test);

        {job_timeout, JobId} ->
          State1 = maps:remove(JobId, State),
          event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] job_timeout", JobId),
          init(State1, NRequests, Test);

        {skip, _} -> 
          event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, "unknown packet received", none),
          init(State, NRequests, Test)
      end;
    {ok, get_nodes} ->
      event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "[job_scheduler] get_nodes sent succesfully", none),
      init(State, NRequests, Test);
    
    {error, Reason} ->
      event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
      halt()
  end.



%%% Periodically requests the node list from the C agent every 5 seconds.
%%% Runs indefinitely.
%%% Params: SenderId (sender process PID)
request_nodes(SenderId) ->
  timer:sleep(5000),
  SenderId ! {get_nodes},
  request_nodes(SenderId).



%%% Dispatches incoming TCP packets based on their prefix.
%%% Returns tagged tuples: {node_records, List} | {job_granted, Id} | 
%%%                        {job_denied, Id} | {job_timeout, Id} | {skip, Reason}
handle_packet(<<"NODES ", Rest/binary>>) ->
    NodeList = get_node_list(Rest),
    case NodeList of
    [] -> {skip, "No nodes available"};
    _ ->
      NodeRecordList = lists:map(fun get_node/1, NodeList),
      {node_records, NodeRecordList}  
    end;
handle_packet(<<"JOB_GRANTED ", Rest/binary>>) ->
    {job_granted, list_to_integer(string:trim(binary_to_list(Rest)))};
handle_packet(<<"JOB_DENIED ", Rest/binary>>) ->
    {job_denied, list_to_integer(string:trim(binary_to_list(Rest)))};
handle_packet(<<"JOB_TIMEOUT ", Rest/binary>>) ->
    {job_timeout, list_to_integer(string:trim(binary_to_list(Rest)))};
handle_packet(_) ->
    {skip, "Unknown packet"}.



%%% Parses a semicolon-separated binary of nodes into a list of strings.
%%% Params: Packet (binary)
get_node_list(Packet) ->
  String = binary:bin_to_list(Packet),
  case String of
    "" -> [];
    _ -> string:split(String, ";", all)
  end.



%% Auxiliar Functions to get resources from each Node.

%%% Parses a single node string into a #node{} record.
%%% Format: "ip:port:cpu:N:mem:N:gpu:N"
get_node(Node) ->
    [Ip, Port | Resources] = string:split(Node, ":", all),
    % [{"cpu", 4}, {"gpu", 10}]
    ResourcePairs = to_pairs(Resources),
    lists:foldl(fun assign_resource/2 ,#node{ip = Ip, port = Port} ,ResourcePairs).


%%% Converts a flat list of alternating keys and values into tuples.
%%% Example: ["cpu", "4", "gpu", "1"] -> [{"cpu", 4}, {"gpu", 1}]
to_pairs([Key, Value | Rest]) ->
    [{Key, list_to_integer(Value)} | to_pairs(Rest)];
to_pairs([]) ->
    [].


%%% Fold function. Populates a #node{} record field from a {ResourceName, Amount} tuple.
assign_resource({"cpu", Amount}, NodeRecord) ->
    NodeRecord#node{cpu = Amount};
assign_resource({"gpu", Amount}, NodeRecord) ->
    NodeRecord#node{gpu = Amount}; 
assign_resource({"mem", Amount}, NodeRecord) ->
    NodeRecord#node{mem = Amount}.


  
%% Auxiliar functions to generate a JOB_REQUEST

%%% Generates a sorted JOB_REQUEST string and the list of requested resources.
%%% Resources are sorted by (NodeIp, ResourceType) to prevent deadlocks.
%%% Params: JobId (integer), NodeRecordList (list of #node{})
%%% Returns: {JobRequestString, SortedResources}
generate_job_request(JobId, NodeRecordList, Test) ->
  %1. Choose how many nodes we use.
  N = rand:uniform(length(NodeRecordList)),
  %2. Pick N random nodes
  SelectedNodes = pick_random(N, NodeRecordList),
  %3. Pick random resources from SelectNodes
  
  %% === ANTI DEADLOCK SORT === %%
  %% Sorting the Resources by resource name first and then by Ip.
  %% This garantees that if two different request requires the same resources from the same Node
  %% those same resources are in the same position in the request.
  %% Therefore, each Node fight for the same resource before requesting the next resource and none of the Nodes
  %% will lock a resource that another Node needs before requesting a resource that is no longer available.
  %% For example:
  %% Node 192.168.1.1:8000 with cpu:2
  %% Node 192.168.1.2:8000 with gpu:1
  %%
  %% JOB_REQUEST 123 192.168.1.1:8000:cpu:2;192.168.1.2:8000:gpu:1
  %% JOB_REQUEST 456 192.168.1.1:8000:cpu:2;192.168.1.2:8000:gpu:1
  %%
  %% When both request are fire at the same time, since the resources and Ips are sorted one of the JobId
  %% will "win" to acquire the `cpu:2` and then the other Job will be blocked waiting for that same reason.
  %% Hence, the first JobId can acquire the `gpu:1` because the other Job was not able to request that first.
  
  % i.e [ {"192.168.1.2", mem, 100} , {"192.168.1.2",cpu, 2}, {"192.168.1.1", gpu, 1}, {"192.168.1.1", cpu, 4} ]
  AvailableResources = lists:flatten(lists:filtermap(fun(SelectedNode) -> pick_resources(SelectedNode) end, SelectedNodes)),
  % i.e [ {"192.168.1.2",[{cpu,2}, {mem,100}]}, {"192.168.1.1", [{cpu,4}, {gpu,1}]} ]
  SortedResources = case Test of 
    true -> AvailableResources;
    false -> lists:sort(fun ({Ip1, Resource1, _}, {Ip2, Resource2, _}) ->
                          {Ip1, resource_order(Resource1)} =< {Ip2, resource_order(Resource2)}
                        end, AvailableResources)
  end,
  JobRequest = "JOB_REQUEST " ++ integer_to_list(JobId),
  JobRequest2 = lists:foldl(fun build_job_request/2, JobRequest, SortedResources) ++ "\n",
  {JobRequest2, SortedResources}.

%% This order is to be in the same page with other class groups.
resource_order(cpu) -> 1;
resource_order(mem) -> 2;
resource_order(gpu) -> 3.

%%% Shuffles a list using random float keys. Used to randomize node/resource selection.
shuffle(NodeRecordList) ->
  % 1. Pair each element with a random float number 
  Pairs = [{rand:uniform(), X} || X <- NodeRecordList],
    
  % 2. Sort the pairs by their random float key
  SortedPairs = lists:keysort(1, Pairs),
    
  % 3. Extract the original elements back into a flat list
  [X || {_, X} <- SortedPairs].



%%% Picks N random elements from a list using shuffle.
pick_random(N, NodeRecordList) ->
  ShuffleList = shuffle(NodeRecordList),
  lists:sublist(ShuffleList, N).



%%% Selects a random subset of available resources from a node.
%%% Returns {true, Resources} if resources are available, false otherwise.
%%% Resources format: [{Ip, ResourceType, Amount}]
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
                              {SelectedNode#node.ip, Res, Amount}
                            end, Selected),
      {true, Resources}
  end.



%%% Builds a JOB_REQUEST string entry for a single resource.
%%% Format: " @Ip:ResourceName:Amount"
% i.e JOB REQUEST 1001 @192.168.1.2:cpu:2 @192.168.1.3:gpu:1
build_job_request({Ip, Name, Amount}, Acc) ->
  Acc ++ " @" ++ Ip ++ ":" ++ atom_to_list(Name) ++ ":" ++ integer_to_list(Amount).



%%% Simulates job execution by sleeping a random amount of time,
%%% then releasing the job resources and notifying the scheduler.
%%% Params: JobId, State (map with sender_pid), InitPid (scheduler PID)
simulate_load(JobId, State1, InitPid) ->
  timer:sleep(rand:uniform(?WORK_TIME)),
  event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "Executing job...", JobId),
  maps:get(sender_pid, State1) ! {job_directive, JobId, "JOB_RELEASE " ++ integer_to_list(JobId) ++ "\n"},
  InitPid ! {job_release, JobId}.
