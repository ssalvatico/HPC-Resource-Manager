#include "include/network_core.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#define UDP_PORT 12529
#define SOCKET_ERROR -1
#define MAX_EVENTS 10
#define MAX_FDS 1024
#define BUFFER_SIZE 256

#define CHECK(expr, msg) do {           \
    if ((expr) == -1) {                 \
        perror(msg);                    \
        exit(EXIT_FAILURE);             \
    }                                   \
} while(0)

/* 
 * CONNECTION STATE MANAGEMENT
 * We use the file descriptor (fd) as the array index for O(1) fast access. */
typedef struct {
    int is_active;                      // 1 if slot is in use, 0 if free
    char ip[16];                        // IPv4 address of the remote node
    char pending_message[BUFFER_SIZE];  // Message waiting to be sent upon EPOLLOUT
} ConnectionState;

ConnectionState active_connections[MAX_FDS];

int mock_juani(const char* sender_ip, const char* message, 
               char* out_target_ip, int* out_target_port, char* out_message) {
    
    // Si escribimos "CONECTAR" en Netcat, el mock finge que necesita recursos de otro nodo
    if (strncmp(message, "CONECTAR", 8) == 0) {
        strcpy(out_target_ip, "127.0.0.1"); // Nos conectaremos a nosotros mismos por otro puerto para probar
        *out_target_port = 9000;            
        strcpy(out_message, "RESERVE 1001 cpu 2\n");
        return 1; // Trigger EPOLLOUT
    }
    return 0; 
}

