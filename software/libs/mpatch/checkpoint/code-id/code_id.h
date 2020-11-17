#ifndef CODE_ID_H_
#define CODE_ID_H_

#include <stdint.h>

typedef uint32_t code_id_t;

extern volatile code_id_t code_id;
code_id_t code_id_is_new(void);
code_id_t code_id_update(void);

static inline void code_id_clear(void) {
    code_id = 0;
}

#endif /* CODE_ID_H_ */
