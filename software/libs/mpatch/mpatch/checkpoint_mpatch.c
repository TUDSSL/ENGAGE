#include <stdlib.h>
#include "mpatch.h"

#include "checkpoint_mpatch.h"

size_t checkpoint_mpatch(void)
{
    mpatch_stage();
    mpatch_core_checkpoint(false);
    return 0;
}

size_t restore_mpatch(void)
{
    mpatch_core_restore();
    mpatch_recover();
    mpatch_apply_all(false);
    return 0;
}

size_t setup_mpatch(void)
{
    size_t blocks = mpatch_init();
    return blocks;
}
