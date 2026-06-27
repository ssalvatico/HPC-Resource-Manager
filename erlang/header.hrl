
-define(HOST, "127.0.0.1").
-define(PORT, 8001).
-define(TRIES, 3).

-define(TIMESTAMP, calendar:local_time()).

-define(TIMEOUT, 5000).
-define(WORK_TIME, 5000).
-define(GET_NODES_INTERVAL, 5000).
-define(N_REQUESTS, 1).
-define(TEST, false).


-record(logInfo,{
    status,
    src_method,
    detail,
    job_involved,
    timestamp
}).

% Represents a C Node data
-record(node,{
    ip,
    port,  
    cpu,
    mem,
    gpu
}).

