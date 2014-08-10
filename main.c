#include "session.h"
#include "new.h"

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

static void process_message(struct session* sess, uint8_t* cur, ssize_t size) {
    switch(*cur) {
        case MESSAGE:
            cur[size] = '\0';
            printf("%s: %s\n",sess->ident, cur);
            break;
        case STATUS:
            cur[size] = '\0';
            printf("%s status %s\n",sess->ident, cur);
            break;
        default:
            printf("Unknown message %d\n",*cur);
            break;
    };
}

static ssize_t read_message(struct session* sess, uint8_t* cur, size_t left) {
    uint16_t msgsize = *((uint16_t*)cur);
    if(msgsize == 0) return 2;

    if(msgsize <= left) {
        process_message(sess, cur,msgsize);
        return msgsize + 2;
    }
    return 0;
}

static 
void on_read(uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf) {
    struct session* sess = (struct session*)handle->data;
    CHECK(nread);
    size_t old = sess->ringbuf.len;
    sess->ringbuf.len = old + buf->len;
    sess->ringbuf.base = realloc(sess->ringbuf.base,sess->ringbuf.len);

    uint8_t* cur = sess->ringbuf.base;
    size_t left = sess->ringbuf.len;

    for(;;) {
        uint8_t* next = read_message(sess, cur, left);
        if(next) {
            left -= (next - cur);
            cur = next;
        } else {
            memcpy(sess->ringbuf.base,cur,left);
            sess->ringbuf.base = realloc(sess->ringbuf.base,left);
            sess->ringbuf.len = left;
        }
    }
}

static void on_connect(uv_connect_t* req, int status) {
    uv_stream_t* outgoing = req->handle;
    uv_loop_t* loop = req->data;
    free(req);

    struct sockaddr_in6 addr;
    int len = sizeof(struct sockaddr_in6);
    uv_tcp_getpeername(outgoing,&addr,len);

    struct session* sess = session_new(&addr);
    if(sess->refused) {
        puts("add %s to your whitelist",sess->ident);
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
    CHECK(uv_accept(server,incoming));

    struct sockaddr_in6 addr;
    int len = sizeof(struct sockaddr_in6);
    uv_tcp_getpeername(incoming,&addr,len);
    assert(len==sizeof(struct sockaddr_in6));

    struct session* sess = session_new(&addr);
    if(sess->refused) {
        puts("session refused from %s",sess->ident);
        free(sess);
        free(incoming);
        return;
    }

    incoming->data = sess;

    terminal_setup(loop,incoming);
    uv_read_start(incoming,alloc,on_read);
}


int main(int argc, char**argv) {
    uv_loop_t* loop = uv_default_loop();
    session_setup();
    if(argc == 3) {
        struct sockaddr_in6 guy;
        assert(inet_pton(PF_INET6,argv[1],&guy));
        int rport = atoi(argv[2]);
        assert(rport);
        guy.sin6_port = htons(rport);

        uv_connect_t req;
        uv_tcp_t handle;
        uv_tcp_init(handle,loop);
        uv_tcp_connect(&req,&handle, &guy, on_connect);
        puts("Connecting...");
        return uv_run(loop,UV_RUN_DEFAULT);
    }

    int port = 55555;
    if(getenv("port")) port = atoi(getenv("port"));
    uv_interface_addr_t* ifa = NULL;
    int naddr = 0
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
        assert(0==inet_ntop(AF_INET6, &ifa[i].address.address6,dst,INET6_ADDRSTRLEN));
        printf("Tell your friend to run %s %s %s! Waiting for them to connect...\n",
                argv[0],dst,ntohs(ifa[i].address.address6.sin6_port));
        uv_tcp_bind(&handle, &ifa[i].address.address6);
        uv_free_interface_addreses(ifa,naddr);
        uv_listen((uv_stream_t*)&stream, 128, on_connection);

        return uv_run(loop,UV_RUN_DEFAULT);
    }
    puts("No interfaces found.");
    exit(3);
}
