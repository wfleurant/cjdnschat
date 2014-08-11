#include "message.h"
#include "check.h"
#include "session.h"
#include "new.h"

#include <string.h> // memcpy
#include <stdio.h>
#include <assert.h>
#include <signal.h>

inline bool max(size_t a, size_t b) {
    return a > b ? a : b;
}

static 
void alloc(uv_handle_t* handle,
        size_t suggested_size,
        uv_buf_t* buf) {
    struct session* sess = (struct session*)handle->data;
    if(sess->buf.base) {
        buf->base = sess->buf.base;
        buf->len = sess->buf.len;
    } else {
        buf->len = sess->buf.len = max(suggested_size,0x100);
        buf->base = sess->buf.base = malloc(sess->buf.len);
    }
}

static void process_message(struct session* sess, uv_buf_t message) {
    switch(*message.base) {
        case MESSAGE:
            message.base[message.len+1] = '\0';
            printf("%s: %s\n",sess->ident, message.base+1);
            break;
/*        case STATUS:
            cur[size] = '\0';
            printf("%s status %s\n",sess->ident, cur);
            break; */
        default:
            printf("Unknown message %d\n",*message.base);
            break;
    };
}

static 
void on_read(uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf) {
    if(nread == UV_EOF) return;
    if(nread == 0) return;
    CHECK(nread);
    struct session* sess = (struct session*)stream->data;

    ssize_t read_message(uv_buf_t buf) {
        if(buf.len < 2) return 0;
        uint16_t msgsize = ntohs(*((uint16_t*)buf.base));
        if(msgsize == 0) return 2;

        if(msgsize + 2 <= buf.len) {
            uv_buf_t message = { .base = buf.base + 2, .len = msgsize };
            process_message(sess, message);
            return msgsize + 2;
        }
        return 0;
    }
    
    uv_buf_t read = { .base = buf->base, .len = nread };
    read_messages(&sess->ringbuf, read_message, read);
}

static void on_connect(uv_connect_t* req, int status) {
    uv_stream_t* outgoing = req->handle;
    uv_loop_t* loop = req->data;

    struct sockaddr_in6 addr = {};
    int len = sizeof(struct sockaddr_in6);
    CHECK(uv_tcp_getpeername((uv_tcp_t*)outgoing,(struct sockaddr*)&addr,&len));

    struct session* sess = session_new(&addr);
    if(sess->refused) {
        printf("add %s to your whitelist\n",sess->ident);
        free(sess);
        return;
    }
    outgoing->data = sess;
    terminal_setup(loop, outgoing);
    uv_read_start(outgoing,alloc,on_read);
}

static void on_connection(uv_stream_t *server, int status) {
    uv_tcp_t* incoming = NEW(uv_tcp_t);
    uv_loop_t* loop = server->data;
    uv_tcp_init(loop,incoming);
    CHECK(uv_accept(server,(uv_stream_t*)incoming));

    struct sockaddr_in6 addr;
    int len = sizeof(struct sockaddr_in6);
    uv_tcp_getpeername(incoming,(struct sockaddr*)&addr,&len);
    assert(len==sizeof(struct sockaddr_in6));

    struct session* sess = session_new(&addr);
    if(sess->refused) {
        printf("session refused from %s\n",sess->ident);
        free(sess);
        free(incoming);
        return;
    }

    incoming->data = sess;

    terminal_setup(loop,incoming);
    uv_read_start((uv_stream_t*)incoming,alloc,on_read);
}


int main(int argc, char**argv) {
    signal(SIGPIPE,SIG_IGN);
    uv_loop_t* loop = uv_default_loop();
    session_setup();
    if(argc == 3) {
        struct sockaddr_in6 guy;
        CHECK(uv_ip6_addr(argv[1],atoi(argv[2]),&guy));

        uv_connect_t req;
        req.data = loop;
        uv_tcp_t handle;
        uv_tcp_init(loop, &handle);
        uv_tcp_connect(&req, &handle, (struct sockaddr*)&guy, on_connect);
        puts("Connecting...");
        return uv_run(loop,UV_RUN_DEFAULT);
    }

    int port = 55555;
    if(getenv("port")) port = atoi(getenv("port"));
    uv_interface_address_t* ifa = NULL;
    int naddr = 0;
    CHECK(uv_interface_addresses(&ifa,&naddr));
    int i;
    for(i=0;i<naddr;++i) {
        if(ifa[i].address.address4.sin_family != PF_INET6) continue;
        if(ifa[i].address.address6.sin6_addr.s6_addr[0] != 0xfc) continue;

        uv_tcp_t handle;
        handle.data = loop;
        uv_tcp_init(loop,&handle);

        ifa[i].address.address6.sin6_port = htons(port);

        session_setup();


        char dst[INET6_ADDRSTRLEN];
        CHECK(uv_ip6_name(&ifa[i].address.address6,dst,INET6_ADDRSTRLEN));
        printf("Tell your friend to run [ %s %s %d ]! Waiting for them to connect...\n",
                argv[0],
                dst,
                ntohs(ifa[i].address.address6.sin6_port));
        uv_tcp_bind(&handle, (struct sockaddr*)&ifa[i].address.address6, 0);
        uv_free_interface_addresses(ifa,naddr);
        uv_listen((uv_stream_t*)&handle, 128, on_connection);

        return uv_run(loop,UV_RUN_DEFAULT);
    }
    puts("No interfaces found.");
    exit(3);
}
