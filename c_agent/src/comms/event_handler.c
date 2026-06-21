#include "../../include/comms/event_handler.h"
#include "../../include/comms/sys_sockets.h"
#include "../../include/comms/sys_epoll.h"
#include "../../include/resources/mock_resource_manager.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

pthread_rwlock_t pending_msg_lock = PTHREAD_RWLOCK_INITIALIZER;

AsyncPending async_pending[MAX_FDS]; 

/*
 *
 *  Las banderas de epolloneshot y epollout en el mismo evento
 * se usa para evitar bugs cuando el mismo cliente se conecta dos 
 * veces muy rapido 
 * 
 * Esta saca el fd de la interest list hasta que el hilo worker
 * termine de trabajar y ahi llama a la funcion rearm epoll para
 * volver a agregarlo a la interest list  
 * 
 * Gracias a diosito y el creador del kernel si el mismo fd
 * me quiere volver a mandar un mensaje en el instante que el fd
 * se sacas del interest list no se pierde nada porque lo guarda el 
 * kernel en un a especie de cola y en el mooento que lo pongamos
 * en el interest list va a salta otro evento epollin
 * 
 * Porque usamos pthread_rwlock en vez del lock normal?
 * lo usamos para proteger la tabla de active connections.
 * Como sucede que la mayoria los accesos a esta tabla son 
 * lectura. Este tipo de lock permite leer a todos los hilos 
 * que quiera pero cuando algun hilo esta escribiendo 
 * nadie puede consutlar la tabla
 * 
 * Lecturas :
 *  - handel cliente message con action == 1
 *  - rearm epoll fd (TODO EL TIEMPO EN TODAS)
 * Escrituras:
 *  - handle new tcp connection para conec nuevas
 *  - handle client message si action = 2 para conec
 *    a un nuevo puerto o cuando se desconecta
 *    de forma repentina 
 *  
*/

// the same for all tcp messages using an already connected socket

void handle_tcp_timer_expiration(ServerContext* ctx) {
    uint64_t exp;
    read(ctx->tcp_timerfd, &exp, sizeof(uint64_t)); 
    
    // Lo sacamos de epoll y lo destruimos
    remove_from_epoll_interest_list(ctx->epollfd, ctx->tcp_timerfd);
    close(ctx->tcp_timerfd);
    ctx->tcp_timerfd = -1; // Evita el bug del reciclaje de FDs
    
    // Empezamos a aceptar conexiones TCP
    add_to_epoll_interest_list(ctx->epollfd, ctx->tcp_public_fd, EPOLLIN);
    add_to_epoll_interest_list(ctx->epollfd, ctx->erlang_tcp_fd, EPOLLIN);
    
    printf("Activate TCP servers\n");
}

void handle_udp_timer_expiration(ServerContext* ctx) {
    uint64_t exp;
    read(ctx->udp_timerfd, &exp, sizeof(uint64_t));
    
    // juani me tiene que pasar solamente un string con los recuros locales 
    // formtateado con la norma de announce
    out_msg_t outbox[1];
    int outbox_count = 0;
    master_function(ctx->mynode, NULL, 0, NULL, outbox, &outbox_count, ACTION_GET_RESOURCES);
    
    broadcast_announce(ctx->udp_fd, UDP_PORT, outbox[0].message);
}

void handle_incoming_discovery(ServerContext* ctx) {
    char buffer[BUFFER_SIZE];
    char sender_ip[16];
    
    if (process_discovery_datagram(ctx->udp_fd, buffer, BUFFER_SIZE, sender_ip) == 0) {
        if (strcmp(sender_ip, ctx->lan_ip) == 0) return; 
        // con esto le digo a juani que hay un nuevo nodo descubierto y en buffer le paso 
        // el announce del emisor y la ip en sender ip
        out_msg_t dummy_outbox[1]; 
        int dummy_count = 0;
        master_function(ctx->mynode, sender_ip, 0, buffer, dummy_outbox, &dummy_count, ACTION_NEW_NODE_DISCOVERED);
    }
}

void handle_new_tcp_connection(ServerContext* ctx, int server_fd) {
    char client_ip[16];
    int client_fd = accept_tcp_connection(server_fd, client_ip);
    
    if (client_fd != -1 && client_fd < MAX_FDS) {
        if (server_fd == ctx->tcp_public_fd) {
            printf("[PUBLIC] New agent connected: %s (FD: %d)\n", client_ip, client_fd);
        } else {
            printf("[ERLANG] Planificador local conectado desde: %s (FD: %d)\n", client_ip, client_fd);
        }
        add_to_epoll_interest_list(ctx->epollfd, client_fd, EPOLLIN | EPOLLONESHOT);
    }
}

