/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_SOCKET_H
#define OP_SOCKET_H

#ifdef _EXTERNAL_SSL_SUPPORT_
#ifndef _STRUCT_CIPHERENTRY_
#define _STRUCT_CIPHERENTRY_
// convenience structure used to transfer selected ciphers to an external SSL handler
struct cipherentry_st
{
	uint8 id[2];
};
#endif // !_STRUCT_CIPHERENTRY_
#endif // _EXTERNAL_SSL_SUPPORT_

class OpSocketAddress;
class OpSocketListener;

#ifdef _EXTERNAL_SSL_SUPPORT_
#include "modules/util/adt/opvector.h"
class OpCertificate;
#endif

class SocketWrapper;

/** @short A socket, an endpoint for [network] communication.
 *
 * A socket is either a listening socket that accepts incoming remote
 * connections, or (more common) a client socket that connects to a
 * remote listening socket. Listening sockets will call Listen(),
 * while client sockets call Connect(). A socket is never both a
 * listening socket and a client socket, so calling Listen() and
 * Connect() on the same socket is not allowed.
 *
 * Socket connect/read/write operations are always non-blocking. Write
 * and connect operations are in addition asynchronous.
 *
 * This means that when attempting to receive data over the socket,
 * the implementation will only provide the caller with data already
 * in the operating system's network buffer, even if the caller
 * requested more data than what's currently available. Likewise, when
 * sending data over a socket, the implementation will just fill the
 * OS network buffer and return. If not all data could fit in the OS
 * network buffer, it will be sent later, when the OS has signalled
 * that it can accept more data for sending. Connecting to a remote
 * host is also handled asynchronously, meaning that the result of the
 * connection attempt is signalled to core via the listener.
 *
 * A socket object has a listener, which the platform code uses to
 * notify core about events (errors, connection established, data
 * ready for reading, data sent, etc.). Which listener method calls
 * are allowed from a given method in this class is documented
 * explicitly for each individual method. If nothing is mentioned
 * about allowed listener method calls in the documentation of a
 * method, it means that no listener method calls are allowed from
 * that method.
 *
 * When an out-of-memory condition has been reported by an OpSocket
 * object, it is in an undefined state, meaning that any further
 * operation, except for deleting it, is pointless.
 */
class OpSocket
{
public:
	/** Enumeration of possible socket errors. */
	enum Error
	{
		/** No error */
		NETWORK_NO_ERROR,

		/** Secure socket error handled */
		ERROR_HANDLED,

		/** Network link error */
		NETWORK_ERROR,

		/** The requested operation caused the socket to block */
		SOCKET_BLOCKING,

		/** Connection attempt failed because the remote host actively
		 * refused to accept the connection */
		CONNECTION_REFUSED,

		/** Connection attempt to remote host failed */
		CONNECTION_FAILED,

		/** Operation failed because socket is not connected. */
		CONNECTION_CLOSED,

		/** Timed out while attempting to connect to remote host */
		CONNECT_TIMED_OUT,

		/** Timed out while attempting to send data to remote host */
		SEND_TIMED_OUT,

		/** Timed out while attempting to receive data from remote host */
		RECV_TIMED_OUT,

		/** While attempting set up a listening socket, the specified
		 * listening address+port was found to already be in use. */
		ADDRESS_IN_USE,

		/** Permission to perform the requested operation was denied. */
		ACCESS_DENIED,

		/** Secure socket connection attempt failed */
		SECURE_CONNECTION_FAILED,

		/** Network went down */
		INTERNET_CONNECTION_CLOSED,

		/** The system is temporarily without network access */
		OUT_OF_COVERAGE,

		/** Unused */
		NO_SOCKETS,

		/** The remote host name could not be resolved to an address.
		 *
		 * While most systems will report this as an error from
		 * OpHostResolver, some systems can only resolve addresses as
		 * part of the socket connect stage.
		 */
		HOST_ADDRESS_NOT_FOUND,

