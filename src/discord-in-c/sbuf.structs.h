#define _GNU_SOURCE
#include <semaphore.h>

//linked list node structure
typedef struct linked_node_t {
  void *value;
  int len;
  struct linked_node_t *next;
  struct linked_node_t *prev;
}linked_node_t;


//sbuf object
typedef struct sbuf_t {
  struct linked_node_t *front;
  struct linked_node_t *back;
  sem_t items;
  sem_t mutex;
  int size;
}sbuf_t;