-module(erlang_c_bridge).
-include("header.hrl").
-import(event_logger, [log_event/4]).
-export([init/0, conn_handler/3, sender/3, receiver/3, connect/2]).

%%% Attempts to establish a TCP connection to the C agent at ?HOST:?PORT.
%%% Retries up to Nth_try times on failure. On success, spawns conn_handler.
%%% Params: servlogger (logger PID), Nth_try (remaining attempts), JobSchedulerId (scheduler PID)
connect(0, JobSchedulerId) ->
    log_event(error, {?MODULE, ?FUNCTION_NAME}, "Out of tries", none),
    JobSchedulerId ! {error, "Out of tries"},
    init:stop();
connect(Nth_try, JobSchedulerId) ->
    case gen_tcp:connect(?HOST , ?PORT , [binary, {active, false}] , ?TIMEOUT) of
        {ok, Socket} ->
            log_event(ok, {?MODULE, ?FUNCTION_NAME}, "Connection succesful", none),
            spawn(?MODULE, conn_handler, [servlogger, Socket, JobSchedulerId]);
    
        {error, Reason} -> 
            log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
            timer:sleep(?TIMEOUT),
            connect(Nth_try - 1, JobSchedulerId)
    end.
%%% Spawns the sender and receiver processes, then notifies the scheduler
%%% with their PIDs. Terminates after delegation.
conn_handler(servlogger, Socket, JobSchedulerId) ->
    ReceiverId = spawn(?MODULE, receiver, [servlogger, Socket, JobSchedulerId]),
    SenderId = spawn(?MODULE, sender, [servlogger, Socket, JobSchedulerId]),
    JobSchedulerId ! {receiver_pid, ReceiverId},
    JobSchedulerId ! {sender_pid, SenderId}.

%%% Listens on Socket for incoming responses from the C agent.
%%% Forwards each packet to the scheduler. On error, notifies scheduler and terminates.
%%% Runs indefinitely until a socket error occurs.
receiver(servlogger, Socket, JobSchedulerId) ->
    case gen_tcp:recv(Socket, 0, infinity) of
        {ok, Packet} ->
            JobSchedulerId ! {packet_received, Packet},
            log_event(ok, {?MODULE, ?FUNCTION_NAME}, "C response", none),
                                receiver(servlogger, Socket, JobSchedulerId);
        {error, Reason} ->
            JobSchedulerId ! {error, Reason},
            log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none)
    end,
    ok.

%%% Waits for directives from the scheduler and forwards them to the C agent via TCP.
%%% Handles: {get_nodes}, {job_directive, JobId, Packet}
%%% Runs indefinitely.
sender(servlogger, Socket, JobSchedulerId) ->
    receive
        {get_nodes} ->
            case gen_tcp:send(Socket, <<"GET_NODES">>) of
                ok ->
                    log_event(ok, {?MODULE, ?FUNCTION_NAME}, get_nodes, none),
                    JobSchedulerId ! {ok, get_nodes};
                {error, Reason} ->
                    log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
                    JobSchedulerId ! {error, get_nodes}
            end;
        % Here we cover the cases JOB_REQUEST and JOB_RELEASE
        {job_directive, JobId, Packet} ->
            case gen_tcp:send(Socket, Packet) of
                ok ->
                    log_event(ok, {?MODULE, ?FUNCTION_NAME}, job_directive, JobId),
                    JobSchedulerId ! {ok, JobId, job_directive};
                {error, Reason} ->
                    log_event(error, {?MODULE, ?FUNCTION_NAME}, Reason, JobId),
                    JobSchedulerId ! {error, JobId, job_directive}
            end
    end,
    sender(servlogger, Socket, JobSchedulerId).

%%% Entry point. Spawns the logger and scheduler processes, then initiates
%%% the TCP connection to the C agent.
init() -> 
    ServLoggerId = spawn(event_logger, init, []),
    register(servlogger, ServLoggerId),
    log_event(ok, {?MODULE, ?FUNCTION_NAME}, initialization, self()),
    JobSchedulerId = spawn(job_scheduler, init, []),
    connect(?TRIES, JobSchedulerId).