int handle_client_message(ServerContext* ctx, int curr_fd) {
    char peer_ip[16];
    get_ip_from_fd(curr_fd, peer_ip);
    
    char recv_buffer[BUFFER_SIZE];
    ssize_t bytes_read = receive_tcp_message(curr_fd, recv_buffer, BUFFER_SIZE);
    
    out_msg_t outbox[10];
    int outbox_count = 0;
    
    if (bytes_read > 0) {
        printf("Msg received (FD %d - IP: %s): %s\n", curr_fd, peer_ip, recv_buffer);
        

        // Juani modifica el outbox segun si quiere responder o no, el lo decide
        master_function(ctx->mynode, peer_ip, curr_fd, recv_buffer, outbox, &outbox_count, ACTION_RESPOND);

        // mandamos lo que nos dijo juani
        send_outbox(ctx, outbox, outbox_count);
        return 1;
    } else if (bytes_read == 0 || bytes_read != -2) {
        if (bytes_read == 0) {
            printf("Client disconnected (FD %d).\n", curr_fd);
        } else {
            printf("Unexpected disconnection (FD %d)\n", curr_fd);
        }
        
        master_function(ctx->mynode, peer_ip, curr_fd, NULL, outbox, &outbox_count, ACTION_DISCONNECTED);
        send_outbox(ctx, outbox, outbox_count);
        
        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);
        return 0;
    }
    return 1;
}

void handle_connection_success(ServerContext* ctx, int curr_fd) {
    int result;
    socklen_t result_len = sizeof(result);
    
    
    if (getsockopt(curr_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
    
        char failed_ip[16];
        pthread_rwlock_rdlock(&pending_msg_lock);
        strncpy(failed_ip, async_pending[curr_fd].ip, 16);
        pthread_rwlock_unlock(&pending_msg_lock);

        printf("Failed to connect to node IP: (%s).\n", failed_ip);
        
        out_msg_t outbox[10];
        int outbox_count = 0;
        master_function(ctx->mynode, failed_ip, curr_fd, "CONNECT_FAILED", outbox, &outbox_count, ACTION_DISCONNECTED);
        send_outbox(ctx, outbox, outbox_count);
        
        // Limpiar el mensaje fantasma
        pthread_rwlock_wrlock(&pending_msg_lock);
        async_pending[curr_fd].message[0] = '\0';
        async_pending[curr_fd].ip[0] = '\0'; // Buena práctica limpiar también la IP
        pthread_rwlock_unlock(&pending_msg_lock);

        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);
        return; 
    }

    char peer_ip[16];
    get_ip_from_fd(curr_fd, peer_ip); // IP para los logs y Juani a esta altura no deberia de dar error
    
    printf("Connected succesfully to %s (FD: %d)\n", peer_ip, curr_fd);
    
    // Leer el buffer, enviarlo y limpiarlo de forma segura
    pthread_rwlock_wrlock(&pending_msg_lock);
    send_tcp_message(curr_fd, async_pending[curr_fd].message);
    async_pending[curr_fd].message[0] = '\0';
    pthread_rwlock_unlock(&pending_msg_lock);

    struct epoll_event mod_event;
    mod_event.events = EPOLLIN | EPOLLONESHOT;
    mod_event.data.fd = curr_fd;
    epoll_ctl(ctx->epollfd, EPOLL_CTL_MOD, curr_fd, &mod_event);
}

void handle_gc_timer_expiration(ServerContext* ctx) {
    uint64_t exp;
    read(ctx->gc_timerfd, &exp, sizeof(uint64_t)); // Limpiar el timer
    
    out_msg_t outbox[10];
    int outbox_count = 0;
    
    // juani revisa nodos 
    master_function(
        ctx->mynode, 
        NULL,           // No hay IP
        -1,             // No hay socket
        NULL,           // No hay mensaje
        outbox, 
        &outbox_count, 
        ACTION_CHECK_DEADNODES 
    );

    // puede que quiere enviar un timeout o denied
    send_outbox(ctx, outbox, outbox_count);
}

void send_outbox(ServerContext* ctx, out_msg_t* outbox, int outbox_count) {
    for (int i = 0; i < outbox_count; i++) {
        if (outbox[i].target_fd != -1) {
            send_tcp_message(outbox[i].target_fd, outbox[i].message);
        } else {
            int new_fd = connect_to_tcp_node(outbox[i].target_ip, outbox[i].target_port);
            if (new_fd != -1 && new_fd < MAX_FDS) {
                
                // Guardamos el mensaje en la nueva tabla minimalista
                pthread_rwlock_wrlock(&pending_msg_lock);
                strncpy(async_pending[new_fd].ip, outbox[i].target_ip, 16);
                strncpy(async_pending[new_fd].message, outbox[i].message, BUFFER_SIZE - 1);
                async_pending[new_fd].message[BUFFER_SIZE - 1] = '\0';
                pthread_rwlock_unlock(&pending_msg_lock);
                
                add_to_epoll_interest_list(ctx->epollfd, new_fd, EPOLLIN | EPOLLOUT);
            }else{
                close(new_fd);
            }
        }
    }
}

void get_ip_from_fd(int fd, char* ip_buffer) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr*)&addr, &addr_len) == 0) {
        strcpy(ip_buffer, inet_ntoa(addr.sin_addr));
    } else {
        strcpy(ip_buffer, "UNKNOWN");
    }
}