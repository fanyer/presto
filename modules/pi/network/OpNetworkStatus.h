/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OPNETWORKSTATUS_H
#define OPNETWORKSTATUS_H

typedef OP_STATUS OP_NETWORK_STATUS;

/** Constants to use with OP_NETWORK_STATUS. */
class OpNetworkStatus : public OpStatus
{
public:
    enum
    {
		/** Network link or interface error */
		ERR_NETWORK = USER_ERROR + 1,

		/** The requested operation caused the socket to block */
		ERR_SOCKET_BLOCKING,

		/** Connection attempt failed because the remote host actively
		 * refused to accept the connection */
		ERR_CONNECTION_REFUSED,

		/** Connection attempt to remote host failed */
		ERR_CONNECTION_FAILED,

		/** Operation failed because socket is not connected. */
		ERR_CONNECTION_CLOSED,

		/** Timed out while attempting to connect to remote host */
		ERR_CONNECT_TIMED_OUT,

		/** Timed out while attempting to send data to remote host */
		ERR_SEND_TIMED_OUT,

		/** Timed out while attempting to receive data from remote host */
		ERR_RECV_TIMED_OUT,

		/** Timed out while attempting to accept an incoming connection request */
		ERR_ACCEPT_TIMED_OUT,

		/** While attempting set up a listening socket, the specified
		 * listening address+port was found to already be in use. */
		ERR_ADDRESS_IN_USE,

		/** No pending incoming connection requests on the listening socket. */
		ERR_NO_PENDING_CONNECTIONS,

		/** Permission to perform the requested operation was denied. */
		ERR_ACCESS_DENIED,

		/** Network went down */
		ERR_INTERNET_CONNECTION_CLOSED,

		/** The system is temporarily without network access */
		ERR_OUT_OF_COVERAGE,

		/** The remote host name could not be resolved to an address.
		 *
		 * While most systems will report this as an error from
		 * OpHostResolver, some systems can only resolve addresses as
		 * part of the socket connect stage.
		 */
		ERR_HOST_ADDRESS_NOT_FOUND

#if 0 // other things we might need.
		ERR_SECURE_SOCKET_ERROR_HANDLED, // (from OpSocket::ERROR_HANDLED)
		ERR_SECURE_CONNECTION_FAILED, // (from OpSocket::SECURE_CONNECTION_FAILED)
#endif
    };
};

#endif // OPNETWORKSTATUS_H
