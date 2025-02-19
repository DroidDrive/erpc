/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * Copyright 2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "erpc_fast_transport.h"
#include "erpc_message_buffer.h"

/// for resolving the forward declaration of Codec in erpc_transport.h
#include "erpc_basic_codec.h"

#include <cassert>
#include <cstdio>

#include <limits>

using namespace erpc;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

FastTransport::FastTransport(void)
: Transport()
, m_crcImpl(NULL)
#if !ERPC_THREADS_IS(NONE)
, m_sendLock()
, m_receiveLock()
#endif
{
}

FastTransport::~FastTransport(void) {}

void FastTransport::setCrc16(Crc16 */*crcImpl*/)
{
    /// we dont have a crc to set 
}

erpc_status_t FastTransport::receive(const Hash& channel, MessageBuffer *message)
{
    erpc_status_t ret = kErpcStatus_Fail;
    FastFrame frame; 

    // Receive a single FastFrame
    ret = underlyingReceive(channel, (uint8_t *)message->get(), sizeof(FastFrame));
    if (ret == kErpcStatus_Success)
    {
        // We do not verify CRC.
        // uint16_t computedCrc = m_crcImpl->computeCRC16(message->get(), headerBuffer_.m_messageSize);
        // if (computedCrc != headerBuffer_.m_crc)
        // {
        //     ret = kErpcStatus_CrcCheckFailed;
        // }
       
        /// and set message buffer length to used and continue with receive = succes
        message->setUsed(sizeof(FastFrame));
    }
    else if (ret == kErpcStatus_Pending){
        /// do nothing
    }
    else{
    }
    
    return ret;
}

erpc_status_t FastTransport::send(const Hash& channel, MessageBuffer *message)
{
    erpc_status_t ret = kErpcStatus_Success;
    uint16_t messageLength = message->getUsed();
    FastFrame frame;

    /// message data should not exceed our fast frame
    if(messageLength > sizeof(FastFrame)){
        return kErpcStatus_BufferOverrun;
    }


    /// copy message buffer data into frame,
    /// if there are more then 6 bytes in messagebuffer, the rest
    /// will be CUT OFF
    message->read(0, &frame.serviceId, sizeof(uint8_t));
    message->read(1, &frame.payload, FastFrame::PAYLOAD_SIZE);
    uint8_t* bytePtr = reinterpret_cast<uint8_t*>(&frame);

    /// calculate new buffer sending address
    bytePtr += this->sentBytesInBuffer_;
    uint32_t missingMessageBytes = sizeof(FastFrame) - this->sentBytesInBuffer_;

    /// ...
    uint32_t sendBytes = underlyingSend(channel, bytePtr, missingMessageBytes);
    this->sentBytesInBuffer_ += sendBytes;

    if(this->sentBytesInBuffer_ == sizeof(FastFrame)){
        ret = kErpcStatus_Success;
        this->sentBytesInBuffer_ = 0;
    }
    else if (sendBytes == std::numeric_limits<uint32_t>::max()) 
    {

        ret = kErpcStatus_SendFailed;
        this->sentBytesInBuffer_ = 0;
    }
    else{
        ret = kErpcStatus_Pending;
    }
  
    return ret;
}

void FastTransport::codecCreationCallback(Codec* codec){
    codec->setFast(true);
}