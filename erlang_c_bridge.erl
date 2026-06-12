-module(erlang_c_bridge).
-include("header.hrl").


%%% Connect tiene la responsabilidad de conectarse al agente C
%%% en ?HOST:?PORT.
%%% Una vez conectado, notifica al proceso logger, 
%%% crea el proceso conn_handler y termina.
%%% Para evitar problemas, connect tiene 3 intentos para una conexion
%%% exitosa. En caso de error, notifica al logger y termina.
connect(ServLoggerId, 0, JobSchedulerId) ->
    ServLoggerId ! #logInfo{status = error,
                            src_method = {?MODULE, ?FUNCTION_NAME},
                            detail = "Out of tries.",
                            job_involved = none,
                            timestamp = ?TIMESTAMP
                          },
    JobSchedulerId ! {error, "Out of tries"};
connect(ServLoggerId, Nth_try, JobSchedulerId) ->
    case gen_tcp:connect(?HOST , ?PORT , [binary, {active, false}] , ?TIMEOUT) of
        {ok, Socket} ->
            ServLoggerId ! #logInfo{status = ok,
                                    src_method = {?MODULE, ?FUNCTION_NAME},
                                    detail = "Connection succesful.",
                                    job_involved = none,
                                    timestamp = ?TIMESTAMP
                                },
            spawn(?MODULE, conn_handler, [ServLoggerId, Socket, JobSchedulerId]);
    
        {error, Reason} -> 
            ServLoggerId ! #logInfo{status = error,
                                    src_method = {?MODULE, ?FUNCTION_NAME},
                                    detail = Reason,
                                    job_involved = none,
                                    timestamp = ?TIMESTAMP
                                },
                                connect(ServLoggerId, Nth_try - 1, JobSchedulerId)
    end.

conn_handler(ServLoggerId, Socket, JobSchedulerId) ->
    ReceiverId = spawn(?MODULE, receiver, [ServLoggerId, Socket, JobSchedulerId]),
    SenderId = spawn(?MODULE, sender, [ServLoggerId, Socket, JobSchedulerId]),
    JobSchedulerId ! {ok, ReceiverId, SenderId}.

%%% C -> RESPONSE -> receiver -> scheduler
receiver(ServLoggerId, Socket, JobSchedulerId) ->
    case gen_tcp:recv(Socket, 0, ?TIMEOUT) of
        {ok, Packet} ->
            JobSchedulerId ! {ok, Packet},
            ServLoggerId ! #logInfo{status = ok,
                                    src_method = {?MODULE, ?FUNCTION_NAME},
                                    detail = "C response.",
                                    job_involved = none,
                                    timestamp = ?TIMESTAMP
                                },
                                receiver(ServLoggerId, Socket, JobSchedulerId);
        {error, closed} ->
            JobSchedulerId ! {error, closed},
            ServLoggerId ! #logInfo{status = error,
                                    src_method = {?MODULE, ?FUNCTION_NAME},
                                    detail = closed,
                                    job_involved = none,
                                    timestamp = ?TIMESTAMP
                                };
        {error, Reason} ->
            JobSchedulerId ! {error, Reason},
            ServLoggerId ! #logInfo{status = error,
                                    src_method = {?MODULE, ?FUNCTION_NAME},
                                    detail = Reason,
                                    job_involved = none,
                                    timestamp = ?TIMESTAMP
                                }
    end,
    ok.

%%% scheduler -> REQUEST -> sender -> C
sender(ServLoggerId, Socket, JobSchedulerId) ->
    receive
        {get_nodes} ->
            case gen_tcp:send(Socket, <<"GET_NODES">>) of
                ok ->
                    ServLoggerId ! #logInfo{status = ok,
                                            src_method = {?MODULE, ?FUNCTION_NAME},
                                            detail = get_nodes,
                                            job_involved = none,
                                            timestamp = ?TIMESTAMP
                                        },
                    JobSchedulerId ! {ok, get_nodes};
                {error, Reason} ->
                    ServLoggerId ! #logInfo{status = error,
                                            src_method = {?MODULE, ?FUNCTION_NAME},
                                            detail = Reason,
                                            job_involved = none,
                                            timestamp = ?TIMESTAMP
                                        },
                    JobSchedulerId ! {error, get_nodes}
            end;
        % Here we cover the cases JOB_REQUEST and JOB_RELEASE
        {job_directive, JobId, Packet} ->
            case gen_tcp:send(Socket, Packet) of
                ok ->
                    ServLoggerId ! #logInfo{status = ok,
                                            src_method = {?MODULE, ?FUNCTION_NAME},
                                            detail = job_directive,
                                            job_involved = JobId,
                                            timestamp = ?TIMESTAMP
                                        },
                    JobSchedulerId ! {ok, JobId, job_directive};
                {error, Reason} ->
                    ServLoggerId ! #logInfo{status = error,
                                            src_method = {?MODULE, ?FUNCTION_NAME},
                                            detail = Reason,
                                            job_involved = JobId,
                                            timestamp = ?TIMESTAMP
                                        },
                    JobSchedulerId ! {error, JobId, job_directive}
            end
    end,
    sender(ServLoggerId, Socket, JobSchedulerId).


init() -> 
    ServLoggerId = spawn(event_logger, init, []),
    JobSchedulerId = spawn(job_scheduler, init, []),
    connect(ServLoggerId, ?TRIES, JobSchedulerId).
