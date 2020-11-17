//*****************************************************************************
//
//! @file appl_ibeacon.h
//!
//! @brief Application Header File for iBeacon example.
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

/**
 *  \file appl_ibeacon.h
 *
 *  Application Header File for iBeacon example.
 */

/*
 *  Copyright (C) 2013. Mindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_APPL_HRS_
#define _H_APPL_HRS_

/* ----------------------------------------- Header File Inclusion */
#include "BT_api.h"
#include "BT_gatt_db_api.h"
#include "gatt_db.h"
#include "appl.h"

/* ----------------------------------------- Data Types/ Structures */

/* --------------------------------------------- Functions */
void appl_ibeacon_init(void);

/* Profile handling */
#define APPL_PROFILE_INIT(...) appl_ibeacon_init()
#define APPL_PROFILE_CONNECT(x)
#define APPL_SEND_MEASUREMENT(x)
#define APPL_PROFILE_DISCONNECT_HANDLER(x)
#define GATT_DB_PROFILE_HANDLER
#define APPL_PROFILE_HVN_NTF_COMPLETE_HANDLER(x, y, z)
#define APPL_PROFILE_HVN_IND_COMPLETE_HANDLER(x, y, z)
#define APPL_PROFILE_MTU_UPDT_COMPLETE_HANDLER(x, y)

#define APPL_USE_IDLE_TIMER
#define APPL_IDLE_TIMEOUT           30
#endif /* _H_APPL_HRS_ */


