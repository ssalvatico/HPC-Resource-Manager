#ifndef SYS_EPOLL_H
#define SYS_EPOLL_H

#include <sys/epoll.h>
#include <sys/types.h>

/*
 * @brief Adds a file descriptor to the epoll interest list.
 * * Reference: man 2 epoll_ctl.
 * This function performs the EPOLL_CTL_ADD control operation on the 
 * epoll instance. It tells the kernel to monitor the target_fd for 
 * the events specified in the event mask (e.g., EPOLLIN).
 * * @param epollfd The file descriptor of the epoll instance.
 * @param targetfd The file descriptor of the socket to be monitored.
 * @param events The bit mask specifying the events of interest.
 * @return 0 on success, or -1 on error.
 */
int add_to_epoll_interest_list(int epollfd, int targetfd, uint32_t events);

/**
 * @brief Removes a file descriptor from the epoll monitoring list.
 * * Reference: man 2 epoll_ctl.
 * This function performs the EPOLL_CTL_DEL control operation. It tells 
 * the kernel to stop monitoring the target_fd. This function should generally 
 * be called before close(target_fd) when a client disconnects to prevent 
 * resource leaks or phantom events in the epoll loop.
 * * @param epoll_fd The file descriptor of the central epoll instance.
 * @param target_fd The file descriptor of the socket to be removed.
 * @return 0 on success, or -1 on error.
 */
int remove_from_epoll_interest_list(int epoll_fd, int target_fd);

/**
 * @brief Re-arms a file descriptor in the epoll interest list after EPOLLONESHOT.
 *
 * Reference: man 2 epoll_ctl.
 * When a file descriptor is registered with EPOLLONESHOT, epoll automatically
 * disables it after the first event fires. This function re-enables monitoring
 * by performing an EPOLL_CTL_MOD operation with EPOLLIN | EPOLLONESHOT,
 * allowing the worker thread to safely signal completion before re-arming.
 * Fails silently if the target_fd has already been closed.
 *
 * @param epoll_fd The file descriptor of the central epoll instance.
 * @param target_fd The file descriptor to re-arm in the interest list.
 * @return 0 on success, or -1 on error.
 */
int rearm_epoll_fd(int epoll_fd, int target_fd);

#endif