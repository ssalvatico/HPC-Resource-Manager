
-define(HOST, "127.0.0.1").
-define(PORT, 8000).
-define(TRIES, 3).

-define(TIMESTAMP, calendar:local_time()).

-define(TIMEOUT, 5000).
-define(WORK_TIME, 15000).
-define(N_REQUESTS, 1).

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

