/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * See: unix-net/unixsocket.cpp
 */
/* TODO: make Listen/Accept and Connect mutually exclusive.
 * TODO: support connect()ion time-outs, c.f. bug 278290; maybe in posix_selector.cpp
 * TODO: profiling
 */

#include "core/pch.h"
#ifdef POSIX_OK_SOCKET
#include "platforms/posix/posix_selector.h"				// PosixSelector
#include "platforms/posix/src/posix_async.h"			// PosixAsyncManager::...
#include "platforms/posix/net/posix_network_address.h"	// PosixNetworkAddress
#include "platforms/posix/net/posix_base_socket.h"		// PosixBaseSocket
#ifdef THREAD_SUPPORT
#include "platforms/posix/posix_mutex.h"				// PosixMutex
#endif

#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/opdata/OpData.h"

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
# ifndef OPSOCKET_PAUSE_DOWNLOAD
#warning "You need to import API_PI_PAUSE_DOWNLOAD (and change any #if-ery testing on TCP_PAUSE_DOWNLOAD_EXTENSION to test OPSOCKET_PAUSE_DOWNLOAD instead)."
#define OPSOCKET_PAUSE_DOWNLOAD
# endif
#endif

#ifdef OPSOCKET_OPTIONS
#include <netinet/tcp.h>

# if defined(SOL_TCP)
#define POSIX_TCP_LEVEL SOL_TCP // Linux
# elif defined(IPPROTO_TCP)
#define POSIX_TCP_LEVEL IPPROTO_TCP // OSX/BSD
# else
#error "Neither SOL_TCP nor IPPROTO_TCP is defined- see CORE-42196"
# endif // SOL_TCP

#endif // OPSOCKET_OPTIONS


#if defined(POSIX_OK_SIGNAL) && defined(_DEBUG) && !defined(_NO_GLOBALS_) && !defined(VALGRIND)
#define LOG_SIG_PIPE
#include "platforms/posix/posix_signal_watcher.h"
# ifdef ENABLE_MEMORY_DEBUGGING
#include "modules/memory/src/memory_memguard.h"
# endif
class SigPipeLogger : public PosixSignalListener, public PosixLogger
{
	static SigPipeLogger *g_instance; // debug-only object: gets leaked
	SigPipeLogger() : PosixLogger(SOCKET) { OP_ASSERT(g_instance == 0); }
public:
	~SigPipeLogger() { OP_ASSERT(g_instance == this); g_instance = 0; }
	void OnSignalled(int signum)
		{ OP_ASSERT(signum == SIGPIPE); Log(NORMAL, "Caught SIGPIPE.\n"); }
	static void PipeWatch()
	{
		if (g_instance == 0)
		{
			g_instance = OP_NEW(SigPipeLogger, ());
			if (g_instance &&
				OpStatus::IsError(PosixSignalWatcher::Watch(SIGPIPE, g_instance)))
				OP_DELETE(g_instance);
#ifdef ENABLE_MEMORY_DEBUGGING
			else
				// Exempt the allocation from leak reporting
				g_mem_guard->Action(MEMORY_ACTION_SET_MARKER_M4_SINGLE, g_instance);
#endif
		}
	}
} *SigPipeLogger::g_instance = 0;
#endif // LOG_SIG_PIPE

/** OpSocket implementation using POSIX APIs.
 *
 * This implementation currently simply assumes the network is available.  Later
 * versions may support management of potentially several network links.
 *
 * This implementation does not support use before g_opera->InitL() or after
 * g_opera->Destroy().
 *
 * The implementations of old Bind, SetLocalPort and Listen only set up data on
 * this object: the actual work (including calling bind) is done by the new
 * Listen method, which replaces these deprecated old methods.
 */
class PosixSocket : public OpSocket, public PosixBaseSocket
{
	PosixSocket(OpSocketListener* listener /* , bool ignored */);
	static Error DecodeErrno(int err);

	OpSocketListener* m_ear;
	PosixNetworkAddress m_remote;
#if defined(SOCKET_LISTEN_SUPPORT) || defined(OPSOCKET_GETLOCALSOCKETADDR)
	PosixNetworkAddress m_local;

	/** Set local address's host part to default local system IP address. */
	OP_STATUS ClearLocal();
	OP_STATUS SetLocalInterface(OpSocketAddress* socket_address);
#endif // SOCKET_LISTEN_SUPPORT || OPSOCKET_GETLOCALSOCKETADDR
	OP_STATUS SetLocalInterface(int fd);

	class ConnectListener : public PosixSelectListener, public PosixLogger
	{
	private:
		PosixSocket *const m_boss;
		int m_fd;
		PosixSelector::Type m_mode;
#ifdef OPSOCKET_PAUSE_DOWNLOAD
		bool m_paused;
		bool m_pending_disconnect;
#endif
		bool m_connected;

		/** This attribute is increased/decreased inside WriteSome() (e.g.,
		 * when being called by PosixSocket::Send()). If the counter is too big
		 * (larger than MAX_SEND_RECURSIONS) WriteSome() returns without trying
		 * to write some data. If WriteSome() successfully wrote some data, it
		 * notifies OpSocketListener::OnSocketDataSent(), which in turn may call
		 * PosixSocket::Send() again, which then calls WriteSome() - with this
		 * counter we avoid a too deep recursion. */
		unsigned int m_sending;
		static const unsigned int MAX_SEND_RECURSIONS = 5;

		void SetConnected(bool toggle) { m_connected = toggle; }
		bool IsConnected() { return m_connected; }

	public:
		ConnectListener(PosixSocket *boss, int fd, bool connected = false);
		virtual ~ConnectListener();

#ifdef OPSOCKET_PAUSE_DOWNLOAD
		void Pause();
		void Resume();
		bool IsPaused() const { return m_paused; }
		// Called when we want to Disconnect(), in case we shouldn't yet.
		bool DisconnectBlocked();
#else
		bool IsPaused() const { return false; }
#endif
		ssize_t Peek();
		ssize_t Receive(void *buffer, size_t length);
		ssize_t Send(const void *buffer, size_t length);

		// Package access to g_posix_selector and m_mode:
		OP_STATUS Listen(int fd, const class OpSocketAddress *connect=0);
		void Snooze();

		/** Turn on or off one (or both) of the select call-backs.
		 *
		 * Qt used to send many "read ready" messages when only one was needed;
		 * and core has been known (e.g. DSK-269032 for write) to ignore the
		 * information that a socket is ready.  We only want to tell core once.
		 * So the socket needs to stop listening for more reads or writes
		 * between being told that it's ready and getting a fresh Recv() or
		 * Send().  This method tracks which modes are currently needed and
		 * ensures that we're listening for those.
		 *
		 * @param enable Will turn on call-backs if true, turn them off if false.
		 * @param mode The types of call-back to enable or disable.
		 */
		void SetMode(bool enable, PosixSelector::Type mode);

#ifdef OPSOCKET_OPTIONS
		OP_STATUS SetOption(OpSocketBoolOption option, BOOL value);
		OP_STATUS GetOption(OpSocketBoolOption option, BOOL& value);
#endif // OPSOCKET_OPTIONS

		// PosixSelectListener API:
		virtual void OnError(int fd, int err);
		virtual void OnDetach(int fd);
		virtual void OnWriteReady(int fd);
		virtual void OnReadReady(int fd);
		virtual void OnConnected(int fd);

		/** Writes as many bytes as possible from the m_boss' m_outgoing to
		 * the socket in non-blocking mode. If writing would block, this method
		 * returns. Written data is removed from m_outgoing. */
		void WriteSome();

	} * m_connect;

	friend class ConnectListener;

	/** Disconnect socket.
	 *
	 * Clears away infrastructure related to being connected.
	 */
	void Disconnect();

	void OnReadReady(int fd, bool force=false);
	void OnConnected(int fd);
	void OnError(int err);
	void OnConnectError(int err);
	void OnReadError(Error error);
	void OnWriteError(Error error);

	/** Send()ing data.
	 *
	 * We accept data instantly and buffer it up here for asynchronous writing
	 * by ConnectListener::WriteSome(). */
	OpData m_outgoing;

#ifdef SOCKET_LISTEN_SUPPORT
	class ListenListener : public SocketBindListener
	{
		// NB: base class destructor detaches this, so we don't need to think about it.
		PosixSocket *const m_boss;
		int m_fd;
	public:
		ListenListener(PosixSocket* boss, int fd);
		~ListenListener();
		OP_STATUS Accept(OpSocket *socket);

