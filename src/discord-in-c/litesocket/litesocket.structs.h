typedef struct callback_t{
    callback_func_t callback;
    SSL *ssl;
    void *state;
}callback_t;