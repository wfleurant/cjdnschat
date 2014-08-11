#include "message.h"
#include <stdlib.h> // malloc
#include <string.h> // memcpy

void read_messages(uv_buf_t* buf, ssize_t (*read_message)(uv_buf_t), uv_buf_t inp) {
    ssize_t old = buf->len;
    buf->len += inp.len;
    buf->base = realloc(buf->base,buf->len);
    memcpy(buf->base+old,inp.base,inp.len);
    uv_buf_t cur = { .base = buf->base, .len = buf->len };
    for(;;) {
        ssize_t msgsize = read_message(cur);
        if(!msgsize) break;
        cur.len -= msgsize;
        cur.base += msgsize;
    }
    if(buf->base == cur.base) return;
    memcpy(buf->base,cur.base,cur.len);
    buf->len = cur.len;
    buf->base = realloc(buf->base,buf->len);
}