		/** Out of memory.
		 *
		 * Should only be used when an asynchronous operation runs out
		 * of memory. In other cases, return OpStatus::ERR_NO_MEMORY
		 * from the method that ran out of memory is the right thing
		 * to do.
		 */
		OUT_OF_MEMORY
	};

#ifdef _EXTERNAL_SSL_SUPPORT_
	/* Enumeration of possible SSL connection errors returned by OpSocket::GetSSLConnectErrors() */
	enum SSLError
	{
		SSL_NO_ERROR = 0,

		CERT_BAD_CERTIFICATE = 1,

		CERT_DIFFERENT_CERTIFICATE = 2,

		CERT_INCOMPLETE_CHAIN = 4,

		CERT_EXPIRED = 8,

		CERT_NO_CHAIN = 16
	};
#endif // _EXTERNAL_SSL_SUPPORT_

private:
	/** Create and return an OpSocket object.
	 *
	 * @param socket (output) Set to the socket created
	 * @param listener The listener of the socket (see
	 * OpSocketListener)
	 * @param secure TRUE if the socket is to be secure, meaning that an
	 * SSL connection is to be established when Connect() is called.
	 * Note: This parameter will always be FALSE when using Opera's internal SSL
	 * support. If this is to be a listening socket, blank sockets passed as a
	 * parameter to Accept() will inherit the secureness of this
	 * socket. If this is to be such a blank socket, specified secureness
	 * is in other words overridden by the secureness of the listening
	 * socket. If the platform implementation cannot honor the requested
	 * secureness, OpStatus::ERR_NOT_SUPPORTED must be returned. If secure is
	 * TRUE and the secure connection failes,
	 * OpSocketListener::OnSocketConnectError(SECURE_CONNECTION_FAILED) must
	 * be called. Core will then call GetSSLConnectErrors() for more
	 * detailed errors.  Depending on the certificate errors
	 * core will call the platform callback OpSSLListener::OnCertificateBrowsingNeeded()
	 * which should trigger a dialog for the user to Accept/Deny access to
	 * the server.
	 *
	 * NOTE: this method should only be called by SocketWrapper::CreateTCPSocket.  
	 * All core code should use SocketWrapper::CreateTCPSocket instead of calling 
	 * this directly.  
	 *
	 * @return ERR_NO_MEMORY, ERR_NOT_SUPPORTED, ERR or OK
	 */
	static OP_STATUS Create(OpSocket** socket, OpSocketListener* listener, BOOL secure=FALSE);

		// This is the only class that should call OpSocket::Create
	friend class SocketWrapper;

public:
	/** Destructor.
	 *
	 * If there is an active connection, attempt to close it
	 * gracefully, unless SetAbortiveClose() has been called.
	 */
	virtual ~OpSocket() {}

	/** Connect a socket to a given address.
	 *
	 * The listener (OpSocketListener) is to be called to provide
	 * feedback (connection established, connection failed) to the
	 * caller.
	 *
	 * A connection attempt is initiated by calling this method. It is
	 * concluded when OnSocketConnected() or OnSocketConnectError() is
	 * called, or when this method returns an error value.
	 *
	 * OnSocketConnected() is called as soon as the connection attempt
	 * succeeds. Then the socket is connected, and core should expect
	 * the listener methods OnSocketDataReady() and OnSocketClosed()
	 * to be called any time.
	 *
	 * Opera sockets are non-blocking, meaning that if the platform
	 * side cannot succeed or fail connecting immediately,
	 * OpStatus::OK is returned, and then core will be notified later
	 * via the listener how the connection attempt went. Otherwise,
	 * OnSocketConnectError() will be called with an error code.
	 *
	 * If the connection attempt succeeded, from now on the
	 * implementation will automatically call the listener method
	 * OnSocketDataReady() when data arrives and OnSocketClosed() when
	 * the remote end closes the connection.
	 *
	 * A socket is never both a listening socket and a client socket,
	 * so calling Listen() and Connect() on the same socket is not
	 * allowed.
	 *
	 * Allowed error codes to pass via the listener triggered by
	 * calling this method: NETWORK_ERROR, CONNECTION_REFUSED,
	 * CONNECTION_FAILED, CONNECT_TIMED_OUT,
	 * INTERNET_CONNECTION_CLOSED, OUT_OF_COVERAGE,
	 * HOST_ADDRESS_NOT_FOUND and SECURE_CONNECTION_FAILED (when
	 * an external ssl library is used. In case of secure
	 * socket, OnSocketConnected() can only be called when the socket
	 * is connected and the ssl connection has been established).
	 *
	 * Allowed listener methods to call triggered by calling this
	 * method: OnSocketConnected(), OnSocketConnectError(). The
	 * listener methods may be called after this method has returned,
	 * but only if it returned OpStatus::OK.
	 *
	 * @param socket_address A socket address. The address must be
	 * valid - i.e. socket_address->IsAddressValid() must return TRUE
	 * if called. Port number may not be 0. The implementation must
	 * not use this socket address object after this method has
	 * returned.
	 *
	 * @return OK if no immediate error. OK does not mean that the
	 * connection attempt was successful, unless OnSocketConnected() of
	 * the listener has already been called (it may just mean "nothing
	 * wrong so far"). ERR is returned when the connection attempt failed
	 * instantly - hopefully preceded by a call to OnSocketConnectError()
	 * of the listener (or core will not be able to tell why it
	 * failed). ERR_NO_MEMORY is returned on OOM.
	 */
	virtual OP_STATUS Connect(OpSocketAddress* socket_address) = 0;

