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
 * @brief Creates and returns an empty queue.
 * @return An empty queue_t (NULL pointer representing an empty list).
 */
queue_t queue_create();

/**
 * @brief Destroys a queue and frees all its elements.
 *
 * Iterates through all elements, calling the provided destructor on each
 * element's data before freeing the node itself.
 *
 * @param act_queue The queue to destroy.
 * @param destroy   Function pointer used to free each element's data.
 * @return Void.
 */
void queue_destroy(queue_t act_queue, fDest destroy);

/**
 * @brief Checks whether a queue is empty.
 *
 * @param act_queue The queue to check.
 * @return 1 if the queue is empty, 0 otherwise.
 */
int queue_empty(queue_t act_queue);

/**
 * @brief Appends a new element to the end of the queue.
 *
 * Uses the provided copy function to duplicate the data before inserting.
 *
 * @param act_queue The current queue.
 * @param dato      Pointer to the data to insert.
 * @param copy      Function pointer used to copy the data.
 * @return The updated queue with the new element appended.
 */
queue_t queue_add(queue_t act_queue, void * dato, fCopy copy);

/**
 * @brief Returns a copy of the first element without removing it.
 *
 * @param act_queue The queue to peek at.
 * @param copy      Function pointer used to copy the data.
 * @return A copy of the first element's data, or NULL if the queue is empty.
 */
void * queue_first(queue_t act_queue, fCopy copy);

/**
 * @brief Removes the first element from the queue.
 *
 * Frees the removed node and its data using the provided destructor.
 *
 * @param act_queue The current queue.
 * @param destroy   Function pointer used to free the removed element's data.
 * @return The updated queue with the first element removed.
 */
queue_t queue_remove(queue_t act_queue, fDest destroy);

/**
 * @brief Removes a specific element from the queue by value.
 *
 * Searches for the first element matching data using the compare function
 * and removes it, freeing its memory via the destructor.
 *
 * @param act_queue The current queue.
 * @param data      Pointer to the value to search for and remove.
 * @param destroy   Function pointer used to free the matched element's data.
 * @param compare   Function pointer returning 0 when two elements match.
 * @return The updated queue with the matching element removed.
 */
queue_t queue_delete(queue_t act_queue, void * data, fDest destroy, fComp compare);

/**
 * @brief Updates a specific element in the queue by value.
 *
 * Searches for the first element matching data using the compare function.
 * If found, destroys the existing data and replaces it with a copy of
 * the new data.
 *
 * @param act_queue The current queue.
 * @param data      Pointer to the new data value to set.
 * @param destroy   Function pointer used to free the old element's data.
 * @param compare   Function pointer returning 0 when two elements match.
 * @param copy      Function pointer used to copy the new data.
 * @return The updated queue.
 */
queue_t queue_update(queue_t act_queue, void * data, fDest destroy, fComp compare, fCopy copy);

#endif