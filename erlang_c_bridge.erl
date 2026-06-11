-module(erlang_c_bridge).
-include("header.hrl").


%% Esta mal, tengo que hacer que la funcion se conecte al agente C
%% El responsable de abrir el server es el agente, no yo
server(ServLogger) ->
    case gen_tcp:listen(?PORT,[binary, {active, false}]) of
    {ok, Socket} ->
        ServLogger ! #logInfo{status = ok,
                              src_module = server,
                              detail = none,
                              job_involved = self(),
                              timestamp = calendar:local_time()
                            },
        spawn(?MODULE, conn_handler, [Socket, ServLogger]);
    
    {error, Reason} -> 
        ServLogger ! #logInfo{status = error,
                              src_module = server,
                              detail = Reason,
                              job_involved = self(),
                              timestamp = calendar:local_time()
                            }
    end,
    ok.

conn_handler(Socket, ServLogger) ->
    case gen_tcp:accept(Socket) of
        {ok, ClientSocket} -> 
            
            spawn(?MODULE, packet_handler, [ClientSocket]);
        {error, closed} -> 
            ServLogger !
    end,
    ok.

init() -> ok.