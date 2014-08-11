#include "session.h"
#include "check.h"

#include <assert.h>
#include <stdlib.h> // getenv
#include <netinet/in.h> // in6_addr
#include <stdio.h>

struct in6_addr* goodaddrs = NULL;
ssize_t ngood = 0;

void session_setup(void) {
    const char* s = getenv("whitelist");
    if(s) {
        FILE* inp = fopen(s,"rt");
        if(inp) {
            char buf[INET6_ADDRSTRLEN];
            char nl;
            while(!feof(inp)) {
                if(INET6_ADDRSTRLEN != fread(buf,INET6_ADDRSTRLEN,1,inp))
                    break;
                
                ssize_t i = ngood;
                ngood += 1;
                goodaddrs = realloc(goodaddrs,sizeof(struct in6_addr)*ngood);
                inet_pton(PF_INET6, buf, goodaddrs + i);
                for(;;) {
                    if(1 == fread(&nl,1,1,inp))
                        if(!isspace(nl)) 
                            break;
                }
            }
        }
    }
}

struct session* session_new(struct sockaddr_in6* addr) {
    struct session* self = calloc(1,sizeof(struct session));
    char* dst = malloc(INET6_ADDRSTRLEN + 6);
    CHECK(uv_ip6_name(addr, dst, INET6_ADDRSTRLEN));
    snprintf(dst+INET6_ADDRSTRLEN,6,":%d",ntohs(addr->sin6_port));
    self->ident = dst;

    if(goodaddrs) {
        bool good = false;
        ssize_t i = 0;
        for(i=0;i<ngood;++i) {
            if(0 == memcmp(addr->sin6_addr,goodaddrs + i, sizeof(struct in6_addr))) {
                good = true;
                break;
            }
        }
        if(!good) {
            self->refused = true;
        }
    }
    return self;
}

void session_free(struct session** derp) {
    struct session* doomed = *derp;
    *derp = NULL;
    free(doomed->ident);
    if(doomed->buf.base)
        free(doomed->buf.base);
    if(doomed->ringbuf.base)
        free(doomed->ringbuf.base);
    free(doomed);
}