	/** Send data over the socket.
	 *
	 * The listener (OpSocketListener) is to be called to provide
	 * feedback to the caller. Feedback includes progress information
	 * and errors.
	 *
	 * Opera sockets are non-blocking, meaning that if not all data
	 * could be sent immediately, an implementation shall keep unsent
	 * data, return, and attempt to send it later, when the OS is
	 * ready to accept more data. Listener methods may be called any
	 * time here. OnSocketDataSent() is to be called every time some
	 * data could be sent to the OS successfully.
	 *
	 * Call OnSocketSendError() when errors related to sending
	 * occur. One common error may be that Send() is called too soon
	 * after a previous call, so that the implementation hasn't been
	 * able to deliver all data to the OS yet. In that case, call
	 * OnSocketSendError() with SOCKET_BLOCKING, and return
	 * OpStatus::ERR, so that the caller may try again later. The
	 * caller should wait for another call to OnSocketDataSent()
	 * before retrying.
	 *
	 * Allowed error codes to pass via the listener triggered by
	 * calling this method: NETWORK_ERROR, SEND_TIMED_OUT,
	 * INTERNET_CONNECTION_CLOSED, OUT_OF_COVERAGE.
	 *
	 * Allowed listener method to call triggered by calling this method:
	 * OnSocketSendError(). The listener method may be called after this
	 * method has returned, but only if it returned OpStatus::OK.
	 *
	 * This method must be re-entrancy safe. However, when it is
	 * re-entered, the implementation has to signal a SOCKET_BLOCKING
	 * error when reaching some recursion level (so that the we don't run
	 * out of stack). Doing this already at the first re-entry is
	 * recommended, but not required if the platform handles deep
	 * recursions well.
	 *
	 * This method may not be called on a closed socket, i.e. when
	 * OnSocketClosed() has already been called, or before OnSocketConnected()
	 * has been called.
	 *
	 * @param data The data to send. The caller is free to re-use the data
	 * buffer for other purposes as soon as this method returns. In other
	 * words: If not all data can be sent immediately, the platform must
	 * make a copy of the unsent data, since the buffer passed in this
	 * parameter may have been overwritten with other data before the
	 * platform is ready to send the remaining data.
	 * @param length The length of the data
	 *
	 * @return OK if no error (meaning: "data is (or will be) sent").
	 * Return ERR_NO_MEMORY on OOM. Return ERR on serious errors that
	 * cannot be expressed via OnSocketSendError() of
	 * OpSocketListener. Returning ERR means that the data passed was
	 * not accepted, so that the caller needs to resend (it if it
	 * doesn't just want to give up).
	 */
	virtual OP_STATUS Send(const void* data, UINT length) = 0;

