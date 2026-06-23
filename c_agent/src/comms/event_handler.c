#include "../../include/comms/event_handler.h"
#include "../../include/comms/sys_sockets.h"
#include "../../include/comms/sys_epoll.h"
#include "../../include/comms/server_types.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

ConnectionState active_connections[MAX_FDS];
pthread_rwlock_t connections_lock = PTHREAD_RWLOCK_INITIALIZER;

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
    // we shall send the port on the socket parameter. Because master function need to know it
    // in order to craft the message 
    resource_adapter_patch(ctx->mynode, NULL, ctx->port, NULL, outbox, &outbox_count, ACTION_GET_RESOURCES);
    
    ssize_t sent = broadcast_announce(ctx->lan_ip, UDP_PORT, outbox[0].message);
    if (sent >= 0) {
        printf("[UDP] Sent from %s: %s", ctx->lan_ip, outbox[0].message);
    } else {
        printf("[UDP] Failed to send ANNOUNCE from %s\n", ctx->lan_ip);
    }
}

void handle_incoming_discovery(ServerContext* ctx) {
    char buffer[BUFFER_SIZE];
    char sender_ip[16];
    
    if (process_discovery_datagram(ctx->udp_fd, buffer, BUFFER_SIZE, sender_ip) == 0) {
        printf("[UDP] Received from %s: %s", sender_ip, buffer);
        // con esto le digo a juani que hay un nuevo nodo descubierto y en buffer le paso 
        // el announce del emisor y la ip en sender ip
        out_msg_t dummy_outbox[1]; 
        int dummy_count = 0;
        resource_adapter_patch(ctx->mynode, sender_ip, 0, buffer, dummy_outbox, &dummy_count, ACTION_NEW_NODE_DISCOVERED);
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
        
        // ALTA EN EL ESTADO DE RED
        pthread_rwlock_wrlock(&connections_lock);
        active_connections[client_fd].is_active = 1;
        strncpy(active_connections[client_fd].ip, client_ip, 16);
        active_connections[client_fd].port = 0;
        active_connections[client_fd].pending_message[0] = '\0';
        pthread_rwlock_unlock(&connections_lock);
        
        add_to_epoll_interest_list(ctx->epollfd, client_fd, EPOLLIN | EPOLLONESHOT);
    }
}

int handle_client_message(ServerContext* ctx, int curr_fd) {
    char target_ip[16];
    pthread_rwlock_rdlock(&connections_lock);
    strncpy(target_ip, active_connections[curr_fd].ip, 16);
    pthread_rwlock_unlock(&connections_lock);
    
    char recv_buffer[BUFFER_SIZE];
    ssize_t bytes_read = receive_tcp_message(curr_fd, recv_buffer, BUFFER_SIZE);
    
    out_msg_t outbox[MAX_OUTBOX];
    int outbox_count = 0;
    
    if (bytes_read > 0) {
        printf("Msg received (FD %d - IP: %s): %s\n", curr_fd, target_ip, recv_buffer);
        

        // Juani modifica el outbox segun si quiere responder o no, el lo decide
        resource_adapter_patch(ctx->mynode, target_ip, curr_fd, recv_buffer, outbox, &outbox_count, ACTION_RESPOND);

        // mandamos lo que nos dijo juani
        send_outbox(ctx, outbox, outbox_count);
        return 1;
    } else if (bytes_read == 0 || bytes_read != -2) {
        if (bytes_read == 0) {
            printf("Client disconnected (FD %d).\n", curr_fd);
        } else {
            printf("Unexpected disconnection (FD %d)\n", curr_fd);
        }
        
        resource_adapter_patch(ctx->mynode, target_ip, curr_fd, NULL, outbox, &outbox_count, ACTION_DISCONNECTED);
        send_outbox(ctx, outbox, outbox_count);
        
        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);

        // BAJA POR DESCONEXIÓN
        pthread_rwlock_wrlock(&connections_lock);
        active_connections[curr_fd].is_active = 0;
        active_connections[curr_fd].ip[0] = '\0';
        active_connections[curr_fd].port = 0;
        active_connections[curr_fd].pending_message[0] = '\0';
        pthread_rwlock_unlock(&connections_lock);
    }
    return 1;
}

