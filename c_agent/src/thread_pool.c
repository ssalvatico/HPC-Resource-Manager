#include "thread_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_QUEUE 1024

typedef struct {
    WorkerTask tasks[MAX_QUEUE]; 
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t notify;
} TaskQueue;

static TaskQueue queue;

/*
 *
 * El cond wait revisa el count de la cola, que 
 * claramente esta consulta esta protegido por el 
 * lock de la queue
 *  
 */

// Función interna que ejecutarán los hilos
static void* worker_thread(void* arg) {
    ServerContext* ctx = (ServerContext*)arg;
    while(1) {
        pthread_mutex_lock(&queue.lock);
        while(queue.count == 0) {
            pthread_cond_wait(&queue.notify, &queue.lock);
        }
        
        // Desencolar tarea completa
        WorkerTask current_task = queue.tasks[queue.head];
        queue.head = (queue.head + 1) % MAX_QUEUE;
        queue.count--;
        pthread_mutex_unlock(&queue.lock);

        // ¡El Switch que distribuye el trabajo!
        switch (current_task.type) {
            case TASK_TCP_CLIENT_MSG:
                if(handle_client_message(ctx, current_task.fd))rearm_epoll_fd(ctx->epollfd, current_task.fd);
                break;

            case TASK_UDP_DISCOVERY:
                handle_incoming_discovery(ctx);
                // El UDP siempre está vivo, lo rearmamos a la fuerza:
                struct epoll_event event;
                event.data.fd = ctx->udp_fd;
                event.events = EPOLLIN | EPOLLONESHOT;
                epoll_ctl(ctx->epollfd, EPOLL_CTL_MOD, ctx->udp_fd, &event);
                break;

            case TASK_UDP_ANNOUNCE:
                handle_udp_timer_expiration(ctx);
                break;

            case TASK_GARBAGE_COLLECTOR:
                // Solo llama a la limpieza, no hay socket que rearmar
                handle_gc_timer_expiration(ctx);
                break;
        }
    }
    return NULL;
}

void init_thread_pool(ServerContext* ctx, int num_threads) {
    //server_ctx = ctx;
    queue.head = 0; queue.tail = 0; queue.count = 0;
    pthread_mutex_init(&queue.lock, NULL);
    pthread_cond_init(&queue.notify, NULL);

    pthread_t thread;
    for(int i = 0; i < num_threads; i++) {
        pthread_create(&thread, NULL, worker_thread, (void*)ctx);
        pthread_detach(thread); // Para que liberen recursos solos al terminar
    }
}

void thread_pool_push_task(WorkerTask task) {
    pthread_mutex_lock(&queue.lock);
    if(queue.count < MAX_QUEUE) {
        queue.tasks[queue.tail] = task;
        queue.tail = (queue.tail + 1) % MAX_QUEUE;
        queue.count++;
        pthread_cond_signal(&queue.notify);
    } else {
        printf("Error: Cola de tareas llena\n");
    }
    pthread_mutex_unlock(&queue.lock);
}


// revisar
void thread_pool_destroy() {
    pthread_cond_broadcast(&queue.notify);
    usleep(100000);

    pthread_mutex_destroy(&queue.lock);
    pthread_cond_destroy(&queue.notify);
}