		// PosixSelectListener API (partial: see PosixBaseSocket::SocketBindListener):
		virtual void OnError(int fd, int err);
		virtual void OnDetach(int fd);
		virtual void OnReadReady(int fd); // Ready to accept().
	} *m_listen;

	friend class ListenListener;
	void OnAcceptReady();
	void EndListen();
	void OnListenError(int err);
	OP_STATUS DecodeListenError(int err);
#endif // SOCKET_LISTEN_SUPPORT

	// const bool m_secure; // TODO: what difference *should* this make ?

	friend class OpSocket;
	void ClearSender();

	bool SetSocketFlags(int fd, int *err); // extends PosixBaseSocket's version
	OP_STATUS DecodeSetupError(int err, const char* fname);

public:
	virtual ~PosixSocket();
	virtual void SetListener(OpSocketListener* listener);
	virtual OP_STATUS Connect(OpSocketAddress* socket_address);
	virtual OP_STATUS Send(const void* data, UINT length);
	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);
	// TODO: add RecvFrom, SendTo; see bug 315474
	virtual void Close();
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address);
#ifdef OPSOCKET_GETLOCALSOCKETADDR // our end of the socket:
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address);
#endif // OPSOCKET_GETLOCALSOCKETADDR

#ifdef SOCKET_LISTEN_SUPPORT
	// Deprecated old API, for backwards compatibility:
	virtual OP_STATUS Bind(OpSocketAddress* socket_address);
	virtual OP_STATUS SetLocalPort(UINT port);
	virtual OP_STATUS Listen(UINT queue_size);
	// New API:
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size);
	virtual OP_STATUS Accept(OpSocket* socket);
	OP_STATUS Incoming(int fd, const struct sockaddr_storage &remote, socklen_t remlen);
#endif // SOCKET_LISTEN_SUPPORT

#ifdef SOCKET_SUPPORTS_TIMER
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default=FALSE);
#endif // SOCKET_SUPPORTS_TIMER

#ifdef OPSOCKET_OPTIONS
	/* Note: these methods are specified to only be callable between when
	 * OnConnected() has been called and when the socket is closed.  As such,
	 * they are only applicable to connecting sockets.  If we ever add any
	 * options for listening sockets, move [GS]etOption to a minimal base-class
	 * that m_connect and m_listen can share and extend the ?: chain in each of
	 * the following to also check m_listen, subject to suitable #if-ery. */
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value);
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& value);
#endif // OPSOCKET_OPTIONS

#ifdef OPSOCKET_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv();
	virtual OP_STATUS ContinueRecv();
#endif

#ifdef _EXTERNAL_SSL_SUPPORT_

# ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

	/** Stubs for correct linking of the reference implementation
	 *  of External SSL.
	 *
	 *  The following methods implement the virtual abstract External SSL API
	 *  by returning an error status. The PosixSocket does not support secure
	 *  sockets directly. Whenever some core code wants to use a secure
	 *  connection, it should have created and used an OpenSSLSocket instance.
	 *  The OpenSSLSocket is a wrapper around an OpSocket implementation,
	 *  which uses the normal OpSocket methods and implements these methods.
	 *
	 *  @see class OpenSSLSocket.
	 *
	 *  @{
	 */
	void ESSL_ASSERT() { OP_ASSERT(!"You need to use an OpenSSLSocket instance when you want to use the secure API!"); }
	virtual OP_STATUS UpgradeToSecure() { ESSL_ASSERT(); return OpStatus::ERR; }
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st*, UINT) { ESSL_ASSERT(); return OpStatus::ERR; }
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate*) { ESSL_ASSERT(); return OpStatus::ERR; }
	virtual OP_STATUS GetCurrentCipher(cipherentry_st*, BOOL*, UINT32*) { ESSL_ASSERT(); return OpStatus::ERR; }
	virtual OP_STATUS SetServerName(const uni_char*) { ESSL_ASSERT(); return OpStatus::ERR; }
	virtual OpCertificate* ExtractCertificate() { ESSL_ASSERT(); return NULL; }
	virtual OpAutoVector<OpCertificate>* ExtractCertificateChain() { ESSL_ASSERT(); return NULL; }
	virtual int GetSSLConnectErrors() { ESSL_ASSERT(); return 0; }
	/** @} */

# else // #ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

	/** External SSL could be supported via inheritance.
	 *
	 * We could expose this class for the platform to use as base, making some
	 * private methods protected for their sake.  However, this issue is
	 * deferred until a platform using essentially POSIX sockets is using an
	 * external SSL library and asks the module owner how best we can support
	 * it.  That could sensibly be achieved by submitting a patch to this file,
	 * adding the needed implementation.
	 *
	 * Of course, if external crypto libraries settle on some shared public API,
	 * that could support a standards-based implementation of these methods.
	 */
#error "No support for external SSL handlers"
	virtual OP_STATUS StartSecure();
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers,
									   UINT cipher_count, BOOL tls_v1_enabled);
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher,
									   BOOL* tls_v1_used, UINT32* pk_keysize) const;
	virtual OP_STATUS SetServerName(const uni_char* server_name);
	virtual void SetCertificateDialogMode(BOOL aUnAttendedDialog);
#  ifdef ABSTRACT_CERTIFICATES
	virtual OpCertificate* ExtractCertificate() const;
#  endif // ABSTRACT_CERTIFICATES
#  ifdef SUPPORT_EXTERNAL_SSL_ERROR_CODES
	virtual int GetLastSecConnError() const;
#  endif // SUPPORT_EXTERNAL_SSL_ERROR_CODES

# endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#endif // _EXTERNAL_SSL_SUPPORT_
};

// ================= PosixAutoClose ===========================================

class PosixAutoClose
{
private:
	int m_fd;

public:
	PosixAutoClose(int fd);
	~PosixAutoClose();
	int release();
	int get();
};

PosixAutoClose::PosixAutoClose(int fd)
	: m_fd(fd)
{
}

PosixAutoClose::~PosixAutoClose()
{
	if (m_fd != -1)
		close(m_fd);
}

int PosixAutoClose::release()
{
	int fd = m_fd;
	m_fd = -1;
	return fd;
}

int PosixAutoClose::get()
{
	return m_fd;
}

// ================= PosixSocket::ConnectListener =============================

PosixSocket::ConnectListener::ConnectListener(PosixSocket *boss, int fd, bool connected)
	: PosixLogger(SOCKET)
	, m_boss(boss)
	, m_fd(fd)
	, m_mode(PosixSelector::CONVERSE)
#ifdef OPSOCKET_PAUSE_DOWNLOAD
	, m_paused(false)
	, m_pending_disconnect(false)
#endif
	, m_connected(connected)
	, m_sending(0)
{
}

PosixSocket::ConnectListener::~ConnectListener()
{
	if (m_fd != -1)
		DetachAndClose(m_fd); // detach before close
}

#ifdef OPSOCKET_PAUSE_DOWNLOAD
void PosixSocket::ConnectListener::Pause()
{
	if (m_paused)
		return;

	m_paused = true;
	Snooze();
}

void PosixSocket::ConnectListener::Resume()
{
	const bool paused = m_paused;
	m_paused = false;
	if (m_fd + 1)
	{
		if (paused)
			Listen(m_fd);

		if (m_pending_disconnect)
		{
			m_pending_disconnect = false;
			/* Provoke a Disconnect() - via core, if available.  Must happen
			 * last, as core might respond to the data-ready message by deleting
			 * this socket, which would make all later actions on this or m_boss
			 * into crashers. */
			m_boss->OnReadReady(m_fd, true);
			// this may be deleted after notifying the boss
		}
	}
}

bool PosixSocket::ConnectListener::DisconnectBlocked()
{
	m_pending_disconnect = m_fd + 1 && m_paused && Peek();
	return m_pending_disconnect;
}
#endif // OPSOCKET_PAUSE_DOWNLOAD

ssize_t PosixSocket::ConnectListener::Peek()
{
	OP_ASSERT(m_fd + 1);
	OP_ASSERT(fcntl(m_fd, F_GETFL, 0) & O_NONBLOCK);
	char c;
	errno = 0;
	return recv(m_fd, &c, 1, MSG_PEEK);
}

ssize_t PosixSocket::ConnectListener::Receive(void *buffer, size_t length)
{
	OP_ASSERT(m_fd + 1);
	errno = 0;
	OP_ASSERT(fcntl(m_fd, F_GETFL, 0) & O_NONBLOCK);
	return recv(m_fd, buffer, length, 0);
}

