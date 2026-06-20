#include "include/network_core.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

ConnectionState active_connections[MAX_FDS];

// NUEVA FIRMA: Recibe el FD de origen (origin_fd) y devuelve el FD de destino (out_target_fd)
int mock_juani(const char* message, int origin_fd, 
               char* out_target_ip, int* out_target_port, int* out_target_fd, char* out_message) {
    
    // SIMULACIÓN 1: Alguien pide recursos y NO tenemos -> ACCIÓN 2 (Conectar afuera)
    if (strncmp(message, "CONECTAR", 8) == 0) {
        strcpy(out_target_ip, "127.0.0.1"); 
        *out_target_port = 9000;            
        strcpy(out_message, "RESERVE 1001 cpu 2\n");
        
        // (En la vida real, acá Juani guardaría en su tabla: "El origin_fd pidió esto")
        
        return 2; // Trigger EPOLLOUT
    }
    
    // SIMULACIÓN 2: Alguien nos manda un ping y respondemos directo -> ACCIÓN 1 (Enviar)
    if (strncmp(message, "HOLA", 4) == 0) {
        *out_target_fd = origin_fd; // Respondemos al mismo que nos saludó
        strcpy(out_message, "HOLA DESDE EL AGENTE C\n");
        return 1;
    }

    return 0; // SILENCIO
}

// parametro 1 programa 2 puerto 3 ip publica
int main(int argc, char *argv[]) {
    // 1. Inicialización limpia y encapsulada
    ServerContext ctx;
    init_server(&ctx, argc, argv);
    memset(active_connections, 0, sizeof(active_connections));

    struct epoll_event events[MAX_EVENTS];
    printf("Starting c_agent PORT: %d IP: %s\n", ctx.port, ctx.lan_ip);

    // 2. El bucle Event-Dispatcher (Sin lógica mezclada)
    while(1) {
        int n_events = epoll_wait(ctx.epollfd, events, MAX_EVENTS, -1);
        
        for(int i = 0 ; i < n_events; i++) {
            int curr_fd = events[i].data.fd; 
            uint32_t event_type = events[i].events;

            if (curr_fd == ctx.tcp_timerfd) {
                handle_tcp_timer_expiration(&ctx);
            } 
            else if (curr_fd == ctx.udp_timerfd) {
                handle_udp_timer_expiration(&ctx);
            }
            else if (curr_fd == ctx.udp_fd) {
                handle_incoming_discovery(&ctx);
            }
            else if (curr_fd == ctx.tcp_public_fd || curr_fd == ctx.erlang_tcp_fd) {
                handle_new_tcp_connection(&ctx, curr_fd);
            }
            else {
                if (event_type & EPOLLIN) {
                    handle_client_message(&ctx, curr_fd);
                }
                if (event_type & EPOLLOUT) {
                    handle_async_connection_success(&ctx, curr_fd);
                }
            }
        }
    }
    return 0;
}