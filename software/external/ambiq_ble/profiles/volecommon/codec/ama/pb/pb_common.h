//*****************************************************************************
//
//  bp_common.h
//! @file
//!
//! @brief Auto-generated (see below).
//!
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2020, Ambiq Micro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision 2.4.2 of the AmbiqSuite Development Package.
//
//*****************************************************************************
/* pb_common.h: Common support functions for pb_encode.c and pb_decode.c.
 * These functions are rarely needed by applications directly.
 */

#ifndef PB_COMMON_H_INCLUDED
#define PB_COMMON_H_INCLUDED

#include "pb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Iterator for pb_field_t list */
struct pb_field_iter_s
{
    const pb_field_t *start;       /* Start of the pb_field_t array */
    const pb_field_t *pos;         /* Current position of the iterator */
    unsigned required_field_index; /* Zero-based index that counts only the required fields */
    void *dest_struct;             /* Pointer to start of the structure */
    void *pData;                   /* Pointer to current field value */
    void *pSize;                   /* Pointer to count/has field */
};
typedef struct pb_field_iter_s pb_field_iter_t;

/* Initialize the field iterator structure to beginning.
 * Returns false if the message type is empty. */
bool pb_field_iter_begin(pb_field_iter_t *iter, const pb_field_t *fields, void *dest_struct);

/* Advance the iterator to the next field.
 * Returns false when the iterator wraps back to the first field. */
bool pb_field_iter_next(pb_field_iter_t *iter);

/* Advance the iterator until it points at a field with the given tag.
 * Returns false if no such field exists. */
bool pb_field_iter_find(pb_field_iter_t *iter, uint32_t tag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

