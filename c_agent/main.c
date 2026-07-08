#include "include/comms/server_init.h"
#include "include/comms/server_types.h"
#include "include/comms/event_handler.h"
#include "include/comms/thread_pool.h"
#include "include/resources/node_structures.h"
#include <sys/epoll.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h> 
#include <unistd.h>

/**
 * @param argc program executable
 * @param argv array with [ip, port, cpu, gpu, ram, num_threads]
 * */ 
int main(int argc, char *argv[]) {
    // Clean initialization
    ServerContext ctx;
    init_server(&ctx, argc, argv);
    node_data_t my_node = init_node((unsigned)atoi(argv[3]), (unsigned)atoi(argv[4]), (unsigned)atoi(argv[5]));
    ctx.mynode = my_node;
    init_thread_pool(&ctx, atoi(argv[6]));
    // TODO: CERRAR DE FORMA LIMPIA EL SERVIDOR CERRANDO LOS
    // FD Y NO MANDANDO SIGPIPE DE UNA CON CTRL + C
    //signal(SIGINT, handle_sigint); 

    struct epoll_event events[MAX_EVENTS];
    printf("Starting c_agent PORT: %d IP: %s CPU: %u MEM: %u GPU: %u\n",
           ctx.port, ctx.lan_ip, atoi(argv[3]), atoi(argv[5]), atoi(argv[4]));

    while(1) {
        int n_events = epoll_wait(ctx.epollfd, events, MAX_EVENTS, -1);
        
        for(int i = 0 ; i < n_events; i++) {
            int curr_fd = events[i].data.fd; 
            uint32_t event_type = events[i].events;

            if (curr_fd == ctx.tcp_timerfd) {
                // After 2 seconds we start to attend new incoming tcp messages
                handle_tcp_timer_expiration(&ctx);
            } 
            else if (curr_fd == ctx.udp_timerfd) {
                // Periodicall UDP announce
                uint64_t exp;
                read(ctx.udp_timerfd, &exp, sizeof(uint64_t));
                
                WorkerTask task = {.fd = -1, .type = TASK_UDP_ANNOUNCE};
                thread_pool_push_task(task);
            }
            else if (curr_fd == ctx.gc_timerfd) {
                // REMOVER ESTO, PUEDO APROVECHAR LA LLAMADA
                // PERIODICA DEL ANNOUNCE 
                uint64_t exp;
                read(ctx.gc_timerfd, &exp, sizeof(uint64_t));
                
                WorkerTask task = {.fd = -1, .type = TASK_GARBAGE_COLLECTOR};
                thread_pool_push_task(task);
            }
            else if (curr_fd == ctx.udp_fd) {
                WorkerTask task = {.fd = ctx.udp_fd, .type = TASK_UDP_DISCOVERY};
                thread_pool_push_task(task);
            }
            else if (curr_fd == ctx.tcp_public_fd || curr_fd == ctx.erlang_tcp_fd) {
                handle_new_tcp_connection(&ctx, curr_fd);
            }
            else {
                if (event_type & EPOLLIN) {
                    WorkerTask task = {.fd = curr_fd, .type = TASK_TCP_CLIENT_MSG};
                    thread_pool_push_task(task);
                }
                if (event_type & EPOLLOUT) {
                    // Sending message to an no related node needs 2 
                    // epoll instances 
                    handle_connection_success(&ctx, curr_fd);
                }
            }
        }
    }
    // con acciones de cerrar limpio queda por hacer
    dest_node(my_node);
    return 0;
}
