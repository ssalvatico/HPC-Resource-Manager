#ifndef SERVER_TYPES_H
#define SERVER_TYPES_H

// macros and global data types

#define MAX_FDS 1024
#define BUFFER_SIZE 256
#define UDP_PORT 12529

// === MAIN  ===
typedef struct {
    int port;
    const char* lan_ip;
    int epollfd;
    int tcp_public_fd;
    int erlang_tcp_fd;
    int udp_fd;
    int tcp_timerfd;
    int udp_timerfd;
    int gc_timerfd; //garage collector
    // UNIQUE MODIFIABLE VARIABLE FROM SERVER CONTEXT
    void* mynode;
} ServerContext;

typedef enum {
    CONNECTION_UNKNOWN,
    CONNECTION_ERLANG,
    CONNECTION_AGENT
} ConnectionKind;

typedef struct {
    int is_active;                      
    char ip[16];                        
    unsigned port;
    ConnectionKind kind;
    char pending_message[BUFFER_SIZE];
    char receive_buffer[BUFFER_SIZE];
    unsigned receive_length;
    int discarding_line;
} ConnectionState;

typedef struct {
    char target_ip[16];   
    int target_port;      
    int target_fd;        
    char message[BUFFER_SIZE]; 
} out_msg_t;

#endif
