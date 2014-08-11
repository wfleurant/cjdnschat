#define MESSAGE 0
#include <uv.h>
void read_messages(uv_buf_t* buf, ssize_t (*read_message)(uv_buf_t), uv_buf_t inp);