	/** Receive data over the socket.
	 *
	 * The listener (OpSocketListener) is to be called to provide
	 * feedback (read errors, connection closed) to the caller.
	 *
	 * This is a non-blocking call, meaning that the implementation may not
	 * wait for additional data to arrive over the network before it returns;
	 * it will simply fill the buffer with what is already in the operating
	 * system's input buffer (which may well be empty, which is not an error
	 * condition).
	 *
	 * Allowed error codes to pass via the listener when calling this
	 * method: NETWORK_ERROR, SOCKET_BLOCKING, RECV_TIMED_OUT,
	 * INTERNET_CONNECTION_CLOSED, OUT_OF_COVERAGE.
	 *
	 * Allowed listener method to calls triggered by calling this method:
	 * OnSocketReceiveError() and OnSocketClosed(). Only to be called
	 * synchronously (before this method returns). OpStatus::ERR is always
	 * returned from this method if OnSocketReceiveError() was called. Calling
	 * OnSocketClosed() does not signify an error; it just means that the
	 * connection was closed by the remote endpoint while the local endpoint
	 * tried to read data. *bytes_received is set to 0 and OpStatus::OK is
	 * returned in this case.
	 *
	 * This method may not be called on a closed socket, i.e. when
	 * OnSocketClosed() has already been called, or before OnSocketConnected()
	 * has been called.
	 *
	 * @param buffer (output) A buffer - the buffer to write received data
	 * into. Must be ignored by the caller unless OpStatus::OK is returned.
	 * @param length Maximum number of bytes to read
	 * @param bytes_received (output) Set to the number of bytes actually
	 * read. Must be ignored by the caller unless OpStatus::OK is returned.
	 *
	 * @return OK, ERR or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received) = 0;

	/** Close the socket.
	 *
	 * Allowed listener method to call triggered by calling this method:
	 * OnSocketCloseError(). Only to be called synchronously (before this
	 * method returns).
	 */
	virtual void Close() = 0;

	/** Get the address of the remote end of the socket connection.
	 *
	 * @param socket_address a socket address to be filled in
	 */
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address) = 0;

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	/** Get the address of the local end of the socket connection.
	 *
	 * @param socket_address a socket address to be filled in
	 */
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address) = 0;
#endif // OPSOCKET_GETLOCALSOCKETADDR

	/** Set or change the listener for this socket.
	 *
	 * @param listener New listener to set.
	 */
	virtual void SetListener(OpSocketListener* listener) = 0;

#ifdef SOCKET_LISTEN_SUPPORT
# ifndef DOXYGEN_DOCUMENTATION
	/** @deprecated Use Listen(OpSocketAddress*, int) instead. */
	DEPRECATED(virtual OP_STATUS Bind(OpSocketAddress* socket_address)); // inlined below

	/** @deprecated Use Listen(OpSocketAddress*, int) instead. */
	DEPRECATED(virtual OP_STATUS SetLocalPort(UINT port)); // inlined below

	/** @deprecated Use Listen(OpSocketAddress*, int) instead. */
	DEPRECATED(virtual OP_STATUS Listen(UINT queue_size)); // inlined below
