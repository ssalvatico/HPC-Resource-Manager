-module(erlang_c_bridge).
-include("header.hrl").
-export([init/0, conn_handler/3, sender/3, receiver/3]).

%%% Connect tiene la responsabilidad de conectarse al agente C
%%% en ?HOST:?PORT.
%%% Una vez conectado, notifica al proceso logger, 
%%% crea el proceso conn_handler y termina.
%%% Para evitar problemas, connect tiene 3 intentos para una conexion
%%% exitosa. En caso de error, notifica al logger y termina.

connect(ServLoggerId, 0, JobSchedulerId) ->
    log_event(ServLoggerId, error, {?MODULE, ?FUNCTION_NAME}, "Out of retries", none),
    JobSchedulerId ! {error, "Out of retries"};

connect(ServLoggerId, Nth_try, JobSchedulerId) ->
    case gen_tcp:connect(?HOST , ?PORT , [binary, {active, false}] , ?TIMEOUT) of
        {ok, Socket} ->
            log_event(ServLoggerId, ok, {?MODULE, ?FUNCTION_NAME}, "Connection succesful", none),
            spawn(?MODULE, conn_handler, [ServLoggerId, Socket, JobSchedulerId]);
    
        {error, Reason} -> 
            log_event(ServLoggerId, error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
                                connect(ServLoggerId, Nth_try - 1, JobSchedulerId)
    end.

conn_handler(ServLoggerId, Socket, JobSchedulerId) ->
    ReceiverId = spawn(?MODULE, receiver, [ServLoggerId, Socket, JobSchedulerId]),
    SenderId = spawn(?MODULE, sender, [ServLoggerId, Socket, JobSchedulerId]),
    JobSchedulerId ! {receiver_pid, ReceiverId},
    JobSchedulerId ! {sender_pid, SenderId}.

%%% C -> RESPONSE -> receiver -> scheduler
receiver(ServLoggerId, Socket, JobSchedulerId) ->
    case gen_tcp:recv(Socket, 0, ?TIMEOUT) of
        {ok, Packet} ->
            JobSchedulerId ! {ok, Packet},
            log_event(ServLoggerId, ok, {?MODULE, ?FUNCTION_NAME}, "C response", none),
                                receiver(ServLoggerId, Socket, JobSchedulerId);
        {error, Reason} ->
            JobSchedulerId ! {error, Reason},
            log_event(ServLoggerId, error, {?MODULE, ?FUNCTION_NAME}, Reason, none)
    end,
    ok.

%%% scheduler -> REQUEST -> sender -> C
sender(ServLoggerId, Socket, JobSchedulerId) ->
    receive
        {get_nodes} ->
            case gen_tcp:send(Socket, <<"GET_NODES">>) of
                ok ->
                    log_event(ServLoggerId, ok, {?MODULE, ?FUNCTION_NAME}, get_nodes, none),
                    JobSchedulerId ! {ok, get_nodes};
                {error, Reason} ->
                    log_event(ServLoggerId, error, {?MODULE, ?FUNCTION_NAME}, Reason, none),
                    JobSchedulerId ! {error, get_nodes}
            end;
        % Here we cover the cases JOB_REQUEST and JOB_RELEASE
        {job_directive, JobId, Packet} ->
            case gen_tcp:send(Socket, Packet) of
                ok ->
                    log_event(ServLoggerId, ok, {?MODULE, ?FUNCTION_NAME}, job_directive, JobId),
                    JobSchedulerId ! {ok, JobId, job_directive};
                {error, Reason} ->
                    log_event(ServLoggerId, error, {?MODULE, ?FUNCTION_NAME}, Reason, JobId),
                    JobSchedulerId ! {error, JobId, job_directive}
            end
    end,
    sender(ServLoggerId, Socket, JobSchedulerId).


init() -> 
    ServLoggerId = spawn(event_logger, init, []),
    JobSchedulerId = spawn(job_scheduler, init, []),
    connect(ServLoggerId, ?TRIES, JobSchedulerId).

log_event(ServLoggerId, Status, SrcMethod, Detail, JobInvolved) ->
    ServLoggerId ! #logInfo{status = Status,
                              src_method = SrcMethod,
                              detail = Detail,
                              job_involved = JobInvolved,
                              timestamp = ?TIMESTAMP},
    ok.
    
