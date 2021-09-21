#define _GNU_SOURCE
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define MIN(x,y) x < y ? x : y

// sbuf package based on linked list

//linked list node structure
typedef struct linked_node_t linked_node_t;
typedef void (*sbuf_iterate_callback_f)(void *value, int len, void *state, int pos, int start_pos, int end_pos);


//sbuf object
typedef struct sbuf_t sbuf_t;


/* initialize sbuf package
 *
 *  Parameters:
 *      sp = pointer to sbuf object already allocated by user
 * 
 * Initializes the front and back elements as well as
 * the semaphors.
 */
void sbuf_init(struct sbuf_t *sp);


/* deinit sbuf package
 *
 *  Parameters:
 *      sp = sbuf object
 * 
 * Loops through and frees all elements.
 */
void sbuf_deinit(struct sbuf_t *sp);


/* remove value by searching
 *
 *  Parameters:
 *      sp      = pointer to sbuf object already allocated by user
 *      value   = pointer to a memory location containing
 *                  some value that we want to search for
 *                  and remove from sbuf
 *      len     = length of value
 * 
 *  returns 1 if found and removed, 0 if not found
 */
int sbuf_remove_value(struct sbuf_t *sp, void *value, int len);


/* insert to the front of the queue
 *
 *  Parameters:
 *      sp      = pointer to sbuf object already allocated by user
 *      value   = pointer to a memory location containing
 *                  some value that we want to insert to
 *                  sbuf
 *      len     = length of value
 */
void sbuf_insert_front_value(struct sbuf_t *sp, void *value, int len);


/* remove from the back of the queue
 *
 *  Parameters:
 *      sp      = pointer to sbuf object already allocated by user
 *      retval  = pointer to a buffer which will hold the value
 *                to of the last node to be returned to the caller.
 *            *******CAN BE NULL. JUST WONT RETURN ANYTHING!!!!********
 *      len     = length of retval buffer
 *      lockitem= if lockitem is 1, sbuf will block until an item
 *                  becomes available. if lockitem is 0 sbuf will
 *                  return immediately.
 * 
 * return value: if found, returns the same pointer as "void *value" 
 *                  parameter. if not found returns 0 (NULL).
 */
void *sbuf_remove_end_value(struct sbuf_t *sp, void *retval, int len,
                           int lockitem);

/* peek end value
 *  
 *
 */
void *sbuf_peek_end_value_copy(struct sbuf_t *sp, void *retval, int len,
                           int lockitem);

/*
 * These functions are used to arbitrarily lock/unlock the removal functions.
 * They are very useful for fixing race conditions...
 */
void sbuf_removal_lock(struct sbuf_t *sp);
void sbuf_removal_unlock(struct sbuf_t *sp);

void sbuf_insert_value_position_from_front(struct sbuf_t *sp, void *value, int len, int position);
void sbuf_insert_value_position_from_back(struct sbuf_t *sp, void *value, int len, int position);
void *sbuf_remove_front_value(struct sbuf_t *sp, void *retval, int len,
                           int lockitem);
void sbuf_iterate(struct sbuf_t *sp, sbuf_iterate_callback_f callback, void *state, int start_pos, int end_pos);

void *sbuf_peek_end_value_direct(struct sbuf_t *sp, int *returned_len, int lockitem);
void sbuf_stop_peeking(struct sbuf_t *sp);
void sbuf_shuffle_random(struct sbuf_t *sp);
void sbuf_clear(struct sbuf_t *sp);
