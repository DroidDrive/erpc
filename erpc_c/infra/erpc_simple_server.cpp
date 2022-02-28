/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * Copyright 2019-2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "erpc_common.h"
#include "erpc_simple_server.h"

using namespace erpc;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

void SimpleServer::disposeBufferAndCodec(Codec *codec)
{
    if (codec != NULL)
    {
        if (codec->getBuffer() != NULL)
        {
            m_messageFactory->dispose(codec->getBuffer());
        }
        m_codecFactory->dispose(codec);
    }
}

erpc_status_t SimpleServer::runInternal(erpc::Hash& channel)
{
    erpc_status_t err = kErpcStatus_Success;
    if(m_state == State::SEND_DONE || m_state == State::RECEIVE){
        /// beginning or already started receiving?
        /// receive more input data (header + payload)
        err = runInternalBegin(&m_codec, m_buff, m_msgType, m_serviceId, channel, m_sequence);
        /// successful done receiving?
        if (err == kErpcStatus_Success)
        {
            /// acknoledge, go forward
            m_state = State::RECEIVE_DONE;
        }
        else if (err == kErpcStatus_Pending){
            /// do nothing
        }
        else{
            /// pretend that nothing happened, reset state machine to the beginning
            m_state = State::SEND_DONE;
            return err;
        }
    }
    if(m_state == State::RECEIVE_DONE || m_state == State::PROCESS || m_state == State::PROCESS_DONE || m_state == State::SEND){
        /// successful done receiving?
        /// start processing and response sending
        err = runInternalEnd(m_codec, m_msgType, m_serviceId, channel, m_sequence);
        if (err == kErpcStatus_Success)
        {
            /// acknowledge, were done, go to start
            m_state = State::SEND_DONE;
        }
        else if (err == kErpcStatus_Pending){
            /// do nothing
        }
        else{
            /// pretend that nothing happened, reset state machine to the beginning
            m_state = State::SEND_DONE;
            return err;
        }
    }
    
    /// return whatever status we have
    return err;
}

erpc_status_t SimpleServer::runInternalBegin(Codec **codec, MessageBuffer &buff, message_type_t &msgType,
                                             uint32_t &serviceId, Hash& methodId, uint32_t &sequence)
{
    erpc_status_t err = kErpcStatus_Success;

    if(m_state == State::SEND_DONE)
    {
        if (m_messageFactory->createServerBuffer() == true)
        {
            buff = m_messageFactory->create();
            if (!buff.get())
            {
                err = kErpcStatus_MemoryError;
            }
        }
        else{
            err = kErpcStatus_MemoryError;
        }

        // Receive the next invocation request.
        if (err == kErpcStatus_Success)
        {
            m_state = State::RECEIVE;
        }
    }

    if(m_state == State::RECEIVE)
    {
        err = m_transport->receive(methodId, &buff, (*codec)->getSkipCrc());
        // Receive the next invocation request.
        if (err == kErpcStatus_Success)
        {
            m_state = State::RECEIVE_DONE;
#if ERPC_PRE_POST_ACTION
            pre_post_action_cb preCB = this->getPreCB();
            if (preCB != NULL)
            {
                preCB();
            }
#endif

#if ERPC_MESSAGE_LOGGING
            err = logMessage(&buff);
#endif       
            *codec = m_codecFactory->create();
            if (*codec == NULL)
            {
                err = kErpcStatus_MemoryError;
            }

            if (err == kErpcStatus_Success)
            {
                (*codec)->setBuffer(buff);

                err = readHeadOfMessage(*codec, msgType, serviceId, methodId, sequence);
                if (err != kErpcStatus_Success)
                {
                    // Dispose of buffers and codecs.
                    disposeBufferAndCodec(*codec);
                }
            }
        }
    }

    if (err != kErpcStatus_Success && err != kErpcStatus_Pending)
    {
        // Dispose of buffers.
        if (buff.get() != NULL)
        {
            m_messageFactory->dispose(&buff);
        }
    }

    return err;
}

erpc_status_t SimpleServer::runInternalEnd(Codec *codec, message_type_t msgType, uint32_t serviceId, Hash methodId,
                                           uint32_t sequence)
{
    erpc_status_t err = kErpcStatus_Success;

    if(m_state == State::RECEIVE_DONE){
        m_state = State::PROCESS;
        err = processMessage(codec, msgType, serviceId, methodId, sequence);
        if (err == kErpcStatus_Success){
            m_state = State::PROCESS_DONE;
        }
        else{
            return kErpcStatus_Fail;
        }
    }

    if(m_state == State::PROCESS_DONE)
    {
        m_state = State::SEND;
        if (msgType != kOnewayMessage)
        {
#if ERPC_MESSAGE_LOGGING
            err = logMessage(codec->getBuffer());
            if (err == kErpcStatus_Success)
            {
#endif
                err = m_transport->send(methodId, codec->getBuffer());
#if ERPC_MESSAGE_LOGGING
            }
#endif
        }

#if ERPC_PRE_POST_ACTION
        pre_post_action_cb postCB = this->getPostCB();
        if (postCB != NULL)
        {
            postCB();
        }
#endif
        if(err == kErpcStatus_Success){
            m_state = State::SEND_DONE;
              // Dispose of buffers and codecs.
            disposeBufferAndCodec(codec);
        }
    }

    return err;
}

erpc_status_t SimpleServer::run(void)
{
    erpc_status_t err = kErpcStatus_Success;
    erpc::Hash channel{};
    while (!err && m_isServerOn)
    {
        err = runInternal(channel);
    }
    return err;
}

#if ERPC_NESTED_CALLS
erpc_status_t SimpleServer::run(RequestContext &request)
{
    erpc_status_t err = kErpcStatus_Success;
    message_type_t msgType;
    uint32_t serviceId;
    uint32_t methodId;
    uint32_t sequence;

    while (!err && m_isServerOn)
    {
        MessageBuffer buff;
        Codec *codec = NULL;

        // Handle the request.
        err = runInternalBegin(&codec, buff, msgType, serviceId, methodId, sequence);

        if (err != kErpcStatus_Success)
        {
            break;
        }

        if (msgType == kReplyMessage)
        {
            if (sequence == request.getSequence())
            {
                // Swap the received message buffer with the client's message buffer.
                request.getCodec()->getBuffer()->swap(&buff);
                codec->setBuffer(buff);
            }

            // Dispose of buffers and codecs.
            disposeBufferAndCodec(codec);

            if (sequence != request.getSequence())
            {
                // Ignore message
                continue;
            }

            break;
        }
        else
        {
            err = runInternalEnd(codec, msgType, serviceId, methodId, sequence);
        }
    }
    return err;
}
#endif

erpc_status_t SimpleServer::poll(void)
{
    erpc_status_t err;

    if (m_isServerOn)
    {
        /// do your usual thing (check for received message and run internals) 
        /// when we have no current special state (we are done with whatever was before)
        if(m_state == State::SEND_DONE){
            m_last_channel = m_transport->hasMessage();
            if (m_last_channel != 0)
            {
                err = runInternal(m_last_channel);
            }
            else
            {
                err = kErpcStatus_Success;
            }
        }
        else{
            /// else if the state is whatever else, we have succesfully read (or are stille reading) 
            /// an incomig message and now have to process it / send the answer 
            err = runInternal(m_last_channel);
        }
    }
    else
    {
        err = kErpcStatus_ServerIsDown;
    }

    return err;
}

void SimpleServer::stop(void)
{
    m_isServerOn = false;
}


void SimpleServer::flush(void)
{
    m_transport->flush();
}