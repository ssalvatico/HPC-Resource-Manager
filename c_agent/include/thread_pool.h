#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "network_core.h"

typedef enum {
    TASK_TCP_CLIENT_MSG,
    TASK_UDP_DISCOVERY,
    TASK_UDP_ANNOUNCE,
    TASK_GARBAGE_COLLECTOR
} TaskType;

// Estructura que viajará por la cola
typedef struct {
    int fd;         // El socket (si aplica)
    TaskType type;  // Lo que el hilo debe hacer
} WorkerTask;

void init_thread_pool(ServerContext* ctx, int num_threads);

void thread_pool_push_task(WorkerTask task);

void thread_pool_destroy();

#endif