ssize_t PosixSocket::ConnectListener::Send(const void *buffer, size_t length)
{
	OP_ASSERT(m_fd + 1);
	errno = 0;
	return send(m_fd, buffer, length, PosixNetworkAddress::DefaultSendFlags);
}

OP_STATUS PosixSocket::ConnectListener::Listen(int fd, const class OpSocketAddress *connect)
{
	return g_posix_selector->Watch(fd, m_mode, this, connect);
}

void PosixSocket::ConnectListener::Snooze()
{
	if (m_fd + 1)
		Detach(m_fd);
}

void PosixSocket::ConnectListener::SetMode(bool enable, PosixSelector::Type mode)
{
	// C++ spec won't let us cast int to enum: bodge round that ...
	PosixSelector::Type stupid[] =
		{ PosixSelector::NONE, PosixSelector::READ,
		  PosixSelector::WRITE, PosixSelector::CONVERSE };

	m_mode = stupid[enable ? m_mode | mode :
					m_mode & PosixSelector::CONVERSE & ~mode];

	if (m_fd + 1)
		g_posix_selector->SetMode(this, m_fd, m_mode);
	else
		g_posix_selector->SetMode(this, m_mode);
}

#ifdef OPSOCKET_OPTIONS
OP_STATUS PosixSocket::ConnectListener::SetOption(OpSocketBoolOption option, BOOL value)
{
	if (m_fd == -1 || !IsConnected())
		return OpStatus::ERR_BAD_FILE_NUMBER;

	int level, opt;
	switch (option)
	{
		case SO_TCP_NODELAY:
			level = POSIX_TCP_LEVEL;
			opt = TCP_NODELAY;
			break;
		default:
			OP_ASSERT(!"not implemented yet");
			return OpStatus::ERR_NOT_SUPPORTED;
	}
	int val = value ? 1 : 0;
	errno = 0;
	if (setsockopt(m_fd, level, opt, &val, sizeof(val)) != 0)
		switch (errno)
		{
			case EINVAL:      /* deliberate fall-through */
			case ENOPROTOOPT:
				return OpStatus::ERR_OUT_OF_RANGE;
			case EFAULT:      /* deliberate fall-through */
			case ENOTSOCK:
				OP_ASSERT(!"setsockopt() called with invalid arguments");
				/* deliberate fall-through */
			case EBADF:       /* deliberate fall-through */
			default:
				return OpStatus::ERR;
		}
	return OpStatus::OK;
}

OP_STATUS PosixSocket::ConnectListener::GetOption(OpSocketBoolOption option, BOOL& value)
{
	if (m_fd == -1 || !IsConnected())
		return OpStatus::ERR_BAD_FILE_NUMBER;

	int level, opt;
	switch (option)
	{
		case SO_TCP_NODELAY:
		{
			level = POSIX_TCP_LEVEL;
			opt = TCP_NODELAY;
			break;
		}
		default:
			OP_ASSERT(!"not implemented yet");
			return OpStatus::ERR_NOT_SUPPORTED;
	}
	int val = 0;
	socklen_t val_length = sizeof(val);
	errno = 0;
	if (getsockopt(m_fd, level, opt, &val, &val_length) != 0)
		switch (errno)
		{
			case EINVAL:      /* deliberate fall-through */
			case ENOPROTOOPT:
				return OpStatus::ERR_OUT_OF_RANGE;
			case EFAULT:      /* deliberate fall-through */
			case ENOTSOCK:
				OP_ASSERT(!"getsockopt() called with invalid arguments");
				/* deliberate fall-through */
			case EBADF:       /* deliberate fall-through */
			default:
				return OpStatus::ERR;
		}
	OP_ASSERT(val_length == sizeof(int));
	OP_ASSERT(val == 1 || val == 0);
	value = val != 0;

	return OpStatus::OK;
}
#endif // OPSOCKET_OPTIONS

void PosixSocket::ConnectListener::OnError(int fd, int err)
{
	if (IsConnected())
	{
		OP_ASSERT(m_fd == fd);
		if (Peek())
			/* There's data waiting to be read; let reading it cause us to
			 * report the error via Recv(). */
			m_boss->OnReadReady(fd);
		else
			m_boss->OnError(err);
		// this may be deleted after notifying the boss
	}
	else // Failed to connect: won't get any activity on this file handle.
	{
		m_fd = fd;
		SetMode(false, PosixSelector::CONVERSE);
		Detach(fd);
		m_boss->OnConnectError(err);
		// this may be deleted after notifying the boss
	}
}

void PosixSocket::ConnectListener::OnDetach(int fd)
{
	OP_ASSERT(!"Socket should have disconnected before Opera shut-down !");
	OP_ASSERT(fd == m_fd);
	SetConnected(false);
	close(fd);
	m_fd = -1;
	m_boss->Disconnect();
	// this may be deleted after notifying the boss
}

void PosixSocket::ConnectListener::OnWriteReady(int fd)
{
	if (IsConnected())
	{
		OP_ASSERT(fd == m_fd);
		WriteSome();
	}
	else // Failed to connect: report this as an error
		OnError(fd, ECONNRESET);
}

void PosixSocket::ConnectListener::OnReadReady(int fd)
{
	if (IsConnected())
	{
		OP_ASSERT(fd == m_fd);
		m_boss->OnReadReady(fd);
		// this may be deleted after notifying the boss
	}
	else // Failed to connect: report this as an error
		OnError(fd, ECONNRESET);
}

void PosixSocket::ConnectListener::OnConnected(int fd)
{
	OP_ASSERT(fd >= 0);
	OP_ASSERT(fd == m_fd);
	if (!IsConnected())
	{
		SetConnected(true);

#ifdef OPSOCKET_PAUSE_DOWNLOAD
		if (m_paused) // Stop listening, until Resume()d.
			Snooze();
		else
#endif
			// Ensure we're ready to report reads and writes:
			SetMode(false, PosixSelector::NONE);

		m_boss->OnConnected(fd);
		// this may be deleted after notifying the boss
	}
}

void PosixSocket::ConnectListener::WriteSome()
{
	OP_ASSERT(m_boss && m_boss->m_ear);
	/* We may send some data to the socket and call on success
	 * OpSocketListener::OnSocketDataSent(), which in turn may call this method
	 * again (via PosixSocket::Send()). Avoid a too deep recursion level and
	 * instead send the data on the next ConnectListener::OnWriteReady()
	 * callback. */
	if (m_sending > MAX_SEND_RECURSIONS)
		return;

	++m_sending;
	Log(SPEW, "%010p: Ready to write to %d (recursion: %d)\n", this, m_fd, m_sending);
	OpSocket::Error err = OpSocket::NETWORK_NO_ERROR;
	size_t sent_now = 0;
	while (err == OpSocket::NETWORK_NO_ERROR && !m_boss->m_outgoing.IsEmpty())
	{
		char buffer[POSIX_SENDBUF_MAX_SIZE];	// ARRAY OK 2012-04-18 eddy
		size_t size = m_boss->m_outgoing.CopyInto(buffer, POSIX_SENDBUF_MAX_SIZE);
		ssize_t sent = Send(buffer, size);
		if (sent < 0)
			err = DecodeErrno(errno);
		else if (sent)
		{
			OP_ASSERT(static_cast<size_t>(sent) <= size);
			Log(PosixLogger::SPEW, "%010p: sent %zd bytes\n", this, sent);
			sent_now += sent;
			m_boss->m_outgoing.Consume(sent);
		}
	}
	if (m_boss->m_outgoing.IsEmpty())
		SetMode(false, PosixSelector::WRITE); // *before* callback
	if (sent_now > 0)
	{
		m_boss->m_ear->OnSocketDataSent(m_boss, sent_now); // may call OpSocket::Send
#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
		m_boss->m_ear->OnSocketDataSendProgressUpdate(m_boss, sent_now);
#endif
	}
	--m_sending;

	if (err != OpSocket::NETWORK_NO_ERROR && err != OpSocket::SOCKET_BLOCKING)
		m_boss->OnWriteError(err);
		// this may be deleted after notifying the boss
}

// ================= PosixSocket::ListenListener ==============================

#ifdef SOCKET_LISTEN_SUPPORT
PosixSocket::ListenListener::ListenListener(PosixSocket* boss, int fd)
	: m_boss(boss)
	, m_fd(fd)
{
	OP_ASSERT(fd + 1);
}

PosixSocket::ListenListener::~ListenListener()
{
	if (m_fd + 1)
		DetachAndClose(m_fd);
}

