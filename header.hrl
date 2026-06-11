
-define(IP, "localhost").

-define(PORT, 8000).

-define(RES_ORDER(Type),
    case Type of
        cpu -> 1;
        gpu -> 2;
        ram -> 3
    end
).

-record(logInfo,{
    status,
    src_module,
    detail,
    job_involved,
    timestamp
}).

-record(resource,{
    type,   % cpu | gpu | ram
    node,   % C agent IP
    amount  % Integer
}).

-record(job,{
    id,         % job_id
    resources,  % sorted list of resources
    status,     % active | pending | denied
    nth_try     % Integer for the backoff to wait
}).

