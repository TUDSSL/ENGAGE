#ifndef CHECKPOINT_SELECTOR_H_
#define CHECKPOINT_SELECTOR_H_

#include "checkpoint_logical_clock.h"

#define checkpoint_get_active_idx()     (lclock%2)
#define checkpoint_get_restore_idx()    ((lclock+1)%2)

#endif /* CHECKPOINT_SELECTOR_H_ */