OP_STATUS PosixSocket::ListenListener::Accept(OpSocket *socket)
{
	if (!socket)
		return OpStatus::ERR_NULL_POINTER;
	OP_STATUS st = OpStatus::OK;
	OpStatus::Ignore(st);

	struct sockaddr_storage ra; // remote address
	socklen_t len = sizeof(ra);
	int err = errno = 0;
	PosixAutoClose fd(accept(m_fd, reinterpret_cast<struct sockaddr*>(&ra), &len));
	g_posix_selector->SetMode(this, m_fd, PosixSelector::READ);

	if (fd.get() == -1)
	{
		err = errno;
		return m_boss->DecodeSetupError(err, "accept");
	}

	// fd is the file handle for a new socket, connected to the incoming request
	m_boss->Log(PosixLogger::SPEW,
				"%010p: accept()ed incoming %010p on %d\n",
				m_boss, socket, fd.get());

	OP_ASSERT(len <= (socklen_t)sizeof(ra)); // else ra has been truncated :-(
	PosixSocket &posix = *static_cast<PosixSocket*>(socket);
	if (!posix.SetSocketFlags(fd.get(), &err))
		return m_boss->DecodeSetupError(err, "accept");
	RETURN_IF_ERROR(posix.Incoming(fd.get(), ra, len));
	fd.release();
	return OpStatus::OK;
}

void PosixSocket::ListenListener::OnError(int fd, int err)
{
	OP_ASSERT(fd == m_fd);
	DetachAndClose(m_fd);
	m_fd = -1;
	m_boss->OnListenError(err);
}

void PosixSocket::ListenListener::OnDetach(int fd)
{
	OP_ASSERT(fd == m_fd);
	// is already detached...
	close(m_fd);
	m_fd = -1;
	m_boss->EndListen();
}

void PosixSocket::ListenListener::OnReadReady(int fd) // Ready to accept().
{
	OP_ASSERT(fd == m_fd);
	g_posix_selector->SetMode(this, m_fd, PosixSelector::NONE);
	m_boss->OnAcceptReady(); // We get several of these where OpSocket selftest expects one
}
#endif // SOCKET_LISTEN_SUPPORT

OP_STATUS OpSocket::Create(OpSocket** socket, OpSocketListener* listener, BOOL secure /* = FALSE */)
{
	if (socket)
		*socket = 0;
	else
		return OpStatus::ERR_NULL_POINTER;

	if (secure)
		return OpStatus::ERR_NOT_SUPPORTED; // See specification.

	PosixSocket *local = OP_NEW(PosixSocket, (listener /* , secure != FALSE */));
	if (local == 0)
		return OpStatus::ERR_NO_MEMORY;

	// local->Init();
	*socket = local;
	return OpStatus::OK;
}

// ================= PosixSocket ==============================================

PosixSocket::PosixSocket(OpSocketListener* listener /* , bool ignored */)
	: m_ear(listener)
	, m_connect(0)
#ifdef SOCKET_LISTEN_SUPPORT
	, m_listen(0)
#endif
	// , m_secure(secure)
{
	Log(VERBOSE, "%010p: Created TCP socket.\n", this);
#ifdef LOG_SIG_PIPE
	SigPipeLogger::PipeWatch(); // leak of g_instance is known
#endif // LOG_SIG_PIPE
}

// static
OpSocket::Error PosixSocket::DecodeErrno(int err) // err is an errno value
{
	switch (err)
	{
	case AGAINCASE:		/* Try again / Operation would block */
	case EINPROGRESS:	/* Operation now in progress */
		return SOCKET_BLOCKING;

	case ECONNABORTED:	/* A connection has been aborted */
	case EPIPE:			/* Socket not open, or not connected; expect SIGPIPE */
	case EINTR:			/* Interrupted system call */
		return CONNECTION_CLOSED;

	case ECONNREFUSED:	/* Connection refused */
		return CONNECTION_REFUSED;

	case EHOSTUNREACH:	/* No route to host */
	case EAFNOSUPPORT:	/* Address family not supported by protocol */
		/* Note: This is a hack to make the URL code try alternative addresses.
		 * CONNECTION_REFUSED is a really improper error code here.  We may get
		 * these errors when we don't have IPv6 support in the kernel, or at
		 * least not access to the IPv6 network. */
		return CONNECTION_REFUSED;

	case EMSGSIZE:		/* Message too long */
	case ENETDOWN:		/* Network is down */
	case ENETUNREACH:	/* Network is unreachable */
		return NETWORK_ERROR;

	case ETIMEDOUT:		/* Connection timed out */
		return CONNECT_TIMED_OUT;

	case EADDRINUSE:	/* Address already in use */
		/* The socket's local address is already in use and the socket was not
		 * marked to allow address reuse with SO_REUSEADDR.  This error usually
		 * occurs when executing bind, but could be delayed until this function
		 * if the bind was to a partially wild-card address (involving ADDR_ANY)
		 * and if a specific address needs to be committed at the time of this
		 * function. */
		return ADDRESS_IN_USE;

#ifdef EPROTO // absent on some systems (DSK-236583)
	case EPROTO:
		/* A protocol error has occurred; for example, the STREAMS protocol
		 * stack has not been initialized. */
#endif // EPROTO
	case EOPNOTSUPP:
		/* The socket type of the specified socket does not support accepting
		 * connections. */
	case EALREADY:		/* Operation already in progress */
	case ENOTSOCK:		/* Socket operation on non-socket */
	case EISCONN:		/* Transport endpoint is already connected */
	case EBADF:			/* The socket argument is not a valid file descriptor. */
	case EINVAL:		/* Invalid argument */
		/* For example, writing on a listening socket, or the destination
		 * address specified is not consistent with that of the constrained
		 * group to which the socket belongs.  Also used by PosixNetworkAddress
		 * to signal invalid address. */
	case EFAULT:		/* Bad address */
		/* The name or the namelen parameter is not a valid part of the user
		 * address space, the namelen parameter is too small, or the name
		 * parameter contains incorrect address format for the associated
		 * address family. */
	case EADDRNOTAVAIL:	/* Cannot assign requested address */
		/* The remote address is not a valid address (such as ADDR_ANY). */
	case ECONNRESET:	/* Connection reset by peer */
		return CONNECTION_FAILED;

	case EACCES:		/* Permission denied */
		/* Attempt to connect datagram socket to broadcast address
		 * failed because setsockopt option SO_BROADCAST is not enabled. */
		return ACCESS_DENIED;

	case ENOMEM: case ENOBUFS:
		return OUT_OF_MEMORY;

	default:
		return (Error)-1;
	}
}

#if defined(SOCKET_LISTEN_SUPPORT) || defined(OPSOCKET_GETLOCALSOCKETADDR)
OP_STATUS PosixSocket::ClearLocal()
{
	OpString system;
	RETURN_IF_ERROR(g_op_system_info->GetSystemIp(system));
	return m_local.FromString(system.CStr());
}

OP_STATUS PosixSocket::SetLocalInterface(OpSocketAddress* socket_address)
{
#ifndef POSIX_LISTEN_UNSET_IS_ANY
	if (!socket_address->IsValid())
	{
		RETURN_IF_ERROR(ClearLocal());
		m_local.SetPort(socket_address->Port());
		return OpStatus::OK;
	}
#endif // POSIX_LISTEN_UNSET_IS_ANY
	return m_local.Import(socket_address);
}
#endif // SOCKET_LISTEN_SUPPORT or OPSOCKET_GETLOCALSOCKETADDR

OP_STATUS PosixSocket::SetLocalInterface(int fd)
{
#if defined(SOCKET_LISTEN_SUPPORT) || defined(OPSOCKET_GETLOCALSOCKETADDR)
	struct sockaddr_storage la;
	socklen_t len = sizeof(la);
	if (1 + getsockname(fd, reinterpret_cast<struct sockaddr*>(&la), &len))
		return m_local.SetFromSockAddr(la, len);

	return OpStatus::ERR;
#else // !SOCKET_LISTEN_SUPPORT and !OPSOCKET_GETLOCALSOCKETADDR
	return OpStatus::OK;
#endif // SOCKET_LISTEN_SUPPORT or OPSOCKET_GETLOCALSOCKETADDR
}

void PosixSocket::Disconnect()
{
	// We shouldn't OnSocketClosed() if there is pending data to Recv(); CORE-17808
#ifdef OPSOCKET_PAUSE_DOWNLOAD
	if (m_connect && m_connect->DisconnectBlocked())
		return; // m_connect shall remember to trigger this when Resume()d.
#endif

	OP_ASSERT(!(m_connect && m_connect->Peek()));
	ClearSender();

#ifdef SOCKET_LISTEN_SUPPORT
	if (m_listen == 0)
#endif
		m_ear->OnSocketClosed(this);
}

