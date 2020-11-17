//*****************************************************************************
//
//! @file gatt_db.h
//!
//! @brief GATT Databse.
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
 *  \file gatt_db.h
 */

/*
 *  Copyright (C) 2013. Mindtree Ltd.
 *  All rights reserved.
 */

#ifndef _H_GATT_DB_
#define _H_GATT_DB_

/**
 * addgroup gatt_db_module
 */

/**
 * defgroup gatt_db_tuneable_param Tuneable Parameters
 * {
 * This section defines the Tuneable Constants of Data Base Module.
 */

/** Number of Characteristics in the data base */
#define GATT_CHARACTERISTIC_COUNT            16

/** Number of Services in the data base */
#define GATT_SERVICE_COUNT                   5

/** Number of Characteristics that are configurable by the client */
#define GATT_DB_MAX_CONFIGUREABLE_CHAR       3

/** Maximum Length of any Characteristic Value/Descriptor */
#define GATT_DB_MAX_VAL_LENGTH                32

#define GATT_VALUE_ARRAY_SIZE                 3

#define GATT_CONST_VALUE_ARRAY_SIZE           203

#define GATT_DB_PEER_VALUE_ARRAY_SIZE         6

#define GATT_DB_MAX_ATTRIBUTES                42

#define GATT_UUID_ARRAY_SIZE                  62

#define GATT_DB_MAX_TYPE_COUNT                25

#define GATT_DB_MAX_PEER_CONFIGURATION        \
        (GATT_DB_PEER_VALUE_ARRAY_SIZE * BT_MAX_DEVICE_QUEUE_SIZE)

/** \} */

/** Service Instance Reference */

/** GAP Service */
#define GATT_SER_GAP_INST                  0

/** GATT Service */
#define GATT_SER_GATT_INST                 1

/** Battery Service */
#define GATT_SER_BATTERY_INST              2

/** DeviceInformation Service */
#define GATT_SER_DEV_INFO_INST             3

/** Heart Rate Service */
#define GATT_SER_HEART_RATE_INST           4

/** Characteristic Instance Reference */

/** DeviceName */
#define GATT_CHAR_DEV_NAME_INST            0

/** Appearance */
#define GATT_CHAR_APPEARANCE_INST          1

/** Service Changed */
#define GATT_CHAR_SER_CHNGD_INST           2

/** BatteryLevel */
#define GATT_CHAR_BATTERY_LVL_INST         3

/** ManufacturerName */
#define GATT_CHAR_MAN_NAME_INST            4

/** ModelNumber */
#define GATT_CHAR_MODEL_NO_INST            5

/** SerialNumber */
#define GATT_CHAR_SL_NO_INST               6

/** FirmwareRevision */
#define GATT_CHAR_FW_REV_INST              7

/** HardwareRevision */
#define GATT_CHAR_HW_REV_INST              8

/** SoftwareRevision */
#define GATT_CHAR_SW_REV_INST              9

/** SystemId */
#define GATT_CHAR_SYS_ID_INST              10

/** RegCertDataList */
#define GATT_CHAR_REG_CERT_DATA_INST       11

/** PnPID */
#define GATT_CHAR_PNP_ID_INST              12

/** HeartRateMeasurement */
#define GATT_CHAR_HR_MSRMNT                13

/** BodySensorLocation */
#define GATT_CHAR_BODY_SNSR_LOC_INST       14

/** HeartRateControlPoint */
#define GATT_CHAR_HR_CNTRL_PNT_INST        15

#endif /* _H_GATT_DB_ */

