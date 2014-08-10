#include "terminal.h"

struct sockaddr_in6 guy;
struct uv_tcp_t handle;
bool hasguy = false;

static void process_message(const char* message) {
    switch(*message) {
        case '/':
            if(0==strncmp(message+1,"connect ")) {
                char* addr = message+1+sizeof("connect ");
                if(strlen(addr) != INET6_ADDRSTRLEN) {
                    puts("That's not an IPv6 address");
                    return;
                }
                if(addr[0] != 'f' || addr[1] != 'c') {
                    puts("That's not a cjdns address");
                    return;
                }
                if(hasguy) {
                    uv_close(handle,NULL);
                    uv_connect_t* req = malloc(sizeof(uv_connect_t));
                    uv
                hasguy = true;
            break;
        case 0:
            return;


uv_tty_t handle;

uv_buf_t ringbuf = {};

static ssize_t read_message(char* cur, ssize_t len) {
    void* nl = memchr(cur,"\n",len);
    if(nl == NULL) return 0;
    *nl = '\0';
    process_message(cur);
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


void terminal_setup(uv_loop_t* loop) {
    uv_tty_init(loop,&handle,0,1);
    uv_read_start(&handle,alloc,on_stdin);
}
