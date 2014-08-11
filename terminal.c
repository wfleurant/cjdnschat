#include "message.h"
#include "check.h"
#include "terminal.h"
#include "new.h"

#include <string.h> // memchr
#include <stdio.h>

uv_stream_t* g_peer = NULL;

static void on_wrote(uv_write_t* req, int status) {
    if(status == -EPIPE) {
        puts("Peer disconnected.");
    } else {
        CHECK(status);
        puts("Sent.");
    }
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
    uv_buf_t message = { .base = buf.base, .len = nl-buf.base };
    process_message(message);
    return message.len + 1;
}

static void on_stdin(uv_stream_t* stream,
        ssize_t nread,
        const uv_buf_t* buf) {
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


void terminal_setup(uv_loop_t* loop, uv_stream_t* peer) {
    static uv_tty_t handle = {};
    g_peer = peer;
    uv_tty_init(loop,&handle,0,1);
    uv_read_start((uv_stream_t*)&handle,alloc,on_stdin);
}
