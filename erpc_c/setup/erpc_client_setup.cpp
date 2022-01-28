/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * Copyright 2020-2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "erpc_client_setup.h"

#include "erpc_basic_codec.h"
#include "erpc_client_manager.h"
#include "erpc_crc16.h"
#include "erpc_manually_constructed.h"
#include "erpc_message_buffer.h"
#include "erpc_transport.h"

#include <cassert>
#if ERPC_NESTED_CALLS
#include "erpc_threading.h"
#endif

using namespace erpc;

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

// // global client variables
// ERPC_MANUALLY_CONSTRUCTED(ClientManager, s_client);
// ClientManager *g_client;
// #pragma weak g_client
// ERPC_MANUALLY_CONSTRUCTED(BasicCodecFactory, s_codecFactory);
// ERPC_MANUALLY_CONSTRUCTED(Crc16, s_crc16);
ERPC_MANUALLY_CONSTRUCTED_ARRAY(ClientManager, s_clients, 10);
ClientManager* g_clients[10] = { nullptr };
// #pragma weak g_client0
ERPC_MANUALLY_CONSTRUCTED_ARRAY(BasicCodecFactory, s_codecFactorys, 10);
ERPC_MANUALLY_CONSTRUCTED_ARRAY(Crc16, s_crc16s, 10);
static size_t clientCounter = 0;
////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

int erpc_client_init(erpc_transport_t transport, erpc_mbf_t message_buffer_factory)
{
    assert(transport);

    Transport *castedTransport;

    if(clientCounter < MAX_CLIENT_COUNT)
    {
        // Init factories.
        s_codecFactorys[clientCounter].construct();

        // Init client manager with the provided transport.
        s_clients[clientCounter].construct();
        castedTransport = reinterpret_cast<Transport *>(transport);
        s_crc16s[clientCounter].construct();
        castedTransport->setCrc16(s_crc16s[clientCounter].get());
        s_clients[clientCounter]->setTransport(castedTransport);
        s_clients[clientCounter]->setCodecFactory(s_codecFactorys[clientCounter]);
        s_clients[clientCounter]->setMessageBufferFactory(reinterpret_cast<MessageBufferFactory *>(message_buffer_factory));
        g_clients[clientCounter] = s_clients[clientCounter];
    }
    return clientCounter < MAX_CLIENT_COUNT ? clientCounter++ : -1;
}

void erpc_client_set_error_handler(size_t id, client_error_handler_t error_handler)
{
    if (g_clients[id] != NULL)
    {
        g_clients[id]->setErrorHandler(error_handler);
    }
}

void erpc_client_set_crc(size_t id, uint32_t crcStart)
{
    s_crc16s[id]->setCrcStart(crcStart);
}

#if ERPC_NESTED_CALLS
void erpc_client_set_server(erpc_server_t server)
{
    if (g_client != NULL)
    {
        g_client->setServer(reinterpret_cast<Server *>(server));
    }
}

void erpc_client_set_server_thread_id(void *serverThreadId)
{
    if (g_client != NULL)
    {
        g_client->setServerThreadId(reinterpret_cast<Thread::thread_id_t *>(serverThreadId));
    }
}
#endif

#if ERPC_MESSAGE_LOGGING
bool erpc_client_add_message_logger(erpc_transport_t transport)
{
    bool retVal;

    if (g_client == NULL)
    {
        retVal = false;
    }
    else
    {
        retVal = g_client->addMessageLogger(reinterpret_cast<Transport *>(transport));
    }

    return retVal;
}
#endif

#if ERPC_PRE_POST_ACTION
void erpc_client_add_pre_cb_action(pre_post_action_cb preCB)
{
    assert(g_client);

    g_client->addPreCB(preCB);
}

void erpc_client_add_post_cb_action(pre_post_action_cb postCB)
{
    assert(g_client);

    g_client->addPostCB(postCB);
}
#endif

void erpc_client_deinit(size_t id)
{
    s_crc16s[id].destroy();
    s_clients[id].destroy();
    s_codecFactorys[id].destroy();
    g_clients[id] = NULL;
}
