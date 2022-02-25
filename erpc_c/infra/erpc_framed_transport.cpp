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

#include <limits>

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

erpc_status_t FramedTransport::receive(const Hash& channel, MessageBuffer *message, bool skipCrc)
{
    assert(m_crcImpl && "Uninitialized Crc16 object.");

    erpc_status_t ret = kErpcStatus_Fail;

    if(!headerReceived_)
    {
        // Receive header first.
        ret = underlyingReceive(channel, (uint8_t *)&headerBuffer_, sizeof(headerBuffer_));

        if (ret == kErpcStatus_Success)
        {
            /// evaluate redundant message sizes
            bool messageSizeOk = true;
            uint16_t realMessageSize = 0;
            if(headerBuffer_.m_messageSize == headerBuffer_.m_messageSize2){ // a == b
                realMessageSize = headerBuffer_.m_messageSize;
            }
            else if(headerBuffer_.m_messageSize == headerBuffer_.m_messageSize3){ // a == c
                realMessageSize = headerBuffer_.m_messageSize;
            }
            else if(headerBuffer_.m_messageSize2 == headerBuffer_.m_messageSize3){ // b == c
                realMessageSize = headerBuffer_.m_messageSize2;
            }
            else{
                messageSizeOk = false;
            }
            /// remember correct message size
            headerBuffer_.m_messageSize = realMessageSize;

            // received size can't be zero.
            if (headerBuffer_.m_messageSize == 0){
                ret = kErpcStatus_ReceiveFailed;
            }
            // received size can't be larger then buffer length.
            else if (headerBuffer_.m_messageSize > message->getLength()){
                ret = kErpcStatus_ReceiveFailed;
            } 
    
            if (ret == kErpcStatus_Success){
                headerReceived_ = true;
            }
        }
    }

    if (headerReceived_)
    {
        // Receive rest of the message now we know its size.
        ret = underlyingReceive(channel, message->get(), headerBuffer_.m_messageSize);

        if (ret == kErpcStatus_Success)
        {
            // Verify CRC.
            uint16_t computedCrc = m_crcImpl->computeCRC16(message->get(), headerBuffer_.m_messageSize);
            if (!skipCrc && (computedCrc != headerBuffer_.m_crc))
            {
                ret = kErpcStatus_CrcCheckFailed;
            }
            else{
                /// and set message buffer length to used and continue with receive = succes
                message->setUsed(headerBuffer_.m_messageSize);
            }

            /// crc ok or crc failed, reset receive flags
            headerReceived_ = false;

        }
        else if (ret == kErpcStatus_Pending){
            /// do nothing
        }
        else{
            /// receive failed, reset receive flags
            headerReceived_ = false;
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

    erpc_status_t ret = kErpcStatus_Success;
    uint16_t messageLength = message->getUsed();
    uint8_t* bytePtr = reinterpret_cast<uint8_t*>(message->get());

    // Send header first.
    if(!this->headerSend_)
    {
        Header h;
        h.m_messageSize = messageLength;
        h.m_messageSize2 = messageLength;
        h.m_messageSize3 = messageLength;
        h.m_crc = m_crcImpl->computeCRC16(message->get(), messageLength);

        /// this should be done in one cycle, and can be repeated N times, 
        /// because it's not fragmented into multiple header packets
        uint32_t sendBytes = underlyingSend(channel, (uint8_t *)&h, sizeof(h));
        if(sendBytes == sizeof(h)){
            this->headerSend_ = true;
        }
        else if (sendBytes == std::numeric_limits<uint32_t>::max()) {
            ret = kErpcStatus_SendFailed;
        }
        else{
            ret = kErpcStatus_Pending;
        }
    }
    
    // Send the rest of the message.
    if(this->headerSend_)
    {
        /// calculate new buffer sending address
        bytePtr += this->sentBytesInBuffer_;
        uint32_t missingMessageBytes = messageLength - this->sentBytesInBuffer_;

        /// ...
        uint32_t sendBytes = underlyingSend(channel, bytePtr, missingMessageBytes);
        this->sentBytesInBuffer_ += sendBytes;

        if(this->sentBytesInBuffer_ == messageLength){
            ret = kErpcStatus_Success;
            this->headerSend_ = false;
            this->sentBytesInBuffer_ = 0;
        }
        else if (sendBytes == std::numeric_limits<uint32_t>::max()) 
        {
            ret = kErpcStatus_SendFailed;
            this->headerSend_ = false;
            this->sentBytesInBuffer_ = 0;
        }
        else{
            ret = kErpcStatus_Pending;
        }
    }
  
    return ret;
}
