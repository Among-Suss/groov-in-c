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

int sbuf_remove_value(struct sbuf_t *sp, void *value, int len) {
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


  return found;
}

void sbuf_insert_front_value(struct sbuf_t *sp, void *value, int len) {
  struct linked_node_t *newnode = malloc(sizeof(struct linked_node_t));
  newnode->value = malloc(sizeof(char) * len);
  newnode->len = len;
  memcpy(newnode->value, value, len);
  
  
  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  newnode->next = sp->front->next;
  newnode->prev = sp->front;
  sp->front->next = newnode;
  newnode->next->prev = newnode;

  sp->size++;

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
  if (sem_post(&(sp->items)) < 0) {
    printf("semaphor ERror\n");
  }
}

void sbuf_insert_value_position_from_front(struct sbuf_t *sp, void *value, int len, int position) {
  if(position > sp->size){
    return;
  }

  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  struct linked_node_t *newnode = malloc(sizeof(struct linked_node_t));
  newnode->value = malloc(sizeof(char) * len);
  newnode->len = len;
  memcpy(newnode->value, value, len);
  

  struct linked_node_t *my_front = sp->front;
  while(position > 0){
    my_front = my_front->next;
    position--;
  }

  newnode->next = my_front->next;
  newnode->prev = my_front;
  my_front->next = newnode;
  newnode->next->prev = newnode;

  sp->size++;

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
  if (sem_post(&(sp->items)) < 0) {
    printf("semaphor ERror\n");
  }
}

void sbuf_insert_value_position_from_back(struct sbuf_t *sp, void *value, int len, int position) {
  if(position > sp->size){
    return;
  }


  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }


  struct linked_node_t *newnode = malloc(sizeof(struct linked_node_t));
  newnode->value = malloc(sizeof(char) * len);
  newnode->len = len;
  memcpy(newnode->value, value, len);
  

  struct linked_node_t *my_back = sp->back;
  while(position > 0){
    my_back = my_back->prev;
    position--;
  }

  newnode->next = my_back;
  newnode->prev = my_back->prev;
  my_back->prev = newnode;
  newnode->prev->next = newnode;

  sp->size++;

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
  if (sem_post(&(sp->items)) < 0) {
    printf("semaphor ERror\n");
  }
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


  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  return retval;
}


void *sbuf_remove_position_from_end(struct sbuf_t *sp, int position, void *retval, int len) {
  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  
  struct linked_node_t *retnode = 0;
  if (sp->size > position) {
    retnode = sp->back->prev;
    while(position > 0){
      retnode = retnode->prev;

      position--;
    }

    retnode->next->prev = retnode->prev;
    retnode->prev->next = retnode->next;

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


  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  return retval;
}


void *sbuf_remove_front_value(struct sbuf_t *sp, void *retval, int len,
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

  
  struct linked_node_t *retnode = 0;
  if (sp->size > 0) {
    retnode = sp->front->next;

    sp->front->next = retnode->next;
    retnode->next->prev = sp->front;

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


  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  return retval;
}

void *sbuf_peek_end_value_copy(struct sbuf_t *sp, void *retval, int len,
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
      }else{
        printf("semaphor ERror\n");
      }
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

void sbuf_iterate(struct sbuf_t *sp, sbuf_iterate_callback_f callback, void *state, int start_pos, int end_pos) {
  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  int pos = 0;
  linked_node_t *node = sp->back->prev;
  while(node != sp->front && pos <= end_pos){
    if(pos >= start_pos){
      callback(node->value, node->len, state, pos, start_pos, end_pos);
    }
    pos++;
    node = node->prev;
  }


  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
}






void *sbuf_peek_end_value_direct(struct sbuf_t *sp, int *returned_len, int lockitem)
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
      }else{
        printf("semaphor ERror\n");
      }
    }
  }

  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }


  struct linked_node_t *retnode = 0;
  if (sp->size > 0) {
    retnode = sp->back->prev;
  }


  if(return_item){
    if (sem_post(&(sp->items)) < 0) {
      printf("semaphor ERror\n");
    }
  }

  if(retnode && returned_len){
    *returned_len = retnode->len;
  }
  return retnode ? retnode->value : NULL;
}

void sbuf_stop_peeking(struct sbuf_t *sp){
  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
}


void sbuf_shuffle_random(struct sbuf_t *sp){
  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  int list_size = sp->size;
  if(list_size < 3){
    goto CLEANUP_SHUFFLE_SBUF;
  }

  #define MOVE_MODULO 32
  #define MOVE_OFFSET 7
  #define NUM_PASSES 3
  linked_node_t *i_node = sp->front->next;
  linked_node_t *j_node = sp->back->prev->prev;
  int delta = 0;
  srand(time(NULL));
  for (int i = 0; i < list_size * NUM_PASSES; i++){
    delta = (rand() % MOVE_MODULO) + MOVE_OFFSET;

    for (int j = 0; j < delta; j++){
      j_node = j_node->prev;
      if(j_node == sp->front){
        j_node = sp->back->prev->prev;
      }
    }

    //swap
    void *tmp_val = i_node->value;
    int tmp_len = i_node->len;
    i_node->value = j_node->value;
    i_node->len = j_node->len;
    j_node->value = tmp_val;
    j_node->len = tmp_len;

    i_node = i_node->next;
    if(i_node == sp->back->prev){
      i_node = sp->front->next;
    }
  }


  CLEANUP_SHUFFLE_SBUF:
  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
}

void sbuf_clear(struct sbuf_t *sp) {
  if (sem_wait(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }

  struct linked_node_t *lp = sp->front->next;
  struct linked_node_t *lp_next = 0;
  while (lp != sp->back) {
    lp_next = lp->next;
    free(lp->value);
    free(lp);
    lp = lp_next;
  }

  sp->front->next = sp->back;
  sp->back->prev = sp->front;

  sp->size = 0;
  sem_init(&(sp->items), 0, 0);

  if (sem_post(&(sp->mutex)) < 0) {
    printf("semaphor ERror\n");
  }
}