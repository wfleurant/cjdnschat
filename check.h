#define CHECK(a) if(a < 0) { printf("Check failed %s:%d %s\n",__FILE__,__LINE__,uv_strerror(a)); exit(a); }
