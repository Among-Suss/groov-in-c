#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

// sbuf package based on linked list

// linked list node structure
struct linked_node_t {
  void *value;
  int len;
  struct linked_node_t *next;
  struct linked_node_t *prev;
};

// sbuf object
struct sbuf_t {
  struct linked_node_t *front;
  struct linked_node_t *back;
  sem_t items;
  sem_t mutex;
  int size;
};

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

/* initialize sbuf package
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

/* initialize sbuf package
 *
 *  Parameters:
 *      sp      = pointer to sbuf object already allocated by user
 *      value   = pointer to a memory location containing
 *                  some value that we want to insert to
 *                  sbuf
 *      len     = length of value
 */
void sbuf_insert_front_value(struct sbuf_t *sp, void *value, int len);

/* initialize sbuf package
 *
 *  Parameters:
 *      sp      = pointer to sbuf object already allocated by user
 *      retval  = pointer to a buffer which will hold the value
 *                to of the last node to be returned to the caller.
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