inline void PosixSocket::OnReadReady(int fd, bool force)
{
	OP_ASSERT(m_connect);
	/* Decide what to do.
	 *
	 * If we have no listener, we can't tell core, who presumably removed the
	 * listener while we were waiting for data, so just disconnect (soon).
	 * Otherwise, if we have a pending disconnect (i.e. force), delegate via
	 * core; tell it we've got some data, which shall prompt it to Recv(), which
	 * should make us rediscover our reasons for wanting to disconnect.
	 *
	 * The remaining case is normal operation: if we \e actually have data to
	 * read, let core know about it; if not, our alleged data ready is
	 * presumably a shutdown, graceful if Peek() gets zero, painful otherwise.
	 */
	ssize_t peek = m_ear ? (force ? 1 : m_connect->Peek()) : 0;
	if (peek < 0) // CORE-19363:
	{
		const int err = errno;
		if (Log(SPEW, "%010p: Failed data-ready message via %010p from select()\n", this, m_connect))
			PError("recv", err, Str::S_ERR_SELECT_DATARDY_LIES,
				   "Failed recv() after select() promissed data");

		switch (err)
		{
		case AGAINCASE:
			// Ignore, a later OnReadReady() should let us get the data.
			return;

		case ENOBUFS: case ENOMEM: case EINTR: case EIO:
			/* Better let core know; its Read() might have better luck,
			 * and has some scope for reporting the problem. */
			break;

		case EOPNOTSUPP: case ETIMEDOUT: case ENOTSOCK:
		case ENOTCONN: case EINVAL: case EBADF: default:
			OP_ASSERT(!"Unexpected error from recv()");
			// fall through;
		case ECONNRESET: // Connection reset by peer.
			peek = 0;
			break;
		}
	}

	if (peek)
	{
		Log(SPEW, "%010p: Ready to read %d using %010p\n", this, fd, m_connect);
		OP_ASSERT(m_ear); // thanks to how we initialized peek.
		m_ear->OnSocketDataReady(this);
	}
	else // No data available and peer has done an orderly shut-down.
		Disconnect();
}

void PosixSocket::OnConnected(int fd)
{
	SetLocalInterface(fd);
	Log(VERBOSE, "%010p: Connected %d using %010p\n", this, fd, m_connect);
	m_ear->OnSocketConnected(this);
}

void PosixSocket::OnError(int err)
{
	Log(NORMAL, "%010p: Error (%d) on socket, using %010p\n", this, err, m_connect);
	Error error = DecodeErrno(err);
	// m_ear->OnSocket*Error(this, ?what?)
	if (error + 1)
		/* Receive or send ?
		 *
		 * In principle this error (about m_connect, from the selector) could be
		 * either a read error or a write error.  However, OpSocket::Recv()
		 * clearly documents that we're only allowed to call
		 * OnSocketReceiveError() synchronously, and we do our reading
		 * synchronously: so we should only get read errors during Recv().
		 *
		 * A socket might give us a read error asynchronously for some bizarre
		 * reason - but select()/poll() and their peers merely report "error"
		 * without distinguishing read from write.  So our best guess is that an
		 * error received asynchronously is a write error.
		 *
		 * If we *did* want to report an asynchronous read error, Recv()'s spec
		 * forbids us to do so: what we "should" do for such a case would be to
		 * report read "ready" and let the listener's subsequent attempted
		 * Recv() be told about the error.
		 *
		 * This is only relevant for selectors that actually know what the error
		 * is: aside from connection errors, which don't come here, the simple
		 * selector doesn't know what the error *is* so we get err equal to
		 * zero, making error be -1.  However, the kqueue and (to a lesser
		 * degree) epoll selectors do have error information. */
		OnWriteError(error);
	else // Don't know what to say to core.
		Disconnect();
}

void PosixSocket::OnConnectError(int err)
{
	Log(NORMAL, "%010p: Error (%d) connecting socket, using %010p\n",
		this, err, m_connect);
	/* In principle, m_connect->Snooze(); but selector already cleared mode;
	 * and m_fd is still -1, so Snooze() would no-op. */
	Error error = DecodeErrno(err);
	/* Reporting NETWORK_ERROR here will make core give up (probably under the
	 * assumption that all available networks are broken).  To have core try
	 * other access paths (in particular alternative server addresses) it is
	 * necessary to report CONNECTION_FAILED, CONNECTION_REFUSED or
	 * CONNECT_TIMED_OUT. */
	if (error == OpSocket::NETWORK_ERROR)
		error = OpSocket::CONNECTION_FAILED;
	m_ear->OnSocketConnectError(this, error + 1 ? error : CONNECTION_FAILED);
}

void PosixSocket::OnReadError(Error error)
{
	if (m_connect && error != SOCKET_BLOCKING)
		m_connect->SetMode(false, PosixSelector::READ);
	m_ear->OnSocketReceiveError(this, error);
}

void PosixSocket::OnWriteError(Error error)
{
	if (m_connect && error != SOCKET_BLOCKING)
		m_connect->SetMode(false, PosixSelector::WRITE);
	m_ear->OnSocketSendError(this, error);
}

#ifdef SOCKET_LISTEN_SUPPORT
void PosixSocket::OnAcceptReady()
{
	m_ear->OnSocketConnectionRequest(this);
}

void PosixSocket::EndListen()
{
	OP_DELETE(m_listen); m_listen = 0;
	if (m_connect == 0)
		m_ear->OnSocketClosed(this);
	/* Note: this may be deleted after notifying the OpSocketListener */
}

void PosixSocket::OnListenError(int err)
{
	Error error = DecodeErrno(err);
	if (error + 1)
		m_ear->OnSocketListenError(this, error);
	/* Note: OnSocketListenError might as well delete this, but there is no
	 * current implementation which tries to do so. So currently it is safe to
	 * continue with the next call... */
	EndListen();
	/* Note: this may be deleted after notifying the OpSocketListener */
}

OP_STATUS PosixSocket::DecodeListenError(int err)
{
	Error error = DecodeErrno(err);
	switch (err)
	{
	case ENOMEM: case ENOBUFS:
		return OpStatus::ERR_NO_MEMORY;

	case EMFILE: case ENFILE:
		PError("socket", err, Str::S_ERR_SOCKET_NO_HANDLE,
			   "Failed (check ulimit) to bind/listen socket");
		break;

	case EACCES:
		/* The specified address is protected and the current user does not have
		 * permission to bind to it. */
		error = ACCESS_DENIED;
		break;

	case EADDRINUSE: // The specified address is already in use.
	case EISCONN: // The socket is already connected.
		error = ADDRESS_IN_USE;
		break;
	}
	if (error + 1)
		m_ear->OnSocketListenError(this, error);
	return OpStatus::ERR;
}
#endif // SOCKET_LISTEN_SUPPORT

void PosixSocket::ClearSender()
{
	OP_ASSERT(OnCoreThread());
	Log(SPEW, "%010p: disconnecting %010p\n", this, m_connect);
	ConnectListener * conn = m_connect;
	m_connect = 0;
	OP_DELETE(conn);
}

bool PosixSocket::SetSocketFlags(int fd, int *err)
{
	errno = 0;
	if (!PosixBaseSocket::SetSocketFlags(fd, err))
		return false;

#if POSIX_SOCKET_BUFFER_SIZE > 0
	int bufsize = POSIX_SOCKET_BUFFER_SIZE;
	int res = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
	if (res == -1)
	{
		*err = errno;
		Log(TERSE, "%010p: Failed to configure socket options: %s\n",
			this, strerror(*err));

		return false;
	}
#endif // POSIX_SOCKET_BUFFER_SIZE
	return true;
}

OP_STATUS PosixSocket::DecodeSetupError(int err, const char* fname)
{
	switch (err)
	{
	case ENOMEM: case ENOBUFS:
		return OpStatus::ERR_NO_MEMORY;

	case EMFILE: case ENFILE:
		PError(fname, err, Str::S_ERR_SOCKET_NO_HANDLE,
			   "Failed (check ulimit) to accept socket");
		break;
	}
	Error error = DecodeErrno(err);
	if (!(error + 1))
		return OpStatus::ERR;

	m_ear->OnSocketConnectError(this, error);
#ifdef YNGVE_PI_HARMONY // see bug 342321
	return OpStatus::ERR;
#else
	return OpStatus::OK;
#endif
}

