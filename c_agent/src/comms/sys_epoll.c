#include "../include/comms/sys_epoll.h"

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

int rearm_epoll_fd(int epoll_fd, int target_fd) {
    struct epoll_event event;
    event.data.fd = target_fd;
    event.events = EPOLLIN | EPOLLONESHOT;
    
    // epoll_ctl falla silenciosamente devolviendo -1 si target_fd ya fue cerrado
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, target_fd, &event);
}