typedef struct callback_t {
  callback_func_t callback;
  SSL *ssl;
  void *state;
  sem_t exiter;
} callback_t;