# endif // !DOXYGEN_DOCUMENTATION

	/** Set the socket to listen for incoming connections on a given address+port.
	 *
	 * Binds the socket to a local address+port and creates a queue to hold
	 * incoming connections, which can be married with blank sockets using
	 * Accept(). Once a listen queue has been created, clients' connection
	 * requests will be appended until the queue is full, at which point it
	 * will reject any incoming connection requests.
	 *
	 * A socket is never both a listening socket and a client socket,
	 * so calling Listen() and Connect() on the same socket is not
	 * allowed.
	 *
	 * Allowed error codes to pass via the listener when calling this
	 * method: NETWORK_ERROR, ADDRESS_IN_USE, ACCESS_DENIED
	 *
	 * Allowed listener method to call triggered by calling this method:
	 * OnSocketListenError() and OnSocketConnectionRequest().
	 *
	 * OnSocketListenError() is only to be called synchronously (before this
	 * method returns). OpStatus::ERR is always returned from this method if
	 * OnSocketListenError() was called. The error code ADDRESS_IN_USE is used
	 * when attempting to use an address+port to which another listening socket
	 * has already been bound. ACCESS_DENIED is used when attempting to use an
	 * address+port to which the user doesn't have permission (e.g. port
	 * numbers less than 1024 on UNIX systems). NETWORK_ERROR may be used in
	 * other, network related, error situations. For other error situations, no
	 * listener method will be called.
	 *
	 * OnSocketConnectionRequest() is only to be called asynchronously (after
	 * this method has returned), whenever there are pending connection
	 * requests in the queue.
	 *
	 * @param socket_address A local socket address. The actual address part
	 * may specify on which network interface (loopback, ethernet interface 1,
	 * ethernet interface 2, etc.) to bind the socket, if the system provides
	 * multiple network interfaces. If no address is set (i.e. if
	 * socket_address->IsAddressValid() would return FALSE if called), the
	 * default network interface on the system will be used. The port number
	 * must always be set. The implementation must not use this socket address
	 * object after this method has returned.
	 * @param queue_size Maximum number of incoming connection requests in the
	 * queue
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors (a
	 * listener method may or may not have been called if ERR is returned).
	 */
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size) = 0;

	/** Accept a pending socket connection request to the listening socket.
	 *
	 * Prior to this call, a successful call to Listen() must have taken place.
	 *
	 * This method extracts the first pending connection request from the queue
	 * created by Listen() and connects it to the parameter-specified socket.
	 *
	 * Allowed error codes to pass via the listener triggered by calling this
	 * method: SOCKET_BLOCKING, NETWORK_ERROR, CONNECT_TIMED_OUT,
	 * INTERNET_CONNECTION_CLOSED, OUT_OF_COVERAGE.
	 *
	 * Allowed listener method to call triggered by calling this method:
	 * OnSocketConnectError(). Only to be called synchronously (before this
	 * method returns). OpStatus::ERR is always returned from this method if
	 * OnSocketConnectError() was called. The error code SOCKET_BLOCKING is
	 * used if there were no pending connection requests in the queue. The
	 * parameter-specified socket may be re-used in a future call to Accept()
	 * if this happened.
	 *
	 * @param socket (out) A socket to represent the accepted connection.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR for other errors (a
	 * listener method may or may not have been called if ERR is returned). If
	 * OK is returned, the parameter-specified socket is ready for use (Recv(),
	 * Send(), Close(), ...).
	 */
	virtual OP_STATUS Accept(OpSocket* socket) = 0;
#endif // SOCKET_LISTEN_SUPPORT

