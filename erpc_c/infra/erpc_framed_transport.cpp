/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * Copyright 2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "erpc_framed_transport.h"
#include "erpc_message_buffer.h"

#include <cassert>
#include <cstdio>

using namespace erpc;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

FramedTransport::FramedTransport(void)
: Transport()
, m_crcImpl(NULL)
#if !ERPC_THREADS_IS(NONE)
, m_sendLock()
, m_receiveLock()
#endif
{
}

FramedTransport::~FramedTransport(void) {}

void FramedTransport::setCrc16(Crc16 *crcImpl)
{
    assert(crcImpl);
    m_crcImpl = crcImpl;
}


static Header h;
static bool headerReceived = false;

erpc_status_t FramedTransport::receive(const Hash& channel, MessageBuffer *message)
{
    assert(m_crcImpl && "Uninitialized Crc16 object.");

    erpc_status_t ret = kErpcStatus_Fail;

    if(!headerReceived)
    {
        // Receive header first.
        ret = underlyingReceive(channel, (uint8_t *)&h, sizeof(h));

        if (ret == kErpcStatus_Success)
        {
            // received size can't be zero.
            if (h.m_messageSize == 0){
                ret = kErpcStatus_ReceiveFailed;
            }
            // received size can't be larger then buffer length.
            else if (h.m_messageSize > message->getLength()){
                ret = kErpcStatus_ReceiveFailed;
            } 
    
            if (ret == kErpcStatus_Success){
                headerReceived = true;
            }
        }
    }

    if (headerReceived)
    {
        // Receive rest of the message now we know its size.
        ret = underlyingReceive(channel, message->get(), h.m_messageSize);

        if (ret == kErpcStatus_Success)
        {
            // Verify CRC.
            uint16_t computedCrc = m_crcImpl->computeCRC16(message->get(), h.m_messageSize);
            if (computedCrc != h.m_crc)
            {
                ret = kErpcStatus_CrcCheckFailed;
            }
            else{
                /// and set message buffer length to used and continue with receive = succes
                message->setUsed(h.m_messageSize);
            }

            /// crc ok or crc failed, reset receive flags
            headerReceived = false;

        }
        else if (ret == kErpcStatus_Pending){
            /// do nothing
        }
        else{
            /// receive failed, reset receive flags
            headerReceived = false;
        }
    }

    return ret;
}

erpc_status_t FramedTransport::send(const Hash& channel, MessageBuffer *message)
{
    assert(m_crcImpl && "Uninitialized Crc16 object.");
#if !ERPC_THREADS_IS(NONE)
    Mutex::Guard lock(m_sendLock);
#endif

    uint16_t messageLength = message->getUsed();

    // Send header first.
    Header h;
    h.m_messageSize = messageLength;
    h.m_crc = m_crcImpl->computeCRC16(message->get(), messageLength);
    erpc_status_t ret = underlyingSend(channel, (uint8_t *)&h, sizeof(h));
    if (ret != kErpcStatus_Success)
    {
        return ret;
    }

    // Send the rest of the message.
    return underlyingSend(channel, message->get(), messageLength);
}
