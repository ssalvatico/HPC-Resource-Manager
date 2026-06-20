#include "../include/network_core.h"
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/timerfd.h>
#define LISTEN_BACKLOG 10

int create_tcp_listener(const char* ip_address, int port){
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addrlen = sizeof(server_addr);
    // 1. Create socket, 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) return-1;
    
    // 2. Config socket
    
    // set socket options to reuse address and port for testing purposes
    int optval = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))){
        close(sockfd);
        return -1;
    }
    
    server_addr.sin_family = AF_INET;           // IPV4           
    server_addr.sin_port = htons(port);         // host to network short (port)
    server_addr.sin_addr.s_addr = inet_addr(ip_address);   // addr to listen
    
    // 3. bind socket 
    if(bind(sockfd, (struct sockaddr*) &server_addr, addrlen)){
        close(sockfd);
        return -1;
    }
    
    // 4. Set socket to listen 
    if(listen(sockfd, LISTEN_BACKLOG)){
        close(sockfd);
        return -1;
    }
    
    // MAKE SOCKET NON BLOCKING
    // get current flags
    int flags = fcntl(sockfd, F_GETFL, 0);
    if(flags == -1) {
        close(sockfd);
        return -1;
    };

    // set non-blocking flag
    flags |= O_NONBLOCK;
    if(fcntl(sockfd, F_SETFL, flags) == -1) {
        close(sockfd);
        return -1;
    };

    return sockfd;
}

int connect_to_tcp_node(const char* target_ip, int target_port) {
    int sockfd;
    struct sockaddr_in remote_addr;
    
    // 1. create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return -1;
    }

    // make socket non blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // 2. config socket
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(target_port);
    remote_addr.sin_addr.s_addr = inet_addr(target_ip);
    

    // 3. connect socket puede interrumpirse el connect?
    if (connect(sockfd, (struct sockaddr*)&remote_addr, sizeof(remote_addr)) < 0) {
        // non blocking socket return -1 and set errno to EINPROGRESS socket still connectig
        if(errno != EINPROGRESS){
            close(sockfd);
            return -1;
        }
        return sockfd; // socket still connecting
    }
    
    return sockfd;
}

int accept_tcp_connection(int server_fd, char* client_ip_buffer) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // 1. accept
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd == -1) {
        return -1; 
    }

    // 2. write twh buffer
    if (client_ip_buffer != NULL) {
        // bytes to string
        char* ip_string = inet_ntoa(client_addr.sin_addr);
        strcpy(client_ip_buffer, ip_string);
    }

    // make socket non blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // 4. return client fd
    return client_fd;
}
// the same for all tcp messages using an already connected socket
int send_tcp_message(int sockfd, const char* message) {
    // Don't generate a SIGPIPE signal if the peer on a tcp socket has closed the connection
    int bytes = send(sockfd, message, strlen(message), MSG_NOSIGNAL);
    return bytes;
}

ssize_t receive_tcp_message(int sockfd, char* buffer, size_t buffer_size){
    ssize_t bytes = recv(sockfd, buffer, buffer_size - 1, 0); // Dejamos 1 byte para el \0
    if (bytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2; // No new data return to epoll, still waiting please
        }
        return -1; 
    }
    if (bytes == 0 ) return 0; 
    
    buffer[bytes] = '\0'; 
    return bytes;
}

int create_udp_listener_broadcaster(int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    // 1. create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1)return -1;
    
    // 2. config socket
    int optval = 1; // 1 = Activado
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        close(sockfd);
        return -1;
    }
    // allow broadcast
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
        close(sockfd);
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // 4. bind socket
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(sockfd);
        return -1;
    }
    
    // make socket non blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    return sockfd;
}

ssize_t broadcast_announce(int sockfd, int targetport, const char *message){
    struct sockaddr_in destaddr;
    // 1. config
    memset(&destaddr, 0, sizeof(destaddr));
    destaddr.sin_family = AF_INET;
    destaddr.sin_addr.s_addr = INADDR_BROADCAST; // todas las computadoras conectadas a la red local
    destaddr.sin_port = htons(targetport);
    // 2. send
    return sendto(sockfd, message, strlen(message), 0,(struct sockaddr*)&destaddr, sizeof(destaddr));
}

int process_discovery_datagram(int udpsockfd, char* buffer, const int buffer_size, char* out_sender_ip) {
    struct sockaddr_in senderaddr; // ¡Asegúrate de que sea sockaddr_in, de 16 bytes!
    socklen_t addr_len = sizeof(senderaddr);
    
    ssize_t answer = recvfrom(udpsockfd, buffer, buffer_size - 1, 0, (struct sockaddr*) &senderaddr, &addr_len);
    
    if(answer == -1) return -1; // error
    
    buffer[answer] = '\0'; // evita bugs
    if (out_sender_ip != NULL) {
        strcpy(out_sender_ip, inet_ntoa(senderaddr.sin_addr));
    }
    
    return 0;
}

int add_to_epoll_interest_list(int epoll_fd, int target_fd, uint32_t events){
    struct epoll_event event;
    event.data.fd = target_fd;
    event.events = events;
    // add target fd to epoll interest list 
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, target_fd, &event) == -1) return -1;

    return 0;
}

int remove_from_epoll_interest_list(int epoll_fd, int target_fd){
    struct epoll_event event;
    event.data.fd = target_fd;
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, target_fd, &event);
}

