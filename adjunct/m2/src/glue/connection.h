/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CONNECTION_H
#define CONNECTION_H

// ----------------------------------------------------

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/util/autodelete.h"

class ProtocolConnection
{
public:
     virtual ~ProtocolConnection() {}

// ----------------------------------------------------

    class Client : public Autodeletable
    {
    public:
		Client() : Autodeletable() {}
		virtual ~Client() {}

        // naming convention - same as for modules/url/comm-classes

        /**
         * Called when the network layer has received data
         * and data is available in the network buffer, and
         * a call to ReadData can be performed without blocking
         **/

        // could be renamed to OnRead
        virtual OP_STATUS ProcessReceivedData() = 0;

        /**
         * Called when the network layer is available for
         * writing again after a failure, probably because
         * of a full network buffer, and a call to SendData can
         * be called without blocking.
         */

        // could be renamed to OnWrite
        virtual void RequestMoreData() = 0;

		/**
		 *  Called on close and fail
         */
        virtual void OnClose(OP_STATUS) = 0;

		/**
		 * Called when connection should be restarted
		 */
		virtual void OnRestartRequested() = 0;
    };

// ----------------------------------------------------

    virtual OP_STATUS SetClient(Client* client) = 0;

    typedef UINT16 port_t;

	virtual void Release() = 0;

    /**
     * Connects
     */

    virtual OP_STATUS Connect(const char* hostname, const char* service, port_t port, BOOL ssl, BOOL V3_handshake = FALSE) = 0;


    /**
     * Read data from the network connection.
     * This method is guaranteed to never block.
     */

    virtual unsigned int Read(char* buf, size_t buflen) = 0;

    /**
     * Writes data to the network connection
     * This method is guaranteed to never block
     */

    virtual OP_STATUS Send(char *buf, size_t buflen) = 0;

	/**
	 * Set a timeout value for the connection. If it's idle longer than the
	 * given amount of seconds, OnClose() will be called. Default timeout
	 * value should be 0 (no timeout will be detected).
	 * @param seconds Seconds to timeout
	 * @param timeout_on_idle Whether to timeout even if we didn't send a message ourself (i.e.
	 *        expect the server to keep the connection alive)
	 */
	virtual OP_STATUS SetTimeout(time_t seconds, BOOL timeout_on_idle) = 0;

    /**
     * Enter TLS mode
     */

	virtual OP_STATUS StartTLS() = 0;


	/** Stop the timeout timer from cutting the connection
	  * Needed for IMAP where it's typical to have an IDLE connection
	  * Should be called when the connection doesn't expect more data
	  */
	virtual void StopConnectionTimer() = 0;

    /**
     * Allocate a buffer for use with the network code
	 * This is needed because the buffer for sending data
	 * is shared between the client code and the comm-code,
	 * and thus must be put under the same memorymanager (same dll).
     */

   	virtual char* AllocBuffer(size_t size) = 0;

    /**
     * Deallocate a buffer for use with the network code
	 * This is needed because the buffer for sending data
	 * is shared between the client code and the comm-code,
	 * and thus must be put under the same memorymanager (same dll).
     */

   	virtual void FreeBuffer(char* buf) = 0;

// ----------------------------------------------------

};

#endif // CONNECTION_H

// ----------------------------------------------------