#ifdef _EXTERNAL_SSL_SUPPORT_
	/** Upgrade the TCP connection to a secure connection.
	 *
	 * This function is only called by core when an SSL connection is
	 * established over a HTTP proxy connection.
	 *
	 * Keep the TCP connection open, and start establishing SSL on the
	 * connection. Call OpSocketListener::OnSocketConnected() or
	 * OpSocketListener::OnSocketConnectError(SECURE_CONNECTION_FAILED) when
	 * done.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS UpgradeToSecure() = 0;

	/** Initialize the ciphers and protocol version of a secure socket.
	 *
	 * Note: this only works for SSL v3 and newer
	 *
	 * @param ciphers An array of structures holding the two element array with
	 * the cipher specification selected for this socket
	 * @param cipher_count The number of elements in the cipher array protocol
	 * support
	 *
	 * Note that this call can be ignored if it is not possible to set the
	 * ciphers used by the external SSL library.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count) = 0;

	/** Accept insecure certificate.
	 *
	 * When OpSocketListener::OnSocketConnectError() has been called, core will
	 * call GetSSLConnectErrors(). If GetSSLConnectErrors() reports a
	 * certificate error, core will call ExtractCertificate() and ask user to
	 * accept/deny. If the certificate is accepted, this function will be
	 * called.
	 *
	 * @param certificate The certificate to be accepted. It must be checked
	 * that this is the same certificate as given by ExtractCertificate on this
	 * socket.
	 *
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if certificate is
	 * not the same as current certificate.
	 */
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate) = 0;

	/** Get the current cipher, TLS protocol version and Public Key length in bits.
	 *
	 * @param used_cipher Return the current SSL/TLS cipher ID in this
	 * structure
	 * @param tls_v1_used Return a SSL/TLS version indicator here
	 * (TRUE if it is TLS v1, FALSE if SSL v3)
	 * @param pk_keysize Return the Public Key cipher's key length (in
	 * bits) here. If the cipher length is unavailable return 0.
	 *
	 * @return OK if the cipher was successfully retrieved, ERR_NO_MEMORY on OOM.
	 */
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize) = 0;

	/** Set the host name.
	 *
	 * This is used to test the validity of a certificate
	 *
	 * @param server_name The name of the server.
	 *
	 * @return OpStatus::OK if servername was successfully set,
	 * OpStatus::ERR_NO_MEMORY if memory allocation error occurred
	 */
	virtual OP_STATUS SetServerName(const uni_char* server_name) = 0;

	/** Get the server certificate used for secure socket.
	 *
	 * @return The SSL/TLS server certificate wrapper. The certificate is owned
	 * and must be deleted by caller. Returns NULL if memory error.
	 */
	virtual OpCertificate *ExtractCertificate() = 0;

	/** Get the server full certificate chain used for secure socket.
	 *
	 * @return The SSL/TLS server certificate wrapper. The certificate chain is
	 * owned and must be deleted by caller. The chain starts with the server
	 * certificate and ends with the top CA certificate. Returns NULL if memory
	 * error.
	 */
	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain() = 0;

	/** Get SSL connection errors.
	 *
	 * Returns the SSL errors that caused the call to OnSocketConnectError()
	 * with error SECURE_CONNECTION_FAILED.
	 *
	 * @return The errors - Bit array of ORed errors as enumerated in
	 * OpSocket::SSLError
	 */
	virtual int GetSSLConnectErrors() = 0;
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	/** Inform the socket layer that an abortive close is required.
	 *
	 * To avoid unwanted re-transmissions in the TCP layer after closing a
	 * socket for e.g. a timed out HTTP connection, it is necessary to signal
	 * that the socket close should happen abortively. If this is signalled to
	 * the socket implementation the platform layer should do its best to shut
	 * down the socket immediately, resetting the socket connection if
	 * necessary.
	 */
	virtual void SetAbortiveClose() = 0;
#endif // SOCKET_NEED_ABORTIVE_CLOSE

#ifdef SOCKET_SUPPORTS_TIMER
	/** Change the socket idle timeout.
	 *
	 * Initial state of a socket is the default timeout value.
	 *
	 * Using an idle timer, after a certain time of socket inactivity, the
	 * socket may disconnect (or, if not yet connected, cancel the connection
	 * attempt), and report this situation using OpSocketListener. If the idle
	 * timer expires while the socket is attempting to connect to the remote
	 * host, OnSocketConnectError(CONNECT_TIMED_OUT) is called.
	 *
	 * @param seconds Number of seconds of socket inactivity to allow before
	 * disconnecting the socket. If this value is 0, there will be no idle
	 * timeout. This parameter is ignored if the 'restore_default' parameter is
	 * TRUE.
	 * @param restore_default If TRUE, ignore the 'seconds' parameter and use
	 * system default timeout interval. This is the initial state of a socket.
	 */
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE) = 0;
#endif // SOCKET_SUPPORTS_TIMER

#ifdef OPSOCKET_PAUSE_DOWNLOAD
	/** Pause data reception.
	 *
	 * Temporarily stop receiving data from the socket. While loading is
	 * paused, an OpSocket will attempt to stop receiving data, and, more
	 * importantly, not notify its listener even if there's more data available
	 * for reading. No data must be lost because of a pause. A user of this
	 * feature should be aware that pausing for too long may cause the remote
	 * end to time out and close the connection.
	 *
	 * @return OK if no error
	 *
	 * @see ContinueRecv()
	 */
	virtual OP_STATUS PauseRecv() = 0;

	/** Resume data reception.
	 *
	 * The counterpart to the PauseRecv() method. It resumes data
	 * reception after a pause.
	 *
	 * @return OK if no error
	 *
	 * @see PauseRecv()
	 */
	virtual OP_STATUS ContinueRecv() = 0;
