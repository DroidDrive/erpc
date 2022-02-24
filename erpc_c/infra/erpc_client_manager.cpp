/*
 * Copyright (c) 2014, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * Copyright 2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "erpc_client_manager.h"

#include "assert.h"

using namespace erpc;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

#if ERPC_NESTED_CALLS_DETECTION
extern bool nestingDetection;
#ifndef _WIN32
#pragma weak nestingDetection
bool nestingDetection = false;
#endif
#endif

void ClientManager::setTransport(Transport *transport)
{
    m_transport = transport;
}

RequestContext ClientManager::createRequest(const erpc::Hash& channel, bool isOneway)
{
    // Create codec to read and write the request.
    Codec *codec = createBufferAndCodec();

    return RequestContext(channel, ++m_sequence, codec, isOneway);
}

bool ClientManager::performRequest(RequestContext &request)
{
    bool result = true;

    // Check the codec status
    if (kErpcStatus_Success != (request.getCodec()->getStatus()) && (kErpcStatus_Pending != (request.getCodec()->getStatus())))
    {
        // Do not perform the request
        result = false;
    }

#if ERPC_NESTED_CALLS
    if (result)
    {
        assert(m_serverThreadId && "server thread id was not set");
        if (Thread::getCurrentThreadId() == m_serverThreadId)
        {
            performNestedClientRequest(request);
            result = false;
        }
    }
#endif
    if(result)
    {
        performClientRequest(request);
    }
    return result;
}

bool ClientManager::performClientRequest(RequestContext &request)
{
    erpc_status_t err;

#if ERPC_NESTED_CALLS_DETECTION
    if (!request.isOneway() && nestingDetection)
    {
        request.getCodec()->updateStatus(kErpcStatus_NestedCallFailure);
        return false;
    }
#endif

#if ERPC_MESSAGE_LOGGING
    if (request.getCodec()->isStatusOk() == true)
    {
        err = logMessage(request.getCodec()->getBuffer());
        request.getCodec()->updateStatus(err);
        return false;
    }
#endif

    if(request.getState() == RequestContextState::SENDING)
    {
         // Send invocation request to server.
        err = m_transport->send(request.getChannel(), request.getCodec()->getBuffer());
        if( err == kErpcStatus_Success){
            request.setState(RequestContextState::SENT);
        }
        else if (err == kErpcStatus_Pending){
            return false;
        }
        else{
            request.getCodec()->updateStatus(err);
            return false;
        }
    }


    // If the request is oneway, then there is nothing more to do.
    if (!request.isOneway())
    {
        if(request.getState() == RequestContextState::SENT || request.getState() == RequestContextState::PENDING)
        {
            // Receive reply.
            err = m_transport->receive(request.getChannel(), request.getCodec()->getBuffer());
            if (err)
            {
                request.getCodec()->updateStatus(err);
                if(err == kErpcStatus_Pending)
                    request.setState(RequestContextState::PENDING);
                return false;
            }

#if ERPC_MESSAGE_LOGGING
            err = logMessage(request.getCodec()->getBuffer());
            if (err)
            {
                request.getCodec()->updateStatus(err);
                return false;
            }
#endif

            // Check the reply.
            verifyReply(request);
            if (err)
            {
                request.getCodec()->updateStatus(err);
                return false;
            }
        }
    }
    request.setState(RequestContextState::DONE);
    return true;
}

#if ERPC_NESTED_CALLS
bool ClientManager::performNestedClientRequest(RequestContext &request)
{
    erpc_status_t err;

    assert(m_transport && "transport/arbitrator not set");

#if ERPC_MESSAGE_LOGGING
    if (request.getCodec()->isStatusOk() == true)
    {
        err = logMessage(request.getCodec()->getBuffer());
        request.getCodec()->updateStatus(err);
    }
#endif

    // Send invocation request to server.
    if (request.getCodec()->isStatusOk() == true)
    {
        err = m_transport->send(request.getChannel(), equest.getCodec()->getBuffer());
        request.getCodec()->updateStatus(err);
    }

    // If the request is oneway, then there is nothing more to do.
    if (!request.isOneway())
    {
        // Receive reply.
        if (request.getCodec()->isStatusOk() == true)
        {
            assert(m_server && "server for nesting calls was not set");
            err = m_server->run(request);
            request.getCodec()->updateStatus(err);
        }

#if ERPC_MESSAGE_LOGGING
        if (request.getCodec()->isStatusOk() == true)
        {
            err = logMessage(request.getCodec()->getBuffer());
            request.getCodec()->updateStatus(err);
        }
#endif

        // Check the reply.
        if (request.getCodec()->isStatusOk() == true)
        {
            verifyReply(request);
        }
    }
}
#endif

void ClientManager::verifyReply(RequestContext &request)
{
    message_type_t msgType;
    uint32_t service;
    Hash requestNumber;
    uint32_t sequence;

    // Some transport layers change the request's message buffer pointer (for things like zero
    // copy support), so inCodec must be reset to work with correct buffer.
    request.getCodec()->reset();

    // Extract the reply header.
    request.getCodec()->startReadMessage(&msgType, &service, &requestNumber, &sequence);

    if (request.getCodec()->isStatusOk() == true)
    {
        // Verify that this is a reply to the request we just sent.
        /// kikass13: how about not doing this?
        /// because we reset request periodically now, so there is no point in doing this
        // if ((msgType != kReplyMessage) || (sequence != request.getSequence()))
        // {
        //     request.getCodec()->updateStatus(kErpcStatus_ExpectedReply);
        // }
    }
}

Codec *ClientManager::createBufferAndCodec(void)
{
    Codec *codec = m_codecFactory->create();
    MessageBuffer message;

    if (codec != NULL)
    {
        message = m_messageFactory->create();
        if (message.get())
        {
            codec->setBuffer(message);
        }
        else
        {
            // Dispose of buffers and codecs.
            m_codecFactory->dispose(codec);
            codec = NULL;
        }
    }

    return codec;
}

void ClientManager::releaseRequest(RequestContext &request)
{
    m_messageFactory->dispose(request.getCodec()->getBuffer());
    m_codecFactory->dispose(request.getCodec());
}

void ClientManager::callErrorHandler(erpc_status_t err, const erpc::Hash functionID)
{
    if (m_errorHandler != NULL)
    {
        m_errorHandler(err, functionID);
    }
}
