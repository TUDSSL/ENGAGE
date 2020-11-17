#include <stdint.h>
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "barrier.h"
#include "code-id/code_id.h"
#include "checkpoint_setup_restore.h"
#include "checkpoint.h"

#include "checkpoint_content.h"

#define PRINT_LOG   (0)
#define PRINT_DEBUG (0)

#if PRINT_DEBUG
#define DEBUG_PRINT(...)    do {printf("[checkpoint-dbg] "); printf(__VA_ARGS__);} while (0)
#else
#define DEBUG_PRINT(...)
#endif

#if PRINT_LOG
#define LOG_PRINT(...)      do {printf("[checkpoint] "); printf(__VA_ARGS__);} while (0)
#else
#define LOG_PRINT(...)
#endif


static inline void checkpoint_commit(void)
{
    barrier;
    lclock += 1;
    LOG_PRINT("Checkpoint committed, new lclock: %d\n", (int)lclock);
}


extern inline bool checkpoint_restore_available(void)
{
    /* Returns the potential new code-id if they are not the same */
    code_id_t new_code_id = code_id_is_new();
    if (new_code_id) {
        return false;
    }
    return true;
}

void checkpoint_restore_invalidate(void)
{
    code_id_clear();
}

void checkpoint_restore_set_availible(void)
{
    code_id_update();
}

// This function must NOT return
__attribute__((noinline)) void restore_process(void) {

    CHECKPOINT_RESTORE_CONTENT();

    /* SHOULD NEVER BE REACHED */
    while (1)
        ;
}

/*
 * Because we restore the stack we can't operate on the normal stack
 * So we need to switch the stack to a dedicated one and restore
 * the stack afterwards
 *
 * NB. This function can NOT use the stack or funny things will happen
 */

__attribute__((noinline)) static void _restore(void) {
    /* Change the restore environment so we don't corrupt the stack while
     * restoring
     */
    checkpoint_setup_restore();

    //// WORKING ON THE SAFE STACK
    restore_process();

    /* SHOULD NEVER BE REACHED */
    while (1)
        ;
}

void checkpoint_restore(void) {
    if (checkpoint_restore_available()) {
        _restore();
    }
}

__attribute__((noinline)) int checkpoint(void) {

    CHECKPOINT_CONTENT();

    // NB: restore point
    if (checkpoint_restored() == 0) {
        /* Normal operation */
        // Commit checkpoint
        checkpoint_commit();

        POST_CHECKPOINT_CONTENT();
    }

    POST_CHECKPOINT_AND_RESTORE_CONTENT();

    return checkpoint_restored();
}

int checkpoint_onetime_setup(void) {
    lclock = 0;

    CHECKPOINT_SETUP_CONTENT();

    return 0;
}