#endif // OPSOCKET_PAUSE_DOWNLOAD

#ifdef OPSOCKET_OPTIONS
	/**
	 * Enumeration of socket options that can be enabled/disabled using OpSocket::SetSocketOption().
	 */
	enum OpSocketBoolOption
	{
		/**
		 * Disables Nagle's algorithm for TCP sockets.
		 *
		 * If a program writes a single byte to a socket it would be
		 * incredibly inefficient to attach a full IP + TCP header to
		 * that and send it a cross the network immediately. To avoid
		 * this OS network stacks will typically buffer the written
		 * data until either A) more data to be sent has arrived, or
		 * B) some amount of time has passed at which point the OS has
		 * no choice but to send the inefficient packet anyway.
		 * For protocols that rely on roundtriping very small packages
		 * over TCP at very low latency this behavior can incur a
		 * performance penalty and then this socket option can be
		 * used to force the OS the always send the data over the
		 * wire immediate when it arrives to the network stack.
		 *
		 * When this socket option is enabled it's a good idea to
		 * try and send() full packets in one go if possible.
		 *
		 * This option applies only to TCP sockets and cannot / must
		 * not be enabled or disabled for UDP sockets.
		 *
		 * See also: http://en.wikipedia.org/wiki/Nagle's_algorithm
		 */
		SO_TCP_NODELAY
	};

	/**
	 * Configure socket behavior.
	 *
	 * Allows the user to opt into non-standard socket behavior,
	 * see OpSocketOption for information on specific options.
	 * Options may exists at various protocol levels, e.g. IP, TCP etc.
	 *
	 * NOTE: This method may only be called while the socket is connected,
	 *       i.e. during or after OnConnected() is called on the listener
	 *       and not after the socket is closed.
	 *
	 *
	 * @param option See @ref OpSocketOption.
	 * @param value indicates whether this socket option is to be turned on or off.
	 * @return OK if the socket option was applied without errors
	 * @return ERR_OUT_OF_RANGE if invalid parameters were passed via "value" or "value_len"
	 * @return ERR for all other errors, including network errors etc.
	 * @return ERR_BAD_FILE_NUMBER if the socket object or subsystem is invalid,
	 *         this error should be considered a bug in the calling code or
	 *         in the socket platform implementation.
	 */
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) = 0;

	/**
	 * Get socket configuration.
	 *
	 * Allows the user to see if a certain socket option is currently activated or not.
	 * See OpSocketOption for information on specific options.
	 * Options may exists at various protocol levels, e.g. IP, TCP etc.
	 *
	 * NOTE: This method may only be called while the socket is connected,
	 *       i.e. during or after OnConnected() is called on the listener
	 *       and not after the socket is closed.
	 *
	 * @param option See @ref OpSocketOption.
	 * @param out_value out parameter where a boolean value is stored indicating
	 *                  whether this socket option is currently turned on or off.
	 * @return OK if the socket option was read without errors.
	 * @return ERR_OUT_OF_RANGE if invalid parameters were passed via "value" or "value_len".
	 * @return ERR for all other errors, including network errors etc.
	 * @return ERR_BAD_FILE_NUMBER if the socket object or subsystem is invalid,
	 *         this error should be considered a bug in the calling code or
	 *         in the socket platform implementation.
	 */
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) = 0;
#endif // OPSOCKET_OPTIONS
};

#ifdef SOCKET_LISTEN_SUPPORT
# ifndef DOXYGEN_DOCUMENTATION
inline OP_STATUS OpSocket::Bind(OpSocketAddress* socket_address) { return OpStatus::ERR; }
inline OP_STATUS OpSocket::SetLocalPort(UINT port) { return OpStatus::ERR; }
inline OP_STATUS OpSocket::Listen(UINT queue_size) { return OpStatus::ERR; }
# endif // DOXYGEN_DOCUMENTATION
#endif // SOCKET_LISTEN_SUPPORT

