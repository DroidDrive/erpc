/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _ERPC_MBF_SETUP_ADDONS_H_
#define _ERPC_MBF_SETUP_ADDONS_H_

#include "erpc_transport_setup.h"
#include "erpc_setup_mbf_static_fixed.h"

/*!
 * @addtogroup message_buffer_factory_setup
 * @{
 * @file
 */

////////////////////////////////////////////////////////////////////////////////
// Types
////////////////////////////////////////////////////////////////////////////////

//! @brief Opaque MessageBufferFactory object type.
typedef struct ErpcMessageBufferFactory *erpc_mbf_t;

////////////////////////////////////////////////////////////////////////////////
// API
////////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Create MessageBuffer factory which is using static fixed allocated buffers.
 */
template <size_t BUFFER_COUNT, size_t WORD_COUNT, class WORD_SIZE=uint8_t>
erpc_mbf_t erpc_mbf_static_fixed_init(void){   
    static ManuallyConstructed<StaticFixedMessageBufferFactory<BUFFER_COUNT, WORD_COUNT, WORD_SIZE>> s_msgFactory;
    s_msgFactory.construct();
    return reinterpret_cast<erpc_mbf_t>(s_msgFactory.get());
    return nullptr;
}

#endif // _ERPC_MBF_SETUP_ADDONS_H_
