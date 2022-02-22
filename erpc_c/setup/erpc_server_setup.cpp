/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * Copyright 2020-2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "erpc_server_setup.h"

#include "erpc_basic_codec.h"
#include "erpc_crc16.h"
#include "erpc_manually_constructed.h"
#include "erpc_message_buffer.h"
#include "erpc_simple_server.h"
#include "erpc_transport.h"

#include <cassert>
#include <array>

using namespace erpc;

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

// global server variables
// ERPC_MANUALLY_CONSTRUCTED(SimpleServer, s_server);
// extern SimpleServer *g_server;
// SimpleServer *g_server = NULL;
ERPC_MANUALLY_CONSTRUCTED_ARRAY(SimpleServer, s_servers, 10);
SimpleServer* g_servers[MAX_SERVER_COUNT] = {nullptr};
ERPC_MANUALLY_CONSTRUCTED_ARRAY(BasicCodecFactory, s_codecFactorys, 10);
ERPC_MANUALLY_CONSTRUCTED_ARRAY(Crc16, s_crc16s, 10);
static size_t serverCount = 0;
// std::array<SimpleServer, MAX_SERVER_SIZE> servers[] = {};

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

// erpc_server_t erpc_server_init(erpc_transport_t transport, erpc_mbf_t message_buffer_factory)
// {
//     assert(transport);

//     Transport *castedTransport;
 
//     // Init factories.
//     s_codecFactory.construct();

//     // Init server with the provided transport.
//     s_server.construct();
//     castedTransport = reinterpret_cast<Transport *>(transport);
//     s_crc16.construct();
//     castedTransport->setCrc16(s_crc16.get());
//     s_server->setTransport(castedTransport);
//     s_server->setCodecFactory(s_codecFactory);
//     s_server->setMessageBufferFactory(reinterpret_cast<MessageBufferFactory *>(message_buffer_factory));
//     g_server = s_server;
//     return reinterpret_cast<erpc_server_t>(g_server);
// }
int erpc_server_init(erpc_transport_t transport, erpc_mbf_t message_buffer_factory)
{
    assert(transport);

    Transport *castedTransport;
    
    if(serverCount < MAX_SERVER_COUNT)
    {
        // Init factories.
        s_codecFactorys[serverCount].construct();

        // Init server with the provided transport.
        s_servers[serverCount].construct();
        s_servers[serverCount]->setId(serverCount);
        castedTransport = reinterpret_cast<Transport *>(transport);
        s_crc16s[serverCount].construct();
        castedTransport->setCrc16(s_crc16s[serverCount].get());
        s_servers[serverCount]->setTransport(castedTransport);
        s_servers[serverCount]->setCodecFactory(s_codecFactorys[serverCount]);
        s_servers[serverCount]->setMessageBufferFactory(reinterpret_cast<MessageBufferFactory *>(message_buffer_factory));
        g_servers[serverCount] = s_servers[serverCount];
    }
    return serverCount < MAX_SERVER_COUNT ? serverCount++ : -1;
}

void erpc_server_reinit(size_t id, erpc_transport_t transport, erpc_mbf_t message_buffer_factory)
{
    assert(transport);

    Transport *castedTransport;
    
    if ( id < serverCount) {
        // Init factories.
        s_codecFactorys[serverCount].construct();

        // Init server with the provided transport.
        s_servers[serverCount].construct();
        s_servers[serverCount]->setId(serverCount);
        castedTransport = reinterpret_cast<Transport *>(transport);
        s_crc16s[serverCount].construct();
        castedTransport->setCrc16(s_crc16s[serverCount].get());
        s_servers[serverCount]->setTransport(castedTransport);
        s_servers[serverCount]->setCodecFactory(s_codecFactorys[serverCount]);
        s_servers[serverCount]->setMessageBufferFactory(reinterpret_cast<MessageBufferFactory *>(message_buffer_factory));
        g_servers[serverCount] = s_servers[serverCount];
    }
}

// void erpc_server_deinit(void)
// {
//     s_crc16.destroy();
//     s_codecFactory.destroy();
//     s_server.destroy();
//     g_server = NULL;
// }
void erpc_server_deinit(size_t id)
{
    s_crc16s[id].destroy();
    s_codecFactorys[id].destroy();
    s_servers[id].destroy();
    g_servers[id] = NULL;
}

void erpc_add_service_to_server(size_t id, void *service)
{
    if ((g_servers[id] != NULL) && (service != NULL))
    {
        g_servers[id]->addService(static_cast<erpc::Service *>(service));
    }
}

void erpc_remove_service_from_server(size_t id, void *service)
{
    if ((g_servers[id] != NULL) && (service != NULL))
    {
        g_servers[id]->removeService(static_cast<erpc::Service *>(service));
    }
}

void erpc_server_set_crc(size_t id, uint32_t crcStart)
{
    s_crc16s[id]->setCrcStart(crcStart);
}

erpc_status_t erpc_server_run(size_t id)
{
    erpc_status_t status;

    if (g_servers[id] == NULL)
    {
        status = kErpcStatus_Fail;
    }
    else
    {
        status = g_servers[id]->run();
    }

    return status;
}

erpc_status_t erpc_server_poll(size_t id)
{
    erpc_status_t status;

    if (g_servers[id] == NULL)
    {
        status = kErpcStatus_Fail;
    }
    else
    {
        status = g_servers[id]->poll();
    }

    return status;
}

erpc_status_t erpc_server_flush(size_t id){
    erpc_status_t status;

    if (g_servers[id] == NULL)
    {
        status = kErpcStatus_Fail;
    }
    else
    {
        g_servers[id]->flush();
        status = kErpcStatus_Success;
    }

    return status;
}


void erpc_server_stop(size_t id)
{
    if (g_servers[id] != NULL)
    {
        g_servers[id]->stop();
    }
}

#if ERPC_MESSAGE_LOGGING
bool erpc_server_add_message_logger(erpc_transport_t transport)
{
    bool retVal;

    if (g_server == NULL)
    {
        retVal = false;
    }
    else
    {
        retVal = g_server->addMessageLogger(reinterpret_cast<Transport *>(transport));
    }

    return retVal;
}
#endif

#if ERPC_PRE_POST_ACTION
void erpc_client_add_pre_cb_action(pre_post_action_cb preCB)
{
    assert(g_server);

    g_server->addPreCB(preCB);
}

void erpc_client_add_post_cb_action(pre_post_action_cb postCB)
{
    assert(g_server);

    g_server->addPostCB(postCB);
}
#endif