// parametro 1 programa 2 puerto 3 ip publica
int main(int argc, char *argv[]){
    if(argc != 3)exit(EXIT_FAILURE);

    int port = atoi(argv[1]); // get port
    const char* lan_ip = argv[2];
    
    // Initialize the connection state array to 0
    memset(active_connections, 0, sizeof(active_connections));

    // tcp socket creation and config
    int tcp_public_fd = create_tcp_listener(lan_ip, port);
    CHECK(tcp_public_fd, "Create public tcp listening socket");
    int erlang_tcp_fd = create_tcp_listener("127.0.0.1", port);
    CHECK(erlang_tcp_fd, "Create erlang tcp listening socket");

    // udp socket creation and config
    int udp_fd = create_udp_listener_broadcaster(UDP_PORT);
    CHECK(udp_fd, "Create udp socket");

    // create epoll instance
    int epollfd = epoll_create1(0);
    CHECK(epollfd, "Create epoll instance");

    struct epoll_event events[MAX_EVENTS];

    // create timerfd for broadcast and first tcp usage
    int tcp_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its_tcp = {0};
    its_tcp.it_value.tv_sec = 2; // 2 seconds shot
    timerfd_settime(tcp_timerfd, 0, &its_tcp, NULL);

    int udp_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its_udp = {0};
    its_udp.it_value.tv_sec = 1; // 1 seconf first shot
    its_udp.it_interval.tv_sec = 1; // interval
    timerfd_settime(udp_timerfd, 0, &its_udp, NULL);
    
    // ctl add una vez expire el fd
    add_to_epoll_interest_list(epollfd, tcp_timerfd, EPOLLIN);
    add_to_epoll_interest_list(epollfd, udp_timerfd, EPOLLIN);
    add_to_epoll_interest_list(epollfd, udp_fd, EPOLLIN);

    // consumable variable 
    uint64_t exp;

    printf("Starting c_agent PORT: %d IP: %s\n", port, lan_ip);
    
    // main loop ONLY EPOLLIN TEMPORARY
    while(1){
        int n_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        
        for(int i = 0 ; i < n_events; i++){
            int curr_fd = events[i].data.fd; 
            // --- TIMER TCP ---
            if(curr_fd == tcp_timerfd){
                // Consume tcp_timerfd
                CHECK(read(tcp_timerfd, &exp, sizeof(uint64_t)), "Read tcp timer"); 
                
                remove_from_epoll_interest_list(epollfd, tcp_timerfd);
                close(tcp_timerfd);
                
                // Start accepting tcp connections 
                add_to_epoll_interest_list(epollfd, tcp_public_fd, EPOLLIN);
                add_to_epoll_interest_list(epollfd, erlang_tcp_fd, EPOLLIN);
                
                printf("Activate TCP servers\n");
            
            // --- TIMER UDP ---
            } else if(curr_fd == udp_timerfd){
                // consume udp_timerfd
                CHECK(read(udp_timerfd, &exp, sizeof(uint64_t)), "Read udp timer");
                // MOCK: Generar string de recursos en el formato exigido
                
                // FUNCION JUANI PARA OBTENER RECURSOS 
                char mock_announce[256];
                sprintf(mock_announce, "ANNOUNCE %s %d cpu:4 mem:8192 gpu:1\n", lan_ip, port);
                
                broadcast_announce(udp_fd, UDP_PORT, mock_announce);
                // printf("Broadcast enviado: %s", mock_announce); // Descomentar para debug
                
            // --- RECEIVE UDP NEW NODES---
            } else if(curr_fd == udp_fd){
                char buffer[BUFFER_SIZE];
                process_discovery_datagram(udp_fd, buffer, BUFFER_SIZE);
                if(strstr(buffer, lan_ip)!= NULL)continue;
                // FUNCION JUANI LE PASO EL BUFFER CON EL NUEVO ANNOUNCE
                
            // --- NEW TCP CONNECTION ---
            } else if(curr_fd == tcp_public_fd){
                char client_ip[16];
                int client_fd = accept_tcp_connection(tcp_public_fd, client_ip);
                if (client_fd != -1 && client_fd < MAX_FDS) {
                    printf("[PUBLIC] New agent connected: %s (FD: %d)\n", client_ip, client_fd);
                    
                    // Register connection in our state table
                    active_connections[client_fd].is_active = 1;
                    strncpy(active_connections[client_fd].ip, client_ip, 16);
                    
                    add_to_epoll_interest_list(epollfd, client_fd, EPOLLIN);
                }

            // --- FIRST ERLANG CONNECTION ---
            } else if(curr_fd == erlang_tcp_fd){
                char client_ip[16]; 
                int client_fd = accept_tcp_connection(erlang_tcp_fd, client_ip);
                if (client_fd != -1 && client_fd < MAX_FDS) {
                    printf("[ERLANG] Planificador local conectado desde: %s (FD: %d)\n", client_ip, client_fd);
                    
                    // Register connection
                    active_connections[client_fd].is_active = 1;
                    strncpy(active_connections[client_fd].ip, client_ip, 16);

                    add_to_epoll_interest_list(epollfd, client_fd, EPOLLIN);
                }

            // --- ALREADY CONNECTED TCP CLIENT OR ASYNC CONNECTION (SEND MESSAGES)---
           } else {
                // CASE A : INCOMING MESSAGE (ERLANG OR C AGENT)
                if (events[i].events & EPOLLIN) {
                    char recv_buffer[BUFFER_SIZE];
                    ssize_t bytes_read = receive_tcp_message(curr_fd, recv_buffer, BUFFER_SIZE);
                    
                    if (bytes_read > 0) {
                        printf("Msg received (FD %d - IP: %s): %s\n", curr_fd, active_connections[curr_fd].ip, recv_buffer);
                        
                        char target_ip[16];
                        int target_port;
                        char msg_to_send[BUFFER_SIZE];

                        // LE PASO A JUANO Y ME EDITA LAS VARIABLES SI HAY QUE RESPONDER
                        int requires_new_connection = mock_juani(
                            active_connections[curr_fd].ip, 
                            recv_buffer, 
                            target_ip, 
                            &target_port, 
                            msg_to_send
                        );

                        // SI JUANI QUIERE RECURSOS DE UN NUEVO NODE ME DICE Y ME CONECTO CON LAS VARIABLES EDITADAS
                        if (requires_new_connection) {
                            int new_fd = connect_to_tcp_node(target_ip, target_port);
                            
                            if (new_fd != -1 && new_fd < MAX_FDS) {
                                // 1. Register new connection details
                                active_connections[new_fd].is_active = 1;
                                strncpy(active_connections[new_fd].ip, target_ip, 16);
                                
                                // 2. Save the message Juani wants us to send
                                strncpy(active_connections[new_fd].pending_message, msg_to_send, BUFFER_SIZE);
                                
                                // 3. Wait for the socket to become writable (connection established)
                                add_to_epoll_interest_list(epollfd, new_fd, EPOLLIN | EPOLLOUT);
                            }
                        }

                    } else if (bytes_read == 0) {
                        // Clean disconnect
                        printf("Client disconnected (FD %d).\\n", curr_fd);
                        
                        // Clean up state
                        active_connections[curr_fd].is_active = 0;
                        active_connections[curr_fd].pending_message[0] = '\0';
                        
                        remove_from_epoll_interest_list(epollfd, curr_fd);
                        close(curr_fd);

                    } else if (bytes_read == -2) {
                        // EAGAIN: Wait for more data
                    } else {
                        perror("Error muerte tragedia");
                        active_connections[curr_fd].is_active = 0;
                        remove_from_epoll_interest_list(epollfd, curr_fd);
                        close(curr_fd);
                    }
                }

                // CASE B: ASYNC CONNECTION FINISHED (EPOLLOUT)
                if (events[i].events & EPOLLOUT) {
                    int result;
                    socklen_t result_len = sizeof(result);
                
                    // Check if async connect was successful
                    if (getsockopt(curr_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
                        printf("Failed to connecto node IP:(%s).\\n", active_connections[curr_fd].ip);
                        
                        // Clean up
                        active_connections[curr_fd].is_active = 0;
                        remove_from_epoll_interest_list(epollfd, curr_fd);
                        close(curr_fd);
                        continue;
                    }
                
                    printf("Connected succesfully to %s (FD: %d)\\n", active_connections[curr_fd].ip, curr_fd);
                
                    // 1. Send the pending message stored when connect_to_tcp_node was called
                    send_tcp_message(curr_fd, active_connections[curr_fd].pending_message);
                    
                    // Clear the pending message buffer
                    active_connections[curr_fd].pending_message[0] = '\0';
                
                    // 2. Remove EPOLLOUT to prevent busy loop, keep EPOLLIN to wait for response
                    struct epoll_event mod_event;
                    mod_event.events = EPOLLIN;
                    mod_event.data.fd = curr_fd;
                    epoll_ctl(epollfd, EPOLL_CTL_MOD, curr_fd, &mod_event);
                }
            }
        }
    }
    return 0;
}