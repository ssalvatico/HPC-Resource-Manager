#include "include/comms/server_init.h"
#include "include/comms/server_types.h"
#include "include/comms/event_handler.h"
#include "include/comms/thread_pool.h"
#include "include/resources/node-structures.h"
#include <sys/epoll.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h> 
#include <unistd.h>

#define NUM_THREADS 4

volatile sig_atomic_t server_running = 1;

void handle_sigint(int sig) {
    (void)sig; 
    printf("\nShutting down...\n");
    server_running = 0; // Rompe el bucle principal
}

// ./c_agent <puerto> <ip> <cpu> <gpu> <ram>
int main(int argc, char *argv[]) {
    // 1. Inicialización limpia y encapsulada
    ServerContext ctx;
    init_server(&ctx, argc, argv);
    unsigned cpu = (unsigned)atoi(argv[3]);
    unsigned gpu = (unsigned)atoi(argv[4]);
    unsigned ram = (unsigned)atoi(argv[5]);
    node_data_t my_node = node_init(cpu, gpu, ram);
    ctx.mynode = my_node;
    init_thread_pool(&ctx, NUM_THREADS);

    //signal(SIGINT, handle_sigint);

    struct epoll_event events[MAX_EVENTS];
    printf("Starting c_agent PORT: %d IP: %s CPU: %u GPU: %u RAM: %u\n",
           ctx.port, ctx.lan_ip, cpu, gpu, ram);

    // 2. El bucle Event-Dispatcher (Sin lógica mezclada)
    while(server_running) {
        int n_events = epoll_wait(ctx.epollfd, events, MAX_EVENTS, -1);
        
        for(int i = 0 ; i < n_events; i++) {
            int curr_fd = events[i].data.fd; 
            uint32_t event_type = events[i].events;

            if (curr_fd == ctx.tcp_timerfd) {
                handle_tcp_timer_expiration(&ctx);
            } 
            else if (curr_fd == ctx.udp_timerfd) {
                // Leemos el timer en main para destrabar el epoll
                uint64_t exp;
                read(ctx.udp_timerfd, &exp, sizeof(uint64_t));
                
                WorkerTask task = {.fd = -1, .type = TASK_UDP_ANNOUNCE};
                thread_pool_push_task(task);
            }
            else if (curr_fd == ctx.gc_timerfd) {
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
                    handle_connection_success(&ctx, curr_fd);
                }
            }
        }
    }

    thread_pool_destroy();
    node_dest(my_node);
    
    close(ctx.epollfd);
    close(ctx.tcp_public_fd);
    close(ctx.erlang_tcp_fd);
    close(ctx.udp_fd);

    return 0;
}
