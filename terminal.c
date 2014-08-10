#include "terminal.h"
#include "new.h"

struct uv_tcp_t* g_peer = NULL;

static void on_wrote(uv_write_t* req, int status) {
    printf("Sent %d\n",status);
    free(req);
}

static void process_message(uint8_t* message, ssize_t len) {
    switch(*message) {
        case '/':
            puts("Maybe we'll have commands for something some day.");
            break;
        case 0:
            return;
        default:
            uv_write_t* req = NEW(uv_write_t);
            uv_buf_t buf = { .base = message, .len = len };
            uv_write(req,g_peer,&buf,1,on_wrote);
            break;
    };
}

uv_tty_t handle;

uv_buf_t ringbuf = {};

static ssize_t read_message(char* cur, ssize_t len) {
    void* nl = memchr(cur,"\n",len);
    if(nl == NULL) return 0;
    process_message(cur,nl-cur);
}

static void on_stdin(uv_stream_t* stream,
        ssize_t nread,
        const uv_buf_t* buf) {
    if(nread == 0) {
        read_message(ringbuf.base, ringbuf.len);
    }
    CHECK(nread);

    ssize_t old = ringbuf.len;
    ringbuf.len += buf->len;
    ringbuf.base = realloc(ringbuf.base,ringbuf.len);
    memcpy(ringbuf.base+old,buf->base,buf->len);
    ssize_t left = ringbuf.len;
    uint8_t* cur = ringbuf.base;
    for(;;) {
        ssize_t msgsize = read_message(cur,left);
        if(!msgsize) break;
        left -= msgsize;
        cur += msgsize;
    }
    if(ringbuf.base == cur) return;
    memcpy(ringbuf.base,cur,left);
    ringbuf.base = realloc(ringbuf.base,left);
    ringbuf.len = left;
}


void terminal_setup(uv_loop_t* loop, uv_stream_t* peer) {
    g_peer = peer;
    uv_tty_init(loop,&handle,0,1);
    uv_read_start(&handle,alloc,on_stdin);
}
