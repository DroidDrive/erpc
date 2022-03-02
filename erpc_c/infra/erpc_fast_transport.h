/*
 * Copyright (c) 2014-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * Copyright 2021 ACRIOS Systems s.r.o.
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _EMBEDDED_RPC__FAST_TRANSPORT_H_
#define _EMBEDDED_RPC__FAST_TRANSPORT_H_

#include "erpc_config_internal.h"
#include "erpc_message_buffer.h"
#include "erpc_transport.h"

#include <cstring>

#if !ERPC_THREADS_IS(NONE)
#include "erpc_threading.h"
#endif

/*!
 * @addtogroup infra_transport
 * @{
 * @file
 */

////////////////////////////////////////////////////////////////////////////////
// Classes
////////////////////////////////////////////////////////////////////////////////

namespace erpc {



/*! @brief Contents of the header that prefixes each message. */
struct FastFrame{
    uint8_t serviceId;
    uint8_t messageSize;
    uint8_t payload[6] = {0};
};

class FastTransport : public Transport
{
public:
    /*!
     * @brief Constructor.
     */
    FastTransport(void);

    /*!
     * @brief Codec destructor
     */
    virtual ~FastTransport(void);

    /*!
     * @brief Receives an entire message.
     *
     * The frame header and message data are received. The CRC-16 in the frame header is
     * compared with the computed CRC. If the received CRC is invalid, #kErpcStatus_Fail
     * will be returned.
     *
     * The @a message is only filled with the message data, not the frame header.
     *
     * This function is blocking.
     *
     * @param[in] message Message buffer, to which will be stored incoming message.
     *
     * @retval kErpcStatus_Success When receiving was successful.
     * @retval kErpcStatus_CrcCheckFailed When receiving failed.
     * @retval other Subclass may return other errors from the underlyingReceive() method.
     */
    virtual erpc_status_t receive(const Hash& channel, MessageBuffer *message, bool skipCrc = false) override;

    /*!
     * @brief Function to send prepared message.
     *
     * @param[in] message Pass message buffer to send.
     *
     * @retval kErpcStatus_Success When sending was successful.
     * @retval other Subclass may return other errors from the underlyingSend() method.
     */
    virtual erpc_status_t send(const Hash& channel, MessageBuffer *message) override;

    /*!
     * @brief This functions sets the CRC-16 implementation.
     *
     * @param[in] crcImpl Object containing crc-16 compute function.
     */
    virtual void setCrc16(Crc16 *crcImpl) override;

protected:
    Crc16 *m_crcImpl; /*!< CRC object. */

#if !ERPC_THREADS_IS(NONE)
    Mutex m_sendLock;    //!< Mutex protecting send.
    Mutex m_receiveLock; //!< Mutex protecting receive.
#endif

    /*!
     * @brief Subclasses must implement this function to send data.
     *
     * @param[in] data Buffer to send.
     * @param[in] size Size of data to send.
     *
     * @retval Amount of Bytes written when data was written successfully.
     * @retval -1 (std::numeric_limits<uint32_t>::max()) When writing data ends with error.
     */
    virtual uint32_t underlyingSend(const erpc::Hash& channel, const uint8_t *data, uint32_t size) = 0;

    /*!
     * @brief Subclasses must implement this function to receive data.
     *
     * @param[inout] data Preallocated buffer for receiving data.
     * @param[in] size Size of data to read.
     *
     * @retval kErpcStatus_Success When data was read successfully.
     * @retval kErpcStatus_Fail When reading data ends with error.
     */
    virtual erpc_status_t underlyingReceive(const erpc::Hash& channel, uint8_t *data, uint32_t size) = 0;

private:
    uint32_t sentBytesInBuffer_ = 0;
};

} // namespace erpc

/*! @} */

#endif // _EMBEDDED_RPC__FRAMED_TRANSPORT_H_