void handle_connection_success(ServerContext* ctx, int curr_fd) {
    int result;
    socklen_t result_len = sizeof(result);
    
    // Rescatar IP de forma segura
    char target_ip[16];
    pthread_rwlock_rdlock(&connections_lock);
    strncpy(target_ip, active_connections[curr_fd].ip, 16);
    pthread_rwlock_unlock(&connections_lock);

    if (getsockopt(curr_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
        printf("Failed to connect to node IP: (%s).\n", target_ip);
        
        out_msg_t outbox[MAX_OUTBOX];
        int outbox_count = 0;
        resource_adapter_patch(ctx->mynode, target_ip, curr_fd, "CONNECT_FAILED", outbox, &outbox_count, ACTION_DISCONNECTED);
        send_outbox(ctx, outbox, outbox_count);
        
        // BAJA POR FALLO
        pthread_rwlock_wrlock(&connections_lock);
        active_connections[curr_fd].is_active = 0;
        active_connections[curr_fd].ip[0] = '\0';
        active_connections[curr_fd].port = 0;
        active_connections[curr_fd].pending_message[0] = '\0';
        pthread_rwlock_unlock(&connections_lock);

        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);
        return; 
    }

    printf("Connected succesfully to %s (FD: %d)\n", target_ip, curr_fd);
    
    // bien paso borramos el mensaje pendiente pero dejamos la badera en 1
    pthread_rwlock_wrlock(&connections_lock);
    int sent = send_tcp_message(curr_fd, active_connections[curr_fd].pending_message);
    printf("[OUTBOX] Sent pending to FD %d (%s): %s", curr_fd, target_ip, active_connections[curr_fd].pending_message);
    if (sent < 0) {
        printf("[OUTBOX] Failed sending pending to FD %d\n", curr_fd);
    }
    active_connections[curr_fd].pending_message[0] = '\0';
    pthread_rwlock_unlock(&connections_lock);

    struct epoll_event mod_event;
    mod_event.events = EPOLLIN | EPOLLONESHOT;
    mod_event.data.fd = curr_fd;
    epoll_ctl(ctx->epollfd, EPOLL_CTL_MOD, curr_fd, &mod_event);
}

void handle_gc_timer_expiration(ServerContext* ctx) {
    uint64_t exp;
    read(ctx->gc_timerfd, &exp, sizeof(uint64_t)); // Limpiar el timer
    
    out_msg_t outbox[MAX_OUTBOX];
    int outbox_count = 0;
    
    // juani revisa nodos 
    resource_adapter_patch(
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
        
        int target = outbox[i].target_fd;
        
        // 2. Si el adaptador NO sabe el FD (nos pasa -1), entonces si buscamos por IP
        // esto es para que en conexiones donde se repite la ip pero con distinto fd como 
        // el cliete erlang, no tenga errores, lo deberia de mandar mi funcin master
        if (target == -1) {
            target = find_fd_by_ip_port(outbox[i].target_ip, outbox[i].target_port);
        }

        // CASO A: Existe conexion 
        if (target != -1) {
            int sent = send_tcp_message(target, outbox[i].message);
            printf("[OUTBOX] Sent to FD %d (%s:%d): %s", target, outbox[i].target_ip, outbox[i].target_port, outbox[i].message);
            if (sent < 0) {
                printf("[OUTBOX] Failed sending to FD %d\n", target);
            }
        } 
        // CASO B: Abrimos conexion asincrona
        else {
            // puede que juani mande un granted a un nodo muerto si el fd destino no esta en la lista
            // arreglar
            int new_fd = connect_to_tcp_node(outbox[i].target_ip, outbox[i].target_port);
            if (new_fd != -1) {
                printf("[OUTBOX] Connecting to %s:%d for: %s", outbox[i].target_ip, outbox[i].target_port, outbox[i].message);
                if (new_fd < MAX_FDS) {
                    
                    // Guardamos el estado unificado
                    pthread_rwlock_wrlock(&connections_lock);
                    active_connections[new_fd].is_active = 1;
                    strncpy(active_connections[new_fd].ip, outbox[i].target_ip, 16);
                    active_connections[new_fd].port = outbox[i].target_port;
                    strncpy(active_connections[new_fd].pending_message, outbox[i].message, BUFFER_SIZE - 1);
                    active_connections[new_fd].pending_message[BUFFER_SIZE - 1] = '\0';
                    pthread_rwlock_unlock(&connections_lock);
                    
                    add_to_epoll_interest_list(ctx->epollfd, new_fd, EPOLLIN | EPOLLOUT);
                } else {
                    printf("Warning: FD limit reached. Dropping connection to %s\n", outbox[i].target_ip);
                    close(new_fd);
                }
            }
        }
    }
}

int find_fd_by_ip_port(const char* target_ip, unsigned target_port) {
    int found_fd = -1;
    pthread_rwlock_rdlock(&connections_lock);
    for (int i = 0; i < MAX_FDS; i++) {
        if (active_connections[i].is_active &&
            active_connections[i].port == target_port &&
            strncmp(active_connections[i].ip, target_ip, 15) == 0) {
            found_fd = i;
            break;
        }
    }
    pthread_rwlock_unlock(&connections_lock);
    return found_fd;
}

unsigned get_connection_port(int fd) {
    unsigned port = 0;
    if (fd < 0 || fd >= MAX_FDS) return 0;

    pthread_rwlock_rdlock(&connections_lock);
    if (active_connections[fd].is_active) {
        port = active_connections[fd].port;
    }
    pthread_rwlock_unlock(&connections_lock);
    return port;
}