/** Socket listener - implemented in core code, called from platform code.
 *
 * An object of this class is notified about any state changes or events that
 * occur on the socket to which it subscribes.
 *
 * Note: Unless explicitly mentioned, any method call on an object of
 * this class may cause the calling socket to be deleted. Therefore,
 * make sure that there is nothing on the call stack referencing the
 * socket after the listener method has returned.
 *
 * As an object could conceivably observe more than one socket at a
 * time, sockets calling these methods pass a pointer to the
 * signalling socket.
 */
class OpSocketListener
{
public:
	virtual ~OpSocketListener() {}

	/** The socket has connected to a remote host successfully.
	 *
	 * @param socket The socket which has connected
	 */
	virtual void OnSocketConnected(OpSocket* socket) = 0;

	/** There is data ready to be received on the socket.
	 *
	 * This listener method is called (typically from the event loop on
	 * the platform side) as long as there is any data at all waiting to
	 * be read. There doesn't necessarily have to be any new data since
	 * the previous call. This means that core should read some (as much
	 * as possible) data (OpSocket::Recv()) when this method is called, to
	 * prevent it from throttling.
	 *
	 * @param socket The socket that is ready for reading
	 */
	virtual void OnSocketDataReady(OpSocket* socket) = 0;

	/** Data has been sent on the socket.
	 *
	 * As a response, core may want to send (OpSocket::Send()) more data,
	 * if there is any left to send. It is legal to do this
	 * immediately. The platform is responsible for avoiding infinite
	 * recursion.
	 *
	 * @param socket The socket that has sent data
	 * @param bytes_sent The number of bytes sent
	 */
	virtual void OnSocketDataSent(OpSocket* socket, UINT bytes_sent) = 0;

	/** The socket has closed.
	 *
	 * This informs core that there is no more data to be read. Core should not
	 * try to read or write on a closed socket. Platform code needs to make
	 * sure that all data has been received (with Recv()) before calling this
	 * listener method.
	 *
	 * @param socket The socket that was closed.
	 */
	virtual void OnSocketClosed(OpSocket* socket) = 0;

#ifdef SOCKET_LISTEN_SUPPORT
	/** A listening socket has received an incoming connection request.
	 *
	 * A typical response to this method call is to call socket->Accept().
	 *
	 * This listener method is called (typically from the event loop on the
	 * platform side) as long as there are any pending connection requests at
	 * all in the queue. There doesn't necessarily have to be any new
	 * connection requests since the previous call. This means that core should
	 * attempt to accept as many connections as possible (at least one) when
	 * this method is called, to prevent it from throttling.
	 *
	 * @param socket listening socket from which the request is coming
	 */
	virtual void OnSocketConnectionRequest(OpSocket* socket) = 0;

	/** An error has occured on the socket whilst setting it up for listening.
	 *
	 * @param socket The socket on which the error has occurred
	 * @param error The error - as enumerated in OpSocket.
	 */
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) = 0;
#endif // SOCKET_LISTEN_SUPPORT

	/** An error has occured on the socket whilst connecting.
	 *
	 * This method is also called when accepting a new incoming connection on a
	 * listening socket fails.
	 *
	 * @param socket The socket on which the error has occurred
	 * @param error The error - as enumerated in OpSocket.
	 */
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error) = 0;

	/** An error has occured on the socket whilst receiving.
	 *
	 * @param socket The socket on which the error has occurred
	 * @param error The error - as enumerated in OpSocket.
	 */
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error) = 0;

	/** An error has occured on the socket whilst sending.
	 *
	 * @param socket The socket on which the error has occurred
	 * @param error The error - as enumerated in OpSocket.
	 */
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error) = 0;

	/** An error has occured on the socket whilst closing.
	 *
	 * @param socket The socket on which the error has occurred
	 * @param error The error - as enumerated in OpSocket.
	 */
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error) = 0;

#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
	/** Data has been sent.
	 *
	 * This method is soon to be removed.
	 *
	 * @param socket The socket on which the data has been sent
	 * @param bytes_sent Number of bytes sent
	 */
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent) { }
#endif // !URL_CAP_TRUST_ONSOCKETDATASENT
};

#ifdef PI_INTERNET_CONNECTION
# include "modules/url/url_socket_wrapper.h"
#endif // PI_INTERNET_CONNECTION

#endif	//	!OP_SOCKET_H
