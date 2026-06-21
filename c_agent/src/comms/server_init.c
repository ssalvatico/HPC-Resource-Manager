#include "../include/comms/server_init.h"
#include "../include/comms/sys_sockets.h"
#include "../include/comms/sys_epoll.h"
#include <stdio.h>
#include <stdlib.h> 

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