PosixSocket::~PosixSocket()
{
	ClearSender();

#ifdef SOCKET_LISTEN_SUPPORT
	OP_DELETE(m_listen);
#endif
	Log(VERBOSE, "%010p: Destroyed TCP socket.\n", this);
}

// virtual
void PosixSocket::SetListener(OpSocketListener* listener)
{
	m_ear = listener;
}

/**
 * Workflow on Connect():
 * \msc
 * core, OpSocketListener, PosixSocket, ConnectListener, PosixSelector, UI;
 * core=>OpSocketListener
 *		[label="1. creates"];
 * core=>PosixSocket
 *		[label="2. OpSocket::Create()", url="\ref OpSocket::Create()"];
 * core=>PosixSocket
 *		[label="3. Connect()", url="\ref PosixSocket::Connect()"];
 * PosixSocket=>ConnectListener
 *		[label="creates"];
 * PosixSocket=>ConnectListener
 *		[label="4. Listen()",
 *		 url="\ref PosixSocket::ConnectListener::Listen()"];
 * ConnectListener=>PosixSelector
 *		[label="5. Watch()", url="\ref PosixSelector::Watch()"];
 * ConnectListener<<PosixSelector;
 * PosixSocket<<ConnectListener;
 * core<<PosixSocket;
 * --- [label="between two calls to Opera::RunSlice():"];
 * UI=>PosixSelector
 *		[label="6. Poll()", url="PosixSelector::Poll()"];
 * PosixSelector=>PosixSelector
 *		[label="7. connect()"];
 * PosixSelector=>ConnectListener
 *		[label="8. OnConnected()",
 *		 url="\ref PosixSocket::ConnectListener::OnConnected()"];
 * ConnectListener=>PosixSocket
 *		[label="9. OnConnected()",
 *		 url="\ref PosixSocket::OnConnected()"];
 * PosixSocket=>OpSocketListener
 *		[label="10. OnSocketConnected()",
 *		 url="\ref OpSocketListener::OnSocketConnected()"];
 * PosixSocket<<OpSocketListener;
 * ConnectListener<<PosixSocket;
 * PosixSelector<<ConnectListener;
 * UI<<PosixSelector;
 * \endmsc
 *
 * -# core creates an OpSocketListener to be informed about socket events.
 *
 * -# core creates a PosixSocket instance by calling OpSocket::Create(),
 *    specifying the just created OpSocketListener instance.
 * -# core Connect()s to the socket (specifying some address).
 * -# The PosixSocket creates a new PosixSocket::ConnectListener instance and
 *    calls PosixSocket::ConnectListener::Listen().
 * -# The PosixSocket::ConnectListener calls PosixSelector::Watch() to
 *    establish the connection and start watching socket activity. The
 *    PosixSelector::Watch() attempts a first (non-blocking) connect(). If
 *    the connect is completed successfully,
 *    PosixSocket::ConnectListener::OnConnected() is called (like step 8). If
 *    the connection is not established immediately each subsequent
 *    PosixSelector::Poll() call attempts to do so again as described in
 *    the following steps.
 * -# Between two Opera::RunSlice() calls the UI implementation calls
 *    PosixSelector::Poll() with some timeout value.
 * -# PosixSelector::Poll() calls connect() to start connecting the socket
 *    in a non-blocking way. It then selects the socket for write activity and
 *    waits until the socket has connected (or the timeout has passed).
 * -# If the socket has connected, PosixSocket::ConnectListener::OnConnected()
 *    is called.
 * -# The PosixSocket::ConnectListener calls PosixSocket::OnConnected()
 * -# PosixSocket calls OpSocketListener::OnSocketConnected().
 *
 * When the socket is connected it can be used for reading and/or writing.
 */
OP_STATUS PosixSocket::Connect(OpSocketAddress* socket_address)
{
	if (m_ear == 0) // you must give me a listener, first !
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(!m_connect); // already connected !
	RETURN_IF_ERROR(m_remote.Import(socket_address));
	int err = errno = 0;
	PosixAutoClose fd(OpenSocket(m_remote.GetDomain(), SOCK_STREAM, 0));
	// Last param, protocol = 0, specifies default.  No doc found on alternative values !

	if (fd.get() == -1)
	{
		err = errno;
		return DecodeSetupError(err, "socket");
	}

	OP_ASSERT(0 <= fd.get() && fd.get() < (int)FD_SETSIZE && errno == 0);
	if (!SetSocketFlags(fd.get(), &err))
		return DecodeSetupError(err, "socket");

	if (!g_opera || !g_posix_selector)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(errno == 0);
	OP_DELETE(m_connect);
	m_connect = OP_NEW(ConnectListener, (this, fd.get(), false));
	RETURN_OOM_IF_NULL(m_connect);
	RETURN_IF_ERROR(m_connect->Listen(fd.get(), socket_address));
	/* Note: this may be deleted when ConnectListener::Listen() encounters an
	 * error, called the OpSocketListener::OnSocketConnectError() and that
	 * implementation deleted the socket. So we return immediately in
	 * case of an error without accessing this instance; m_connect will be
	 * deleted on deleting the socket. */

#ifdef POSIX_OK_LOG
	OpString8 whence;
	if (OpStatus::IsError(m_remote.ToString8(&whence)))
		whence.Empty();
	const char unknown[] = "<unknown>";
#define FromFmt " with %s"
#define FromArg , whence.HasContent() ? whence.CStr() : unknown
#else
#define FromFmt
#define FromArg
#endif // POSIX_OK_LOG
	OpStatus::Ignore(SetLocalInterface(fd.get()));
	Log(SPEW, "%010p: connecting %d" FromFmt " using %010p\n", this, fd.get() FromArg, m_connect);
#undef FromFmt
#undef FromArg

	fd.release();
	return OpStatus::OK;
}

/** Implementation of OpSocket::Send
 *
 * This simply adds the data to a queue of data-chunks. The data is then written
 * to the socket, when the PosixSelector::Poll() is notified that the socket is
 * ready for writing - see OnWriteReady().
 *
 * Workflow on writing data to a socket.
 * \msc
 * core, OpSocketListener, PosixSocket, ConnectListener, m_outgoing,
 *		PosixSelector, UI;
 * core => PosixSocket
 *		[label="1. Send(data, length)", url="\ref PosixSocket::Send()"];
 * PosixSocket => m_outgoing
 *		[label="2. AppendCopyData(data, length)",
 *		 url="\ref OpData::AppendCopyData()"];
 * PosixSocket << m_outgoing;
 * core << PosixSocket;
 * --- [label="between two calls to Opera::RunSlice():"];
 * UI => PosixSelector
 *		[label="3. Poll()", url="PosixSelector::Poll()"];
 * PosixSelector => PosixSelector
 *		[label="4. select()"];
 * PosixSelector => ConnectListener
 *		[label="5. OnWriteReady()",
 *		 url="\href PosixSocket::ConnectListener::OnWriteReady()"];
 * ConnectListener => ConnectListener
 *		[label="WriteSome()",
 *		 url="\href PosixSocket::ConnectListener::WriteSome()"];
 * ConnectListener => ConnectListener
 *		[label="6. send()"];
 * ConnectListener => m_outgoing
 *		[label="7. Consume()",
 *		 url="\href OpData::Consume()"];
 * ConnectListener << m_outgoing;
 * ConnectListener => OpSocketListener
 *		[label="8. OnSocketDataSent()",
 *		 url="\ref OpSocketListener::OnSocketDataSent()"];
 * ConnectListener << OpSocketListener;
 * PosixSelector << ConnectListener;
 * UI << PosixSelector;
 * \endmsc
 *
 * -# Core calls OpSocket::Send() to write a block of data to the socket.
 * -# PosixSocket::Send() appends the data to send to its #m_outgoing buffer.
 * -# Between two Opera::RunSlice() calls the UI implementation calls
 *    PosixSelector::Poll() with some timeout value.
 * -# PosixSelector::Poll() selects the socket for write-access and calls
 *    select().
 * -# If the socket is ready for writing, PosixSelector::Poll() calls
 *    PosixSocket::ConnectListener::OnWriteReady() which calls
 *    PosixSocket::ConnectListener::WriteSome().
 * -# PosixSocket::ConnectListener::WriteSome() gets data from the PosixSocket's
 *    #m_outgoing buffer and writes as much of it as possible to the socket
 *    asynchronously and non-blocking.
 * -# The sent bytes are removed from the #m_outgoing buffer.
 * -# If some bytes were written to the socket,
 *    OpSocketListener::OnSocketDataSent() is called.
 */
