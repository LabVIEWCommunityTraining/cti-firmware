#ifndef common_h_
#define common_h_

#define _CTI_STR(x) #x
#define CTI_STR(x) _CTI_STR(x)

#define _CTI_CONCAT(x, y) x##y
#define CTI_CONCAT(x, y) _CTI_CONCAT(x, y)

#endif //common_h_