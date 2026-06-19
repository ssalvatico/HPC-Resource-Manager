-module(erlang_c_bridge).
-include("header.hrl").
-import(event_logger, [log_event/4]).
-export([init/0, conn_handler/2, sender/2, receiver/2, connect/2]).

%%% Attempts to establish a TCP connection to the C agent at ?HOST:?PORT.
%%% Retries up to Nth_try times on failure. On success, spawns conn_handler.
%%% Nth_try (remaining attempts), JobSchedulerId (scheduler PID)
connect(0, JobSchedulerId) ->
    event_logger:log_event(close, {?MODULE, ?FUNCTION_NAME}, "Out of tries", none),
    JobSchedulerId ! {error, "Out of tries"},
    timer:sleep(500),
    erlang:halt();
connect(Nth_try, JobSchedulerId) ->
    case gen_tcp:connect(?HOST , ?PORT , [binary, {active, false}] , ?TIMEOUT) of
        {ok, Socket} ->
            event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "Connection succesful", none),
            spawn(?MODULE, conn_handler, [Socket, JobSchedulerId]);
    
        {error, Reason} -> 
            timer:sleep(?TIMEOUT),
            event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
            connect(Nth_try - 1, JobSchedulerId)
    end.
%%% Spawns the sender and receiver processes, then notifies the scheduler
%%% with their PIDs. Terminates after delegation.
conn_handler(Socket, JobSchedulerId) ->
    ReceiverId = spawn(?MODULE, receiver, [Socket, JobSchedulerId]),
    SenderId = spawn(?MODULE, sender, [Socket, JobSchedulerId]),
    JobSchedulerId ! {receiver_pid, ReceiverId},
    JobSchedulerId ! {logger_id, ServLoggerId},
    JobSchedulerId ! {sender_pid, SenderId}.

%%% Listens on Socket for incoming responses from the C agent.
%%% Forwards each packet to the scheduler. On error, notifies scheduler and terminates.
%%% Runs indefinitely until a socket error occurs.
receiver(Socket, JobSchedulerId) ->
    case gen_tcp:recv(Socket, 0, infinity) of
        {ok, Packet} ->
            JobSchedulerId ! {packet_received, Packet},
            event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, "C response", none),
                                receiver(Socket, JobSchedulerId);
        {error, Reason} ->
            JobSchedulerId ! {error, Reason},
            event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none)
    end,
    ok.

%%% Waits for directives from the scheduler and forwards them to the C agent via TCP.
%%% Handles: {get_nodes}, {job_directive, JobId, Packet}
%%% Runs indefinitely.
sender(Socket, JobSchedulerId) ->
    receive
        {get_nodes} ->
            case gen_tcp:send(Socket, <<"GET_NODES">>) of
                ok ->
                    event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, get_nodes, none),
                    JobSchedulerId ! {ok, get_nodes};
                {error, Reason} ->
                    event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
                    JobSchedulerId ! {error, get_nodes}
            end;
        % Here we cover the cases JOB_REQUEST and JOB_RELEASE
        {job_directive, JobId, Packet} ->
            case gen_tcp:send(Socket, Packet) of
                ok ->
                    event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, job_directive, JobId),
                    JobSchedulerId ! {ok, JobId, job_directive};
                {error, Reason} ->
                    event_logger:log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, JobId),
                    JobSchedulerId ! {error, JobId, job_directive}
            end
    end,
    sender(Socket, JobSchedulerId).

%%% Entry point. Spawns the logger and scheduler processes, then initiates
%%% the TCP connection to the C agent.
init() -> 
    init(?PORT).

init(Port) ->
    ServLoggerId = spawn(event_logger, init, []),
    register(eventLoggerId, ServLoggerId),
    event_logger:log_event(ok, {?MODULE, ?FUNCTION_NAME}, initialization, self()),
    JobSchedulerId = spawn(job_scheduler, init, []),
    connect(?TRIES, JobSchedulerId).

