#include "../../include/resources/queues.h"
#include <assert.h>
#include <stdlib.h>

queue_t queue_create    (){ return NULL; }

void    queue_destroy   (queue_t act_queue, fDest destroy){
    queue_elem_t * delete_this_elem;
    while(act_queue != NULL){
        delete_this_elem = act_queue;
        act_queue = act_queue -> next;
        destroy(delete_this_elem -> data);
        free(delete_this_elem);
    }
}

int     queue_empty     (queue_t act_queue) { return act_queue == NULL ;}

queue_t queue_add       (queue_t act_queue, void * dato, fCopy copy){
    if(act_queue == NULL){
        queue_t new_elem = malloc(sizeof(queue_elem_t));
        assert(new_elem != NULL);
        new_elem -> next = act_queue;
        new_elem -> data = copy(dato);
        return new_elem;
    }

    act_queue -> next = queue_add(act_queue -> next, dato, copy);
    return act_queue;
}

void *  queue_first     (queue_t act_queue, fCopy copy){
    if(queue_empty(act_queue))
        return NULL;

    void * out = copy(act_queue -> data);

    return out;
}

queue_t queue_remove    (queue_t act_queue, fDest destroy){
    if(act_queue == NULL)
        return NULL;
    
    queue_t delete_this_node = act_queue;
    act_queue = act_queue -> next;
    destroy(delete_this_node -> data);
    free(delete_this_node);

    return act_queue;
}

queue_t queue_delete    (queue_t act_queue, void * data, fDest destroy, fComp compare){
    if(act_queue == NULL)
        return NULL;
    
    if(!compare(act_queue -> data, data))
        return queue_remove(act_queue, destroy);

    act_queue -> next = queue_delete(act_queue -> next, data, destroy, compare);

    return act_queue;
}

queue_t queue_update    (queue_t act_queue, void * data, fDest destroy, fComp compare, fCopy copy){
    if(act_queue == NULL)
        return NULL;

    if(!compare(act_queue -> data, data)){
        destroy(act_queue -> data);
        act_queue -> data = copy(data);

        return act_queue;
    }

    act_queue -> next = queue_update(act_queue -> next, data, destroy, compare, copy);

    return act_queue;
}