OP_STATUS PosixSocket::Send(const void* data, UINT length)
{
	OP_ASSERT(m_ear);
	if (!m_connect)
	{
		OnWriteError(CONNECTION_CLOSED);
		return OpStatus::ERR;
	}
	Log(SPEW, "%010p: sending %d bytes via %010p\n", this, length, m_connect);
	if (length == 0) return OpStatus::OK; // Nothing to do.
	if (length > POSIX_SENDBUF_MAX_SIZE ||
		m_outgoing.Length() > POSIX_SENDBUF_MAX_SIZE - length)
	{
		Log(PosixLogger::VERBOSE,
			"%010p: Queue full; no space for %u bytes more\n", this, length);
		// Tell caller to try again later:
		OnWriteError(SOCKET_BLOCKING);
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(m_outgoing.AppendCopyData(static_cast<const char*>(data), length));
	Log(PosixLogger::SPEW, "%010p: Queued %u bytes\n", this, length);
	m_connect->SetMode(true, PosixSelector::WRITE);
	m_connect->WriteSome();
	/* Note: this instance may be deleted if WriteSome() encounters an error,
	 * notifies OpSocketListener::OnSocketSendError() and the listener deletes
	 * the PosixSocket. */
	return OpStatus::OK;
}

/**
 * Workflow on reading data from a socket.
 * \msc
 * core, OpSocketListener, PosixSocket, ConnectListener, PosixSelector,
 *		UI;
 * UI=>PosixSelector
 *		[label="1. Poll()", url="\ref PosixSelector::Poll()"];
 * PosixSelector=>PosixSelector
 *		[label="2. select()"];
 * PosixSelector=>ConnectListener
 *		[label="3. OnReadReady()",
 *		 url="\ref PosixSocket::ConnectListener::OnReadReady()"];
 * ConnectListener=>PosixSocket
 *		[label="4. OnReadReady()", url="\ref PosixSocket::OnReadReady()"];
 * PosixSocket=>OpSocketListener
 *		[label="5. OnSocketDataReady()",
 *		 url="\ref OpSocketListener::OnSocketDataReady()"];
 * core=>PosixSocket
 *		[label="6. Recv()", url="\ref PosixSocket::Recv()"];
 * PosixSocket<<OpSocketListener;
 * ConnectListener<<PosixSocket;
 * PosixSelector<<ConnectListener;
 * UI<<PosixSelector;
 * \endmsc
 *
 * -# Reading data from a socket starts with the UI calling
 *    PosixSelector::Poll().
 * -# PosixSelector::Poll() selects the socket for reading notifications.
 * -# If the socket was signalled for reading activity,
 *    PosixSelector::Poll() calls
 *    PosixSocket::ConnectListener::OnReadReady().
 * -# PosixSocket::ConnectListener::OnReadReady() notifies
 *    PosixSocket::OnReadReady().
 * -# PosixSocket::OnReadReady() peeks for data, tests for error and (on
 *    success) calls OpSocketListener::OnSocketDataReady().
 * -# core can then call PosixSocket::Recv() to actually read data from the
 *    socket.
 */
OP_STATUS PosixSocket::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	if (m_connect == 0)
	{
		OnReadError(CONNECTION_CLOSED);
		return OpStatus::ERR; // you are not connected !
	}

	ssize_t read = m_connect->Receive(buffer, length);
	int err = errno;
	OP_ASSERT(err == 0 || read < 0);
	Log(SPEW, "%010p: received %d bytes using %010p\n", this, length, m_connect);

#ifdef POSIX_OK_LOG
	OpString8 whence;
	if (OpStatus::IsError(m_remote.ToString8(&whence)))
		whence.Empty();
	const char unknown[] = "<unknown>";
#define FromFmt " from %s"
#define FromArg , whence.HasContent() ? whence.CStr() : unknown
#else
#define FromFmt
#define FromArg
#endif // POSIX_OK_LOG
	if (read > 0)
	{
		Log(SPEW, "%010p: Read %ld bytes on socket" FromFmt "\n",
			this, (long)read FromArg);
		*bytes_received = read;
		return OpStatus::OK;
	}
	*bytes_received = 0;

	if (read == 0)
	{
		// When paused, more data might come when we Resume():
		if (!m_connect->IsPaused())
		{
			/* The remote socket has shut down (gracefully; contrast ECONNRESET,
			 * below); if no data were available but the socket were still open,
			 * recv would fail with EWOULDBLOCK (covered by AGAINCASE). */
			Log(VERBOSE, "%010p: End of data" FromFmt "\n", this FromArg);
			Disconnect();
		}
		*bytes_received = 0;
		return OpStatus::OK;
	}

	switch (err)
	{
	case EINTR: // interrupted before data available
	case AGAINCASE: // try again later !
		OnReadError(SOCKET_BLOCKING);
		break;

	case ETIMEDOUT: // timed out
		Log(VERBOSE, "%010p: Socket read" FromFmt " timed out.\n", this FromArg);
		OnReadError(RECV_TIMED_OUT);
		break;

	case ENOBUFS: case ENOMEM:
		Log(TERSE, "%010p: OOM during read on socket" FromFmt ".\n", this FromArg);
		return OpStatus::ERR_NO_MEMORY;

	case ECONNRESET: // reset by peer
		Disconnect();
		break;

	case EIO: // I/O error
		Log(NORMAL, "%010p: Input/Output error on socket" FromFmt " during recv().\n",
			this FromArg);
		break;

	case ENOTCONN: // not connected
		OP_ASSERT(!"Recv() called on socket which isn't connect()ed."); // wait for OnConnected() !
		break;

	case EPIPE: // socket not open, or not connected
	case EBADF: // bad file descriptor
	case ENOTSOCK: // not a socket
	case EOPNOTSUPP: // bad flags (but flags were all unset !)
	default: // not mentioned in the POSIX manual
		OP_ASSERT(!"Unhandled errno value"); // these conditions should not arise.
		break;
	}

#undef FromFmt
#undef FromArg
	return OpStatus::ERR;
}

// virtual
void PosixSocket::Close()
{
	ClearSender();

#ifdef SOCKET_LISTEN_SUPPORT
	OP_DELETE(m_listen);
	m_listen = 0;
#endif
}

// virtual
OP_STATUS PosixSocket::GetSocketAddress(OpSocketAddress* socket_address)
{
	return socket_address ? socket_address->Copy(&m_remote) : OpStatus::ERR_NULL_POINTER;
}

#ifdef OPSOCKET_GETLOCALSOCKETADDR // our end of the socket:
// virtual
OP_STATUS PosixSocket::GetLocalSocketAddress(OpSocketAddress* socket_address)
{
	return socket_address ? socket_address->Copy(&m_local) : OpStatus::ERR_NULL_POINTER;
}
#endif // OPSOCKET_GETLOCALSOCKETADDR

#ifdef SOCKET_LISTEN_SUPPORT // Deprecated old API, for backwards compatibility:
// virtual
OP_STATUS PosixSocket::Bind(OpSocketAddress* socket_address)
{
	return SetLocalInterface(socket_address);
}

// virtual
OP_STATUS PosixSocket::SetLocalPort(UINT port)
{
	if (!m_local.IsValid())
		RETURN_IF_ERROR(ClearLocal());

	m_local.SetPort(port);
	return OpStatus::OK;
}

// virtual
OP_STATUS PosixSocket::Listen(UINT queue_size)
{
	return Listen(0, queue_size);
}
#endif // SOCKET_LISTEN_SUPPORT

