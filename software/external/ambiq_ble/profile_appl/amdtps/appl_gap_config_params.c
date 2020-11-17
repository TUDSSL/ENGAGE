//*****************************************************************************
//
//  appl_gap_config_params.c
//! @file
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
 *  \file appl_gap_config_params.c
 *
 *  This file contains GAP Configuration Parameters used by the application.
 */


/* --------------------------------------------- Header File Inclusion */
#include "appl_gap.h"

#ifdef AMDTPS

/* --------------------------------------------- External Global Variables */

/* --------------------------------------------- Exported Global Variables */

/* --------------------------------------------- Static Global Variables */

#if ((defined APPL_GAP_BROADCASTER_SUPPORT) || (defined APPL_GAP_PERIPHERAL_SUPPORT))
/** Advertisement Data Options */
const APPL_GAP_ADV_DATA appl_gap_adv_data[APPL_GAP_MAX_ADV_DATA_OPTIONS] =
{
    /* GAP Advertisement Parameters */
    {
        {
            /**
             *  Flags:
             *      0x01: LE Limited Discoverable Mode
             *      0x02: LE General Discoverable Mode
             *      0x04: BR/EDR Not Supported
             *      0x08: Simultaneous LE and BR/EDR to Same Device
             *            Capable (Controller)
             *      0x10: Simultaneous LE and BR/EDR to Same Device
             *            Capable (Host)
             */
            0x02, 0x01,
            (BT_AD_FLAGS_LE_GENERAL_DISC_MODE | BT_AD_FLAGS_LE_BR_EDR_SUPPORT),

            /**
             *  Service UUID List:
             *      Battery Service (0x180F)
             *      DeviceInformation Service (0x180A)
             *      Heart Rate Service (0x180D)
             */
            0x07, 0x03, 0x0F, 0x18, 0x0A, 0x18, 0x0D, 0x18,

            /**
             *  Shortened Device Name: Mindtree
             */
            0x09, 0x08, 0x4D, 0x69, 0x6E, 0x64, 0x74, 0x72, 0x65, 0x65
        },

        21
    }
};

/* Advertisement parameters options */
const APPL_GAP_ADV_PARAM appl_gap_adv_param[APPL_GAP_MAX_ADV_PARAM_OPTIONS] =
{
    /* 0 - Normal Advertising Params */
    {
        32,

        32,

        7,

        0
    },
    /* 1 - Fast Connection Advertising Params */
    {
        32,

        48,

        7,

        0
    },
    /* 2 - Low Power Advertising Params */
    {
        1600,

        4000,

        7,

        0
    }
};

/* Advertisement Table */
APPL_GAP_ADV_INFO appl_gap_adv_table =
{
    appl_gap_adv_data,

    appl_gap_adv_param,

    APPL_GAP_ADV_IDLE
};
#endif /* APPL_GAP_BROADCASTER || APPL_GAP_PERIPHERAL_SUPPORT */

#if ((defined APPL_GAP_OBSERVER_SUPPORT) || (defined APPL_GAP_CENTRAL_SUPPORT))
/* Scan Parameters Option */
const APPL_GAP_SCAN_PARAM appl_gap_scan_param[APPL_GAP_MAX_SCAN_PARAM_OPTIONS] =
{
    /* Normal Scan Params */
    {
        32,

        7,

        0
    },
    /* Fast Connection Scan Params */
    {
        48,

        7,

        0
    },
    /* Low Power Scan Params */
    {
        4000,

        7,

        0
    }
};

/* Scan Table */
APPL_GAP_SCAN_INFO    appl_gap_scan_table =
{
    appl_gap_scan_param,

    APPL_GAP_SCAN_IDLE
};
#endif /* APPL_GAP_OBSERVER_SUPPORT || APPL_GAP_CENTRAL_SUPPORT */

#ifdef APPL_GAP_CENTRAL_SUPPORT
/* Connection Parameters Options */
const APPL_GAP_CONN_PARAM appl_gap_conn_param[APPL_GAP_MAX_CONN_PARAM_OPTIONS] =
{
    {
        4,

        4,

        0,

        40,

        56,

        0,

        955,

        32,

        32
    }
};

/* GAP Connection Table */
APPL_GAP_CONN_INFO    appl_gap_conn_table =
{
    appl_gap_conn_param,

    APPL_GAP_CONN_IDLE
};
#endif /* APPL_GAP_CENTRAL_SUPPORT */
#endif /* AMDTPS */

