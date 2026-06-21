#include "../include/network_core.h"
#include "../include/mock_resource_manager.h"
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
#include <pthread.h>
#define LISTEN_BACKLOG 10

AsyncPending async_pending[MAX_FDS]; // Reemplaza a async_pending_messages
pthread_rwlock_t pending_msg_lock = PTHREAD_RWLOCK_INITIALIZER;

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

    //garbage collector 
    ctx->gc_timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    struct itimerspec its_gc = {0};
    its_gc.it_value.tv_sec = 5;    // Primer disparo en 5 segundos
    its_gc.it_interval.tv_sec = 5;

    timerfd_settime(ctx->tcp_timerfd, 0, &its_tcp, NULL);
    timerfd_settime(ctx->udp_timerfd, 0, &its_udp, NULL);
    timerfd_settime(ctx->gc_timerfd, 0, &its_gc, NULL);

    add_to_epoll_interest_list(ctx->epollfd, ctx->tcp_timerfd, EPOLLIN);
    add_to_epoll_interest_list(ctx->epollfd, ctx->udp_timerfd, EPOLLIN);
    add_to_epoll_interest_list(ctx->epollfd, ctx->udp_fd, EPOLLIN | EPOLLONESHOT);
    add_to_epoll_interest_list(ctx->epollfd, ctx->gc_timerfd, EPOLLIN);
    
    return;
}

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
int rearm_epoll_fd(int epoll_fd, int target_fd) {
    struct epoll_event event;
    event.data.fd = target_fd;
    event.events = EPOLLIN | EPOLLONESHOT;
    
    // epoll_ctl falla silenciosamente devolviendo -1 si target_fd ya fue cerrado
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, target_fd, &event);
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