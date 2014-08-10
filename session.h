#include <uv.h>
#include <stdbool.h>

struct session {
    uv_buf_t buf;
    uv_buf_t ringbuf;
    const char* ident;
    bool refused;
};

struct session* session_new(struct sockaddr_in6* addr);

void session_free(struct session**);
