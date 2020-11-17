# MPatch Checkpoint Library

ENGAGE's checkpoint mechanism, called MPatch, consists of two distinct parts. The checkpoint *core* located in [checkpoint/](checkpoint/) and MPatch located in [mpatch/](mpatch/), which acts as a plugin to the checkpoint core and is responsible for checkpointing and restoring the emulated game memory using incremental checkpoint patches.

## Checkpoint Core

The checkpoint core can operate without MPatch, resulting in the _naive_ version as described in the [paper describing ENGAGE platform](https://github.com/TUDSSL/ENGAGE/tree/master/README.md#How-to-Cite-this-Work). To achieve this, all components, including the emulated game memory, must be checkpointed using the core only. Note that this means that during a checkpoint *all* memory is copied to non-volatile memory. When MPatch is enabled, all but the emulated game memory is still checkpointed using the checkpoint core.

The checkpoint core consists of a simple API described below.

```
/* Invalidate all checkpoints */
void checkpoint_restore_invalidate()

/* Check if there is a restore availible */
bool checkpoint_restore_available()

/* Setup the checkpoint machanism (each boot) */
void checkpoint_setup()

/* Perform one-time setup (first boot only) */
void checkpoint_onetime_setup()

/* Create a checkpoint, it is also the point at which a restore continues
Returns 0 if it is a checkpoint
Returns 1 if it is a restore */
int checkpoint()

/* Restore the most recent checkpoint
Will continue in the code at the checkpoint() call with return value 1*/
void checkpoint_restore()
```

The checkpoint core has to be configured as is demonstrated in [`example_checkpoint_content.h`](/software/libs/mpatch/checkpoint/example_checkpoint_content.h).
The defines configured here will be used in the checkpoint API and decide what happens during the respective operations.

The available hooks in [`example_checkpoint_content.h`](/software/libs/mpatch/checkpoint/example_checkpoint_content.h) are:

- `CHECKPOINT_SETUP_CONTENT`: Includes all actions to be taken only once, i.e. during the first time boot.
- `CHECKPOINT_CONTENT`: Includes all actions to be taken during a checkpoint.
- `CHECKPOINT_RESTORE_CONTENT`: Includes all actions to be taken during a restore.
- `POST_CHECKPOINT_CONTENT`: Includes all actions to be taken after a successful checkpoint is completed.
- `POST_CHECKPOINT_AND_RESTORE_CONTENT`: Includes all actions to be taken after a successful restore **and** a successful checkpoint.

For an example of how to use the API, see:

1. [`checkpoint_test.c`](/software/apps/checkpoint/checkpoint_test.c) with [`checkpoint/config/checkpoint_content.h`](/software/apps/checkpoint/config/checkpoint_content.h) configuration file, and
2. [`main_emulator.c`](/software/apps/emulator/main_emulator.c) with [`emulator/config/checkpoint_content.h`](/software/apps/emulator/config/checkpoint_content.h) configuration file.

## MPatch

MPatch hooks into the core checkpoint operation by configuring [`software/apps/emulator/config/checkpoint_content.h`](/software/apps/emulator/config/checkpoint_content.h).

Essentially MPatch checkpoints consist of three calls configured in this [`checkpoint_content.h`](/software/apps/emulator/config/checkpoint_content.h), as shown below.

```
/* Create patches */
checkpoint_memtracker();

/* Stage patches */
checkpoint_mpatch();

/* Commit patches */
post_checkpoint_mpatch(); 
```

MPatch requires address ranges to create patches. This is done in [`checkpoint_memtracker()`](/software/libs/memtracker/checkpoint_memtracker.c) function. Patches can be added and staged using the following call.

```
mpatch_pending_patch_t pp;

mpatch_new_region(&pp, (mpatch_addr_t)mpatchregionstart, (mpatch_addr_t)mpatchregionend, MPATCH_STANDALONE);

mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);
```

### Patch Allocation

To allocate patches MPatch uses a block allocator that has been modified to work under intermittent power. A [separate document](bliss_allocator/README.md) describe its operation, structure and use.

## Building MPatch Unit Tests

The directory [test/unit](test/unit) contains unit tests for many of the MPatch features. To build and execute the tests execute the following (from the directory of this README).

```
$ mkdir build
$ cd build
$ cmake ../
$ make
$ make tests-run
```

The results of the tests will we shown in the terminal.
