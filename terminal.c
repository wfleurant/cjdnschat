#include "message.h"
#include "check.h"
#include "terminal.h"
#include "new.h"
#include "session.h"

#include <string.h> // memchr
#include <stdio.h>
#include <stdbool.h>

uv_stream_t* g_peer = NULL;
bool g_master = false;

static void on_wrote(uv_write_t* req, int status) {
    if(status == -EPIPE) {
        puts("Peer disconnected.");
        if(g_master == false) exit(0);
    
        session_free((struct session**) &g_peer->data);
        uv_read_stop(g_peer);
        uv_close((uv_handle_t*)g_peer,NULL);
        g_peer = NULL;
    } else {
        CHECK(status);
        printf("you: %s\n",req->data);
    }
    free(req->data);
    free(req);
}

static void process_message(uv_buf_t message) {
    switch(*message.base) {
        case '/':
            puts("Maybe we'll have commands for something some day.");
            break;
        case 0:
            return;
        default: 
            {
            uv_write_t* req = NEW(uv_write_t);
            req->data = strndup((const char*)message.base,message.len);
            uint8_t prefixbuf[3] = "";
            prefixbuf[2] = MESSAGE;
            *((uint16_t*)prefixbuf) = htons(message.len+1);
            uv_buf_t bufs[2] = {
                { .base = prefixbuf, .len = 3 },
                message
            };
            uv_write(req,g_peer,bufs,2,on_wrote);
            }
            break;
    };
}


uv_buf_t ringbuf = {};

static ssize_t read_message(uv_buf_t buf) {
    char* nl = memchr(buf.base,'\n',buf.len);
    if(nl == NULL) return 0;
    if(nl==buf.base) return 1;
    uv_buf_t message = { .base = buf.base, .len = nl-buf.base };
    process_message(message);
    return message.len + 1;
}

static void on_stdin(uv_stream_t* stream,
        ssize_t nread,
        const uv_buf_t* buf) {
    if(!g_peer) {
        uv_read_stop(stream);
        return;
    }
    if(nread == 0) {
        read_message(ringbuf);
    }
    CHECK(nread);

    uv_buf_t read = { .base = buf->base, .len = nread };

    read_messages(&ringbuf,read_message,read);
}

uint8_t derp[0x100];
uv_buf_t g_buf = { .base = derp, .len = 0x100 };

static 
void alloc(uv_handle_t* handle,
        size_t suggested_size,
        uv_buf_t* buf) {
    buf->base = g_buf.base;
    buf->len = g_buf.len;
}


void terminal_setup(uv_loop_t* loop, uv_stream_t* peer, bool master) {
    static uv_tty_t handle = {};
    g_peer = peer;
    g_master = master;
    uv_tty_init(loop,&handle,0,1);
    uv_read_start((uv_stream_t*)&handle,alloc,on_stdin);
    struct session* sess = peer->data;
    printf("Connection with %s ready!\n",sess->ident);
    if(master == false) {
        struct sockaddr_in6 addr;
        char dst[INET6_ADDRSTRLEN];
        int namelen = sizeof(struct sockaddr_in6);
        CHECK(uv_tcp_getsockname((uv_tcp_t*)peer, (struct sockaddr*) &addr, &namelen));
        CHECK(uv_ip6_name(&addr,dst,INET6_ADDRSTRLEN));
        printf("You have address %s\n",dst);
    }
}