void init_server(ServerContext* ctx, int argc, char *argv[]) {
    if(argc != 3) exit(EXIT_FAILURE);
    
    ctx->port = atoi(argv[1]);
    ctx->lan_ip = argv[2];
    ctx->epollfd = epoll_create1(0);
    
    ctx->tcp_public_fd = create_tcp_listener(ctx->lan_ip, ctx->port);
    ctx->erlang_tcp_fd = create_tcp_listener("127.0.0.1", ctx->port);
    ctx->udp_fd = create_udp_listener_broadcaster(UDP_PORT);
    
    // tcp timerfd config
    ctx->tcp_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its_tcp = {0};
    its_tcp.it_value.tv_sec = 2; // 2 seconds shot

    // udp timer config
    ctx->udp_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its_udp = {0};
    its_udp.it_value.tv_sec = 1; // 1 second first shot
    its_udp.it_interval.tv_sec = 1;

    timerfd_settime(ctx->tcp_timerfd, 0, &its_tcp, NULL);
    timerfd_settime(ctx->udp_timerfd, 0, &its_udp, NULL);

    add_to_epoll_interest_list(ctx->epollfd, ctx->tcp_timerfd, EPOLLIN);
    add_to_epoll_interest_list(ctx->epollfd, ctx->udp_timerfd, EPOLLIN);
    add_to_epoll_interest_list(ctx->epollfd, ctx->udp_fd, EPOLLIN);
    
    return;
}

void handle_tcp_timer_expiration(ServerContext* ctx) {
    uint64_t exp;
    CHECK(read(ctx->tcp_timerfd, &exp, sizeof(uint64_t)), "Read tcp timer"); 
    
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
    CHECK(read(ctx->udp_timerfd, &exp, sizeof(uint64_t)), "Read udp timer");
    
    char mock_announce[256];
    sprintf(mock_announce, "ANNOUNCE %d cpu:4 mem:8192 gpu:1\n", ctx->port);
    
    broadcast_announce(ctx->udp_fd, UDP_PORT, mock_announce);
}

void handle_incoming_discovery(ServerContext* ctx) {
    char buffer[BUFFER_SIZE];
    char sender_ip[16];
    
    if (process_discovery_datagram(ctx->udp_fd, buffer, BUFFER_SIZE, sender_ip) == 0) {
        if (strcmp(sender_ip, ctx->lan_ip) == 0) return; 
        
        // (Acá Juani actualizará su tabla de nodos)
        printf("Descubierto nodo %s con recursos: %s\n", sender_ip, buffer);
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
        
        // Registrar en memoria y agregar al epoll
        active_connections[client_fd].is_active = 1;
        strncpy(active_connections[client_fd].ip, client_ip, 16);
        add_to_epoll_interest_list(ctx->epollfd, client_fd, EPOLLIN);
    }
}

void handle_client_message(ServerContext* ctx, int curr_fd) {
    char recv_buffer[BUFFER_SIZE];
    ssize_t bytes_read = receive_tcp_message(curr_fd, recv_buffer, BUFFER_SIZE);
    
    if (bytes_read > 0) {
        printf("Msg received (FD %d - IP: %s): %s\n", curr_fd, active_connections[curr_fd].ip, recv_buffer);
        
        char target_ip[16];
        int target_port = 0;
        int target_fd = -1;
        char msg_to_send[BUFFER_SIZE];

        // Se lo pasamos a la lógica pura
        int action = mock_juani(recv_buffer, curr_fd, target_ip, &target_port, &target_fd, msg_to_send);

        if (action == 1) {
            // Acción 1: Disparar el mensaje a donde indique Juani
            if (target_fd != -1 && active_connections[target_fd].is_active) {
                send_tcp_message(target_fd, msg_to_send);
            }
        } 
        else if (action == 2) {
            // Acción 2: Conectarse a otro nodo
            int new_fd = connect_to_tcp_node(target_ip, target_port);
            if (new_fd != -1 && new_fd < MAX_FDS) {
                active_connections[new_fd].is_active = 1;
                strncpy(active_connections[new_fd].ip, target_ip, 16);
                strncpy(active_connections[new_fd].pending_message, msg_to_send, BUFFER_SIZE);
                add_to_epoll_interest_list(ctx->epollfd, new_fd, EPOLLIN | EPOLLOUT);
            }
        }
    } 
    else if (bytes_read == 0) {
        printf("Client disconnected (FD %d).\n", curr_fd);
        active_connections[curr_fd].is_active = 0;
        active_connections[curr_fd].pending_message[0] = '\0';
        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);
    } 
    else if (bytes_read != -2) { // Si no es EAGAIN (-2), es un error grave
        printf("Unexpected disconnection (FD %d)\n", curr_fd);
        active_connections[curr_fd].is_active = 0;
        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);
    }
}

void handle_async_connection_success(ServerContext* ctx, int curr_fd) {
    int result;
    socklen_t result_len = sizeof(result);

    // Verificar si conectó o el nodo estaba apagado
    if (getsockopt(curr_fd, SOL_SOCKET, SO_ERROR, &result, &result_len) < 0 || result != 0) {
        printf("Failed to connect to node IP: (%s).\n", active_connections[curr_fd].ip);
        active_connections[curr_fd].is_active = 0;
        remove_from_epoll_interest_list(ctx->epollfd, curr_fd);
        close(curr_fd);
        return; // IMPORTANTE el return para cortar la ejecución
    }

    printf("Connected succesfully to %s (FD: %d)\n", active_connections[curr_fd].ip, curr_fd);

    // 1. Enviar el string retenido
    send_tcp_message(curr_fd, active_connections[curr_fd].pending_message);
    active_connections[curr_fd].pending_message[0] = '\0';

    // 2. Apagar EPOLLOUT cambiando el evento a solo lectura (EPOLLIN)
    struct epoll_event mod_event;
    mod_event.events = EPOLLIN;
    mod_event.data.fd = curr_fd;
    epoll_ctl(ctx->epollfd, EPOLL_CTL_MOD, curr_fd, &mod_event);
}