#ifdef SOCKET_LISTEN_SUPPORT // New API:
OP_STATUS PosixSocket::Listen(OpSocketAddress* socket_address, int queue_size)
{
#ifdef SOMAXCONN
	OP_ASSERT(queue_size <= SOMAXCONN);
#endif
	if (m_ear == 0)
		return OpStatus::ERR_NULL_POINTER;

	if (m_listen) // already listening !
		return OpStatus::ERR;

	if (socket_address)
		RETURN_IF_ERROR(SetLocalInterface(socket_address));

	if (m_local.Port() == 0) // no port specified
		return OpStatus::ERR;

	int err = errno = 0;
	PosixAutoClose fd(OpenSocket(m_local.GetDomain(), SOCK_STREAM, 0));
	if (fd.get() == -1)
	{
		err = errno;
		return DecodeListenError(err);
	}
	OP_ASSERT(errno == 0);

	// Set options on the file descriptor:
	int flag = 1; // true
	int res = setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
	if (res == -1)
	{
		err = errno;
		Log(TERSE,
			"%010p: %d = setsockopt(%d, ...) or fcntl(%d, ...) preparing for bind().\n",
			res, fd.get(), fd.get());
		return DecodeListenError(err);
	}

	if (!SetSocketFlags(fd.get(), &err)) // does its own reporting
		return DecodeListenError(err);
	OP_ASSERT(errno == 0);

	// Actually bind to the relevant interface:
#ifdef POSIX_LISTEN_UNSET_IS_ANY
	res = m_local.Bind(fd.get(), true);
#else
	res = m_local.Bind(fd.get());
#endif
	if (res == -1)
	{
		err = errno;
		Log(TERSE, "%010p: Failed bind(%d,,) on port %d\n",
			this, fd.get(), m_local.Port());
		return DecodeListenError(err);
	}
	OP_ASSERT(errno == 0);
	Log(VERBOSE, "%010p: Successful bind(%d,,) on port %d\n",
		this, fd.get(), m_local.Port());

	res = listen(fd.get(), queue_size);
	if (res == -1)
	{
		err = errno;
		Log(TERSE, "%010p: %d = listen(%d, %d) on port %d\n",
			this, res, fd.get(), queue_size, m_local.Port());
		return DecodeListenError(err);
	}
	OP_ASSERT(errno == 0);

	Log(VERBOSE, "%010p: Successful listen(%d, %d) on port %d\n",
		this, fd.get(), queue_size, m_local.Port());
	m_listen = OP_NEW(ListenListener, (this, fd.get()));
	RETURN_OOM_IF_NULL(m_listen);
	OP_STATUS st = g_posix_selector->Watch(fd.get(), PosixSelector::READ,
										   m_listen, 0, true);
	if (OpStatus::IsError(st))
	{
		OP_DELETE(m_listen);
		m_listen = NULL;
		return st;
	}
	fd.release();
	return OpStatus::OK;
}

// virtual
OP_STATUS PosixSocket::Accept(OpSocket* socket)
{
	return m_listen->Accept(socket);
}

OP_STATUS PosixSocket::Incoming(int fd, const struct sockaddr_storage &remote, socklen_t remlen)
{
	RETURN_IF_ERROR(SetLocalInterface(fd));
	RETURN_IF_ERROR(m_remote.SetFromSockAddr(remote, remlen));
	OP_ASSERT(remote.ss_family == m_remote.GetDomain());
#ifdef POSIX_SUPPORT_IPV6
	if (m_remote.IsUsableIPv6())
		g_opera->posix_module.SetIPv6Usable(true);
#endif
	OP_ASSERT(!m_connect); // if this fails, we need to delete and UnsetCallBack().

	m_connect = OP_NEW(ConnectListener, (this, fd, true));
	RETURN_OOM_IF_NULL(m_connect);

	Log(SPEW, "%010p: handling inbound connection on %d using %010p\n",
		this, fd, m_connect);

	OP_STATUS res = m_connect->Listen(fd);
	if (OpStatus::IsError(res))
	{
		OP_DELETE(m_connect);
		m_connect = 0;
	}
	else
		OnConnected(fd); // Actually accept() connected us, but now we succeed with it.

	return res;
}
#endif // SOCKET_LISTEN_SUPPORT

#ifdef SOCKET_SUPPORTS_TIMER
void PosixSocket::SetTimeOutInterval(unsigned int seconds, BOOL restore_default /* = FALSE */)
{
	// TODO: implement-me ! Absent from unix-net - idle timer control.
#if 0
	/* The following is what's needed for the *socket-level* timeout control;
	 * however, the PI spec talks about how long we wait around for connection
	 * and how long we tolerate pauses between data arrival.  We could do that
	 * using an OpTimer() - but surely core could do that themselves ? well,
	 * they have no way to cancel a send, I guess, short of deleting the socket
	 * - and/or by remembering and examining time-stamps.  OpTimer is surely
	 * handier.
	 *
	 * Most of the actual work can be handled by m_connect; each time it gets a
	 * relevant call-back, it resets the OpTimer, using its .Start() - which
	 * does an internal Stop() for us, so no need to do it ourselves.  On
	 * time-out, m_connect lets the socket know (via the message queue, so we
	 * don't return from a succession of methods on dead objects);
	 * distinguishing between connection time-out and data time-out is easy;
	 * IsConnected() is false for the former.  When it gets the timeout message,
	 * the socket destroys m_connect (hence its timer) and tells m_ear about the
	 * time-out.
	 *
	 * Fiddlinesses: socket has to remember the timeout settings, seconds and
	 * default, since this method could be called to set its timeout *before* we
	 * have m_connect to implement it; changing the timeout during an operation
	 * requires a little care.  Connecting and receiving are the main things for
	 * which we care about timing out, and both happen in the core thread, but
	 * if data does get sent (which happens in another thread) we do want to
	 * reset the timer.  That reset can wait for a message from the sender, but
	 * this message is apt to be somewhat delayed, since sending tries its very
	 * best to clear all its chunks at once, before notifying us.  In short,
	 * I've designed PendingSend::Run to be Block()ing.  One answer is for
	 * m_connect to ignore timeout if a TryLock() reveals (by failing) that data
	 * is in the process of being sent.  In this case, it's worth marking
	 * m_connect as impatient, so that Run() can notice and, for example, not
	 * .Take() any more data for delivery, so as to send a data-sent message.
	 *
	 * What should we do if, while the "I've timed out" message is being sent
	 * round the message queue, some more data arrives after all ?  I think we
	 * should just forget the time-out happened; so perhaps the right answer is
	 * for m_connect to have an m_patient flag that says whether it's got a live
	 * timer; when the socket gets a time-out message, it can check m_patient
	 * and only act on the message if !m_patient.  This ties in adequately well
	 * with having Run() check m_patient to know whether to .Take() more data;
	 * if m_connect.OnTimeOut() didn't send a timeout message, because Run() was
	 * hard at work, it can still set m_patient = false to let Run() know to
	 * take a break. */
	struct timeval out;
	memset(&out, 0, sizeof(out));

	/* Setting out to zero restores the default - blocking behaviour.  If
	 * seconds is zero (and block is false), we should (set a tiny positive
	 * value, just to be sure, and then ...) make the socket non-blocking. */
	if (restore_default) // ignore seconds; restore system default idle timeout
		seconds = 1; // to simplify later conditionals for {B,Unb}lock().
	else if (seconds)
		out.tv_sec = seconds;
	else
		out.tv_usec = 1; // one millisecond

	if (seconds) m_connect->Block();
	// We *always* receive non-blocking, so don't try to configure receive time-out:
	// setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &out, sizeof(out));
	/* Ignoring return value (0 on success, else -1): we're a void function with
	 * no way to report error; POSIX doesn't require platforms to support
	 * setting these options anyway; and this is an optional interface.  We've
	 * done the best we can (i.e. asked); leave it at that. */
	/* Setting time-out on send may be pointless: Run() doesn't have a way to
	 * respond to it (it persists until data runs out).  But Run() could notice
	 * each time it gets EAGAIN and, as for when m_conn->m_patient is false,
	 * abstain from any further .Take() thereafter. */
	setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &out, sizeof(out));
	if (!seconds) m_connect->Unblock();
#endif // 0
}
#endif // SOCKET_SUPPORTS_TIMER

#ifdef OPSOCKET_OPTIONS
// virtual
OP_STATUS PosixSocket::SetSocketOption(OpSocketBoolOption option, BOOL value)
{
	return m_connect
		? m_connect->SetOption(option, value)
		: OpStatus::ERR_BAD_FILE_NUMBER;
}

// virtual
OP_STATUS PosixSocket::GetSocketOption(OpSocketBoolOption option, BOOL& value)
{
	return m_connect
		? m_connect->GetOption(option, value)
		: OpStatus::ERR_BAD_FILE_NUMBER;
}
#endif // OPSOCKET_OPTIONS

#ifdef OPSOCKET_PAUSE_DOWNLOAD
// virtual
OP_STATUS PosixSocket::PauseRecv()
{
	return m_connect ? m_connect->Pause(), OpStatus::OK : OpStatus::ERR;
}

// virtual
OP_STATUS PosixSocket::ContinueRecv()
{
	return m_connect ? m_connect->Resume(), OpStatus::OK : OpStatus::ERR;
}
#endif // OPSOCKET_PAUSE_DOWNLOAD

#undef POSIX_TCP_LEVEL

#endif // POSIX_OK_SOCKET
