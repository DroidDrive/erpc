/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * Copyright 2019 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ERPC_TRANSPORT_SETUP_ADDONS_H_
#define _ERPC_TRANSPORT_SETUP_ADDONS_H_

#include "erpc_fifo_transport.h"

/*!
 * @addtogroup transport_setup
 * @{
 * @file
 */

////////////////////////////////////////////////////////////////////////////////
// Types
////////////////////////////////////////////////////////////////////////////////

//! @brief Opaque transport object type.
typedef struct ErpcTransport *erpc_transport_t;
//! @brief Ready callback object type for RPMsg-Lite transport.
typedef void (*rpmsg_ready_cb)(void);

////////////////////////////////////////////////////////////////////////////////
// API
////////////////////////////////////////////////////////////////////////////////
erpc_transport_t erpc_transport_fifo_init(erpc::Buffer* receiveBuffer, erpc::Buffer* sendBuffer);

/*! @} */

#endif // _ERPC_TRANSPORT_SETUP_ADDONS_H_
