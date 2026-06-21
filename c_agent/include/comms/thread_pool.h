#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "server_types.h"

/**
 * @brief Defines the different types of jobs the worker threads can execute.
 */
typedef enum {
    TASK_TCP_CLIENT_MSG,    // Handle incoming TCP data
    TASK_UDP_DISCOVERY,     // Process UDP discovery datagrams
    TASK_UDP_ANNOUNCE,      // Broadcast local state via UDP
    TASK_GARBAGE_COLLECTOR  // Clean up dead nodes
} TaskType;

/**
 * @brief Represents a unit of work to be processed by the thread pool.
 */
typedef struct {
    int fd;         // File descriptor associated with the task (if any)
    TaskType type;  // The specific action the worker needs to perform
} WorkerTask;

/**
 * @brief Initializes the thread pool, task queue, and spawns worker threads.
 * * Reference: man 3 pthread_create, man 3 pthread_mutex_init.
 * This function sets up a circular task queue and its synchronization 
 * primitives (mutex and condition variable). It then spawns the specified 
 * number of worker threads. These threads are created in a detached state, 
 * meaning their resources will be automatically released upon termination.
 * * @param ctx Pointer to the global ServerContext structure.
 * * @param num_threads The number of concurrent worker threads to spawn.
 * @return Void.
 */
void init_thread_pool(ServerContext* ctx, int num_threads);

/**
 * @brief Enqueues a new task into the thread pool's circular queue.
 * * Reference: man 3 pthread_mutex_lock, man 3 pthread_cond_signal.
 * This function securely acquires the queue lock, pushes the given task 
 * onto the tail of the circular buffer, and increments the pending count. 
 * It then signals one waiting worker thread via the condition variable 
 * to wake up and process the task. If the queue is at MAX_QUEUE capacity, 
 * the task is dropped and an error is logged.
 * * @param task The WorkerTask structure containing the job context.
 * @return Void.
 */
void thread_pool_push_task(WorkerTask task);

/**
 * @brief Attempts to cleanly shut down and destroy the thread pool resources.
 * * Reference: man 3 pthread_cond_broadcast, man 3 pthread_mutex_destroy.
 * Wakes up all sleeping worker threads and destroys the condition variable 
 * and mutex. Note: A shutdown flag must be implemented in the worker loop 
 * to prevent threads from locking a destroyed mutex.
 * @return Void.
 */
void thread_pool_destroy();

#endif