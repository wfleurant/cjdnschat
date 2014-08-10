#include <string.h>

#define NEW(a) malloc(sizeof(a))
#define NEW0(a) calloc(1,sizeof(a))
#define MAKENEW(a,b) a* b = NEW(a)
#define MAKENEW0(a,b) a* b = NEW0(a)
