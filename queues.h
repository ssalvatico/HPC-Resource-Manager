#ifndef __QUEUE_H__
#define __QUEUE_H__

typedef void    (* fDest) (void * dato);
typedef void *  (* fCopy) (void * dato);
typedef int     (* fComp) (void * dato1, void * dato2);

typedef struct _queue_elem {
    void * data;
    struct _queue_elem * next;
} queue_elem_t;

typedef queue_elem_t * queue_t;

/**
 *  Creates an empty queue
 */
queue_t queue_create    ();

/**
 *  Destroys existing queue and it's elements
 */
void    queue_destroy   (queue_t act_queue, fDest destroy);

/**
 *  Returns 1 if the queue is empty and 0 otherwise
 */
int     queue_empty     (queue_t act_queue);

/**
 *  appends a new element to the end of the queue
 */
queue_t queue_add       (queue_t act_queue, void * dato, fCopy copy);

/**
 *  returns a copy of the first element
 */
void *  queue_first     (queue_t act_queue, fCopy copy);

/**
 *  removes the first element of the queue
 */
queue_t queue_remove    (queue_t act_queue, fDest destroy);

/**
 *  removes an specific element of the queue
 */
queue_t queue_delete    (queue_t act_queue, void * data, fDest destroy, fComp compare);

/**
 *  updates an specific element of the queue
 */
queue_t queue_update    (queue_t act_queue, void * data, fDest destroy, fComp compare, fCopy copy);

#endif