#include "sbuf.h"
#include "sbuf.structs.h"

// sbuf package based on linked list

void sbuf_init(struct sbuf_t *sp) {
  sp->size = 0;
  sp->front = malloc(sizeof(struct linked_node_t));
  sp->back = malloc(sizeof(struct linked_node_t));

  sp->front->next = sp->back;
  sp->front->prev = 0;
  sp->back->prev = sp->front;
  sp->back->next = 0;

  sp->front->value = malloc(1);
  sp->back->value = malloc(1);

  sp->front->len = 1;
  sp->back->len = 1;
  if (sem_init(&(sp->items), 0, 0) < 0) {
    printf("semaphor ERror\n");
  }
  if (sem_init(&(sp->mutex), 0, 1) < 0) {
    printf("semaphor ERror\n");
  }

  if (sem_init(&(sp->removal_lock), 0, 1) < 0) {
    printf("semaphor ERror\n");
  }
}

void sbuf_deinit(struct sbuf_t *sp) {
  struct linked_node_t *lp = sp->front;
  struct linked_node_t *lp_next = 0;
  while (lp) {
    lp_next = lp->next;
    free(lp->value);
    free(lp);
    lp = lp_next;
  }
}

void DONOTUSE_sbuf_insert_front(struct sbuf_t *sp, struct linked_node_t *node) {
  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  node->next = sp->front->next;
  node->prev = sp->front;
  sp->front->next = node;
  node->next->prev = node;

  sp->size++;

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
  if (sem_post(&(sp->items)) < 0) {
    printf("semaphor ERror\n");
  }
}

int sbuf_remove_value(struct sbuf_t *sp, void *value, int len) {
  if (sem_wait(&(sp->removal_lock)) < 0) {
    printf("semaphor ERror\n");
  }

  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
  int found = 0;

  struct linked_node_t *cp = sp->front->next;
  while (cp != sp->back) {
    if ((cp->len == len) && (memcmp(cp->value, value, len) == 0)) {
      found = 1;
      break;
    }
    cp = cp->next;
  }

  if (found) {
    cp->prev->next = cp->next;
    cp->next->prev = cp->prev;

    free(cp->value);
    free(cp);

    sp->size--;

    if (sem_trywait(&(sp->items)) < 0) {
      printf("semaphor ERror\n");
    }
  }

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  if (sem_post(&(sp->removal_lock)) < 0) {
    printf("semaphor ERror\n");
  }

  return found;
}

void sbuf_insert_front_value(struct sbuf_t *sp, void *value, int len) {
  struct linked_node_t *newnode = malloc(sizeof(struct linked_node_t));
  newnode->value = malloc(sizeof(char) * len);
  newnode->len = len;
  memcpy(newnode->value, value, len);
  DONOTUSE_sbuf_insert_front(sp, newnode);
}

void *sbuf_remove_end_value(struct sbuf_t *sp, void *retval, int len,
                           int lockitem) {
  
  if (lockitem) {
    if (sem_wait(&(sp->items)) < 0) {
      printf("semaphor ERror\n");
    }
  }else{
    if (sem_trywait(&(sp->items)) < 0) {
      printf("semaphor ERror\n");
    }
  }

  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  if (sem_wait(&(sp->removal_lock)) < 0) {
    printf("semaphor ERror\n");
  }

  
  struct linked_node_t *retnode = 0;
  if (sp->size > 0) {
    retnode = sp->back->prev;

    sp->back->prev = retnode->prev;
    retnode->prev->next = sp->back;

    sp->size--;
  }


  if (retnode) {
    if(retval)
      memcpy(retval, retnode->value, MIN(len, retnode->len));
    free(retnode->value);
    free(retnode);
  } else {
    retval = 0;
  }

  if (sem_post(&(sp->removal_lock)) < 0) {
    printf("semaphor ERror\n");
  }

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  return retval;
}

void *sbuf_peek_end_value(struct sbuf_t *sp, void *retval, int len,
                           int lockitem)
{
  int return_item = 1;
  if (lockitem) {
    if (sem_wait(&(sp->items)) < 0) {
      printf("semaphor ERror\n");
    }
  }else{
    if (sem_trywait(&(sp->items)) < 0) {
      if(errno == EAGAIN){
        return_item = 0;
      }
      printf("semaphor ERror\n");
    }
  }

  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
  
  struct linked_node_t *retnode = 0;
  if (sp->size > 0) {
    retnode = sp->back->prev;
  }


  if (retnode) {
    if(retval)
      memcpy(retval, retnode->value, MIN(len, retnode->len));
  } else {
    retval = 0;
  }

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  if(return_item){
    if (sem_post(&(sp->items)) < 0) {
      printf("semaphor ERror\n");
    }
  }

  return retval;
}

void sbuf_removal_lock(struct sbuf_t *sp){
  if (sem_wait(&(sp->removal_lock)) < 0) {
    printf("semaphor ERror\n");
  }
}

void sbuf_removal_unlock(struct sbuf_t *sp){
  if (sem_post(&(sp->removal_lock)) < 0) {
    printf("semaphor ERror\n");
  }
}