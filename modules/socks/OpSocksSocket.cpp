/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#ifdef SOCKS_SUPPORT

#include "modules/socks/OpSocksSocket.h"
#include "modules/socks/socks_module.h"

//#define LOG_SOCKS_ACTIVITY //uncomment this line if you want to enable socks logging in debug mode
#ifdef  LOG_SOCKS_ACTIVITY

class SocksLogger {
	OpString  buf;
public:
	void  logIt() {
		//OP_NEW_DBG("-", "SOCKS");
		//OP_DBG_LEN(buf, buf.Length());
		OutputDebugString(buf.CStr());
		buf.Delete(0);
	}

	~SocksLogger() {
		logIt();
	}

	SocksLogger& operator << (const uni_char* str) {
		buf.Append(str);
		return *this;
	}

	SocksLogger& operator << (const char* str) {
		buf.Append(str);
		return *this;
	}

	SocksLogger& operator << (const OpString& str) {
		buf.Append(str);
		return *this;
	}

	SocksLogger& operator << (int n) {
		buf.AppendFormat(UNI_L("%d"), n);
		return *this;
	}

	SocksLogger& operator << (const void* ptr) {
		buf.AppendFormat(UNI_L("0x%x"), (UINTPTR)ptr);
		return *this;
	}

	SocksLogger& operator << (OpSocketAddress* addr) {
		OpString temp;
		if (addr)
			addr->ToString(&temp);

		return *this << temp;
	}
};

#define DEBUG_OUT(STREAM)	SocksLogger() << STREAM << "\n"
#else
#define DEBUG_OUT(STREAM)	do {} while (0)
#endif // LOG_SOCKS_ACTIVITY
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


/** Global PrefsCollectionSocks object (singleton). */
#define g_pc_network (g_opera->prefs_module.PrefsCollectionNetwork())


OpSocksSocket::OpSocksSocket(OpSocketListener* listener, BOOL secure, OpSocket &inner_socket)
	: m_secure(secure)
	, m_socks_socket(&inner_socket)
	, m_target_host()
	, m_target_port(0)
	, m_actual_target_address(NULL)
	, m_listener(listener)
	, m_nb_sent_ignore(0)
	, m_nb_recv_ignore(0)
	, m_host_resolver(NULL)
	, m_socks_address(NULL)
	, m_socks_server_name(NULL)
	, m_state(SS_CONSTRUCTED)
{
	m_socks_socket->SetListener(this);
}

OpSocksSocket::~OpSocksSocket()
{
	DEBUG_OUT(this << "->~destroy " << " (connected to " << m_target_host << ")");
	if (m_socks_socket != NULL) OP_DELETE(m_socks_socket);
	if (m_actual_target_address != NULL) OP_DELETE(m_actual_target_address);
	if (m_host_resolver != NULL) OP_DELETE(m_host_resolver);
	if (m_socks_address != NULL) OP_DELETE(m_socks_address);
	if (m_socks_server_name != NULL) m_socks_server_name = NULL;
}


OP_STATUS
OpSocksSocket::Create(OpSocket** psocket, OpSocket& inner_socket, OpSocketListener* listener, BOOL secure, ServerName* socks_server_name, UINT socks_server_port)
{
	OpAutoPtr<OpSocksSocket> socket(OP_NEW(OpSocksSocket, (listener, secure, inner_socket)));
	RETURN_OOM_IF_NULL(socket.get());

	RETURN_IF_ERROR(OpSocketAddress::Create(&socket->m_socks_address));

	socket->m_socks_address->SetPort(socks_server_port);

	if (socks_server_name->IsHostResolved())
	{
		socket->m_socks_address->Copy(socks_server_name->SocketAddress());
		socket->m_state = SS_SOCKS_RESOLVED;
		*psocket = socket.release();
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(SocketWrapper::CreateHostResolver(&socket->m_host_resolver, socket.get()));

	socket->m_socks_server_name = socks_server_name;
	RETURN_IF_ERROR(socket->m_host_resolver->Resolve(socks_server_name->UniName()));

	socket->m_state = SS_RESOLVING_SOCKS;

	*psocket = socket.release();
	return OpStatus::OK;
}

void OpSocksSocket::OnHostResolved(OpHostResolver* host_resolver)
{
	OP_ASSERT(m_host_resolver == host_resolver);

	if (m_host_resolver != host_resolver || host_resolver->GetAddressCount() == 0)
	{
		Error(SS_SOCKS_RESOLVE_FAILED);
		return;
	}

	host_resolver->GetAddress(m_socks_address, 0);
	OP_DELETE(m_host_resolver);
	m_host_resolver = NULL;
	if (m_socks_server_name != NULL && m_socks_address->IsValid())
	{
		// cache resolved address so we don't need to resolve it every other next:
		m_socks_server_name->SetSocketAddress(m_socks_address);
		m_socks_server_name = NULL;
	}
	switch (m_state)
	{
	case SS_RESOLVING_SOCKS:
		m_state = SS_SOCKS_RESOLVED;
		return;

	case SS_WAITING_RESOLVING_SOCKS:
		CommonConnect();
		break;

	default:
		OP_ASSERT(!"Unexpected SOCKS socket state !");
		break;
	}
}

void OpSocksSocket::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	m_state = SS_CONNECTION_FAILED;
}

void
OpSocksSocket::Error(State state)
{
	OP_ASSERT(state < 0);
	if (m_listener)
	{
		switch (state)
		{
		default:
			OP_ASSERT(!"Unexpected SOCKS socket state !");
			// fall through
		case SS_CONNECTION_FAILED:
			DEBUG_OUT(this << " SS_CONNECTION_FAILED");
			m_listener->OnSocketConnectError(this, CONNECTION_FAILED);
			break;
		case SS_AUTH_FAILED:
			DEBUG_OUT(this << " SS_CONNECTION_REFUSED");
			m_listener->OnSocketConnectError(this, CONNECTION_REFUSED);
			break;
		case SS_SOCKS_RESOLVE_FAILED:
			DEBUG_OUT(this << " SS_SOCKS_RESOLVE_FAILED");
			if (m_state == SS_WAITING_RESOLVING_SOCKS)
				m_listener->OnSocketConnectError(this, CONNECTION_REFUSED);
			break;
		}
	}
	m_state = state;
}

OP_STATUS
OpSocksSocket::Connect(const uni_char* target_host, signed short target_port)
{
	OP_STATUS stat = m_target_host.Set(target_host);
	if (OpStatus::IsError(stat))
	{
		Error(SS_CONNECTION_FAILED);
		return stat;
	}

	m_target_port = target_port;

	return CommonConnect();
}

OP_STATUS
OpSocksSocket::Connect(OpSocketAddress* socket_address)
{
	OP_ASSERT(socket_address->IsValid());

	DEBUG_OUT(this << "->Connect " << " to " << socket_address);

	// copy the target address:
	OP_STATUS stat = socket_address->ToString(&m_target_host);
	if (OpStatus::IsError(stat))
	{
		Error(SS_CONNECTION_FAILED);
		return stat;
	}

	m_target_port = socket_address->Port();

	return CommonConnect();
}

OP_STATUS
OpSocksSocket::CommonConnect()
{
	OP_STATUS stat = OpStatus::OK;

	if (m_state == SS_RESOLVING_SOCKS)
	{
		m_state = SS_WAITING_RESOLVING_SOCKS;
		return OpStatus::OK;
	}

	if (m_state == SS_SOCKS_RESOLVE_FAILED)
	{
		m_state = SS_WAITING_RESOLVING_SOCKS; // required to fire the approp. error listener
		Error(SS_SOCKS_RESOLVE_FAILED); // that is to fire the approp. error listener
		return OpStatus::ERR;
	}

	OP_ASSERT(m_socks_socket); // should have been set by the constructor
	m_socks_socket->SetListener(this);

	// connect socket to the socks server:
	OpSocketAddress* socksServerAddress;

	if (m_socks_address != NULL)
		socksServerAddress = m_socks_address;
	else
		socksServerAddress = g_opera->socks_module.GetSocksServerAddress();

	if (!socksServerAddress->IsValid()) // invalid or unresolved socks proxy address
		goto failure;

	DEBUG_OUT(this << " SS_SOCKET_TO_SOCKS_CREATED");
	m_state = SS_SOCKET_TO_SOCKS_CREATED;
	stat = m_socks_socket->Connect(socksServerAddress);
		if (OpStatus::IsError(stat)) goto failure;

	return OpStatus::OK;

failure:
	Error(SS_CONNECTION_FAILED);
	return stat;
}

void
OpSocksSocket::OnSocketConnected(OpSocket* socket)
{
	OP_ASSERT(socket == m_socks_socket);
	OP_ASSERT(m_state == SS_SOCKET_TO_SOCKS_CREATED);
	// +----+----------+----------+
	// |VER | NMETHODS | METHODS  |
	// +----+----------+----------+
	// | 1  |    1     | 1 to 255 |
	// +----+----------+----------+

	DEBUG_OUT(this << "->OnSocketConnected");

	OP_STATUS stat;
	if (g_opera->socks_module.HasUserPassSpecified())
	{
		UINT8 rq1_data[] = { 5, 2, 0, 2 }; // support 2 methods- <no auth> and <username/password> // ARRAY OK stansialvj 2010-12-01
		m_nb_sent_ignore += ARRAY_SIZE(rq1_data);
		stat = socket->Send(rq1_data, ARRAY_SIZE(rq1_data));
	}
	else
	{
		UINT8 rq1_data[] = { 5, 1, 0 }; // support 1 method- <no auth> // ARRAY OK stansialvj 2010-12-01
		m_nb_sent_ignore += ARRAY_SIZE(rq1_data);
		stat = socket->Send(rq1_data, ARRAY_SIZE(rq1_data));
	}

	if (!OpStatus::IsError(stat))
	{
		DEBUG_OUT(this << " SS_RQ1_SENT");
		m_state = SS_RQ1_SENT;
	}
	else
		// in case sending over the m_socks_socket fails, signal it to this socket's listener:
		Error(SS_CONNECTION_FAILED);
}


#define NEEDS_NB_INBUF(NBR, BUF) \
{ \
	UINT nb_recv = 0, nb_expected = (NBR);\
	if (nb_expected) \
	{ \
		OP_STATUS stat = socket->Recv(buf, nb_expected, &nb_recv); \
		if (nb_recv < (NBR) || OpStatus::IsError(stat)) \
			return; \
	} \
}


void
OpSocksSocket::OnSocketDataReady(OpSocket* socket)
{
	OP_ASSERT(socket == m_socks_socket);
	OP_STATUS stat = OpStatus::OK;
	UINT8  buf[32]; // ARRAY OK stansialvj 2010-12-01
	DEBUG_OUT(this << "->DataReady case #" << m_state);
	switch (m_state)
	{
	case SS_SOCKET_TO_SOCKS_CREATED:
		OP_ASSERT(!"Unexpected SOCKS socket state"); // ?
		// ... are we really meant to fall through here ? or break; ?
	case SS_RQ1_SENT:
		DEBUG_OUT(this << "->DataReady case SS_RQ1_SENT");
		NEEDS_NB_INBUF(2, buf);
		if (buf[1] == 0) // <no auth>
		{
			DEBUG_OUT(this << "->DataReady case SS_RQ1_SENT :> NO auth required");
			SocksSendConnectRequest();
		}
		else if (buf[1] == 2) // <username/password>
		{
			OP_ASSERT(g_opera->socks_module.HasUserPassSpecified());
			const char* user_pass_datagram = g_opera->socks_module.GetUserPassDatagram();
			UINT  length = g_opera->socks_module.GetUserPassDatagramLength();
			m_nb_sent_ignore += length;
			stat = socket->Send(user_pass_datagram, length);
			if (OpStatus::IsSuccess(stat))
			{
				DEBUG_OUT(this << "->DataReady case SS_RQ1_SENT :> SS_USERNAME_PASSWORD_SENT");
				m_state = SS_USERNAME_PASSWORD_SENT;
			}
			else
			{
				// in case sending over the m_socks_socket fails, signal it to this socket's listener:
				Error(SS_CONNECTION_FAILED);
			}
		}
		return;

	case SS_USERNAME_PASSWORD_SENT:
		// Server response should look like this:
		// +----+--------+
		// |VER | STATUS |
		// +----+--------+
		// | 1  |   1    |
		// +----+--------+

		DEBUG_OUT(this << "->DataReady case SS_USERNAME_PASSWORD_SENT");

		NEEDS_NB_INBUF(2, buf);
		OP_ASSERT(buf[0] == 1);
		if (buf[1] != 0)
		{
			DEBUG_OUT(this << "->DataReady case SS_USERNAME_PASSWORD_SENT: CONNECTION_REFUSED");
			Error(SS_AUTH_FAILED);
			return;
		}

		//CONSUME_NBR(2); // consume the top 2 octets
		SocksSendConnectRequest();
		return;

	case SS_TARGET_ADDR_SENT:
		// +----+-----+-------+------+----------+----------+
		// |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
		// +----+-----+-------+------+----------+----------+
		// | 1  |  1  | X'00' |  1   | Variable |    2     |
		// +----+-----+-------+------+----------+----------+
		// where REP is as follows:
		//   o  X'00' succeeded
		//   o  X'01' general SOCKS server failure
		//   o  X'02' connection not allowed by ruleset
		//   o  X'03' Network unreachable
		//   o  X'04' Host unreachable
		//   o  X'05' Connection refused
		//   o  X'06' TTL expired
		//   o  X'07' Command not supported
		//   o  X'08' Address type not supported
		//   o  X'09' to X'FF' unassigned
		//
		// where ATYP (address type) is as follows:
		//   o  IP V4 address: X'01'
		//   o  DOMAINNAME: X'03'
		//   o  IP V6 address: X'04'

		DEBUG_OUT(this << "->DataReady case SS_TARGET_ADDR_SENT");

		NEEDS_NB_INBUF(5, buf);
		OP_ASSERT(buf[0] == 5);
		OP_ASSERT(buf[2] == 0);
		UINT rep; rep = buf[1];

		if (rep != 0)
		{
			Error(rep == 2 || rep == 5 ? SS_AUTH_FAILED : SS_CONNECTION_FAILED);
			return;
		}

		m_state = SS_TARGET_CONNECTED;
		if (buf[3] == 1) // IPv4 address
		{
			// 5 left to read -- 3 bytes IPv4 addr left + 2 bytes port left
			UINT bytesExpected = 5;
			UINT bytesReceived = 0;
			buf[0] = buf[4];
			stat = socket->Recv(buf+1, bytesExpected, &bytesReceived);
			if (OpStatus::IsError(stat) || bytesReceived < bytesExpected) // TODO: we should really wait for more data rather than fail the connection.
				goto conn_failed;

			if (buf[0] || buf[1] || buf[2] || buf[3])
			{
				stat = OpSocketAddress::Create(&m_actual_target_address);
				if (OpStatus::IsError(stat))
					goto conn_failed;

				uni_char str[16]; // ARRAY OK stansialvj 2010-12-01
				uni_snprintf(str, ARRAY_SIZE(str), UNI_L("%u.%u.%u.%u"), buf[0], buf[1], buf[2], buf[3]);
				m_actual_target_address->FromString(str);
				m_actual_target_address->SetPort((UINT)(buf[4] << 8) + buf[5]);
				DEBUG_OUT(this << "->DataReady case SS_TARGET_CONNECTED: actual target IP: " << str);
			}
			else
			{
				DEBUG_OUT(this << "->DataReady case SS_TARGET_CONNECTED: actual target IP: N/A");
			}
		}
		else if (buf[3] == 3) // domain name
		{
			UINT bytesExpected = buf[4] + 2;
			UINT bytesReceived = 0;
			char domainName[255+2+1]; // ARRAY OK stansialvj 2010-12-01
			stat = socket->Recv(domainName, bytesExpected, &bytesReceived); // buf[4] bytes domain name left + 2 bytes port left
			if (OpStatus::IsError(stat) || bytesReceived < bytesExpected) // TODO: we should really wait for more data rather than fail the connection.
				goto conn_failed;
			stat = OpSocketAddress::Create(&m_actual_target_address);
			if (OpStatus::IsError(stat))
				goto conn_failed;

			OpString16 str;
			if (OpStatus::IsError(str.Set(domainName, bytesReceived - 2)))
				goto conn_failed;
			if (OpStatus::IsError(m_actual_target_address->FromString(str.CStr())))
				goto conn_failed;
			m_actual_target_address->SetPort((UINT)(domainName[bytesReceived-2] << 8) + domainName[bytesReceived-1]);

			DEBUG_OUT(this << "->DataReady case SS_TARGET_CONNECTED: actual target hostname: " << str);
		}
		else if (buf[3] == 4) // IPv6 ?! just skip -- don't de-serialize
		{
			UINT bytesExpected = 15 + 2; // 15 bytes left over from the IPv6 addres + 2 bytes port number
			UINT bytesReceived = 0;
			char ipBuf[15 + 2];
			stat = socket->Recv(ipBuf, bytesExpected, &bytesReceived);
			if (OpStatus::IsError(stat) || bytesReceived < bytesExpected) // TODO: we should really wait for more data rather than fail the connection.
				goto conn_failed;

			DEBUG_OUT(this << "->DataReady case SS_TARGET_CONNECTED: but IPv6 ?!");
		}
		else
		{
			OP_ASSERT(!"Unexpected fourth byte");
		conn_failed:
			Error(SS_CONNECTION_FAILED);
			return;
		}

		if (m_listener)
			m_listener->OnSocketConnected(this);
		return;

	default: // unhandled enum case?!!
	case SS_CONSTRUCTED:
	case SS_AUTH_FAILED:
		DEBUG_OUT(this << "->DataReady socket in trouble " << m_target_host);
		OP_ASSERT(!"Unexpected SOCKS socket state");
	case SS_CONNECTION_FAILED:
	case SS_TARGET_CONNECTED:
		if (m_listener)
			m_listener->OnSocketDataReady(this);
		return;
	}
}


OP_STATUS
OpSocksSocket::SocksSendConnectRequest()
{
    //  +----+-----+-------+------+----------+----------+
    //  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
    //  +----+-----+-------+------+----------+----------+
    //  | 1  |  1  | X'00' |  1   | Variable |    2     |
    //  +----+-----+-------+------+----------+----------+

	if (m_target_port == 0)
	{
		Error(SS_CONNECTION_FAILED);
		return OpStatus::ERR;
	}
	OpString str_host_name;
	OP_STATUS stat = str_host_name.Set(m_target_host);
	if (OpStatus::IsError(stat))
	{
	error:
		Error(SS_CONNECTION_FAILED);
		return stat;
	}
	if (0 < str_host_name.FindFirstOf(':')) // IPv6 address is not supported yet
	{
		stat = OpStatus::ERR;
		goto error;
	}

	INT length = (INT)str_host_name.Length();
	INT ndots = 0, last_dot_idx = -1;
	UINT8 addr[4]; // ARRAY OK stansialvj 2010-12-01
	BOOL is_IPv4_addr = TRUE; // IPv4 address vs. hostname

	// If RemoteSocksDNS is enabled we let the proxy handle all IPv4/host name issues.
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RemoteSocksDNS))
		is_IPv4_addr = FALSE;

	for (INT i = 0; is_IPv4_addr && i < length; ++i)
	{
		uni_char c = str_host_name.DataPtr()[i];
		if (c == '.')
		{
			if (ndots >= 3 || i - last_dot_idx <= 1) // that's the 4th dot or two conseq dots which *is* a problem
				is_IPv4_addr = FALSE;
			else
			{
				str_host_name.DataPtr()[i] = uni_char(0);
				addr[ndots++] = uni_atoi(str_host_name.CStr() + last_dot_idx + 1);
				str_host_name.DataPtr()[i] = uni_char('.');
				last_dot_idx = i;
			}
		}
		else if (c < '0' || '9' < c) // not a digit
			is_IPv4_addr = FALSE;
	}
	if (is_IPv4_addr && ndots != 3 || length - last_dot_idx <= 1)
		is_IPv4_addr = FALSE;

	UINT  port = m_target_port;
	UINT8 port_hi = (port >> 8) & 0xff;
	UINT8 port_lo = port & 0xff;

	if (is_IPv4_addr)	// IPv4 address:
	{
		DEBUG_OUT(this << "->SocksSendConnectRequest target address: " << str_host_name);
		addr[3] = uni_atoi(str_host_name.CStr() + last_dot_idx + 1);
		UINT8 buf[10] = { 5, 1, 0, 1, addr[0], addr[1], addr[2], addr[3], port_hi, port_lo }; // ARRAY OK stansialvj 2010-12-01
		m_nb_sent_ignore += ARRAY_SIZE(buf);
		stat = m_socks_socket->Send(buf, ARRAY_SIZE(buf));
	}
	else
	{
		if (length > 255)
		{
			OP_ASSERT(!"Host name is way too long!");
			stat = OpStatus::ERR;
			goto error;
		}
		DEBUG_OUT(this << "->SocksSendConnectRequest target name: " << str_host_name);
		UINT8 buf[255+8]; // ARRAY OK stansialvj 2010-12-01
		UINT8 *put = buf;
		*put++ = 5;
		*put++ = 1;
		*put++ = 0;
		*put++ = 3;
		*put++ = (UINT8)length;
		for (INT i=0; i < length; i++)
			*put++ = (UINT8)str_host_name.DataPtr()[i];
		*put++ = port_hi;
		*put++ = port_lo;

		m_nb_sent_ignore += (put - buf);
		stat = m_socks_socket->Send(buf, (put - buf));
	}

	if (OpStatus::IsError(stat))
	{
		goto error;
	}
	else
	{
		DEBUG_OUT(this << "->SocksSendConnectRequest m_state: SS_TARGET_ADDR_SENT");
		m_state = SS_TARGET_ADDR_SENT;
	}

	return stat;
}

OP_STATUS  OpSocksSocket::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	DEBUG_OUT(this << "->Recv ");
	if (m_socks_socket)
		return m_socks_socket->Recv(buffer, length, bytes_received);
	else
		return OpStatus::ERR;
}


OP_STATUS  OpSocksSocket::Send(const void* data, UINT length)
{
	DEBUG_OUT(this << "->Send (" << length << ")");
	if (m_socks_socket)
		return m_socks_socket->Send(data, length);
	else
		return OpStatus::ERR;
}

	/** Data has been sent on the socket. */
void  OpSocksSocket::OnSocketDataSent(OpSocket* socket, UINT bytes_sent)
{
	if (m_nb_sent_ignore >= bytes_sent)
	{
		m_nb_sent_ignore -= bytes_sent;
		return;
	}

	if (m_nb_sent_ignore > 0)
	{
		bytes_sent -= m_nb_sent_ignore;
		m_nb_sent_ignore = 0;
	}

	DEBUG_OUT(this << "->OnSocketDataSent (" << bytes_sent << ")");
	if (m_listener)
	{
		m_listener->OnSocketDataSent(this, bytes_sent);
	}
	else
	{
		DEBUG_OUT(this << "\t\t... no listener!");
	}
}

	/** The socket has closed. */
void  OpSocksSocket::OnSocketClosed(OpSocket* socket)
{
	if (m_listener)
		m_listener->OnSocketClosed(this);
	m_state = SS_CONNECTION_CLOSED;
}

	/** An error has occured on the socket whilst connecting. */
void  OpSocksSocket::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	if (m_listener)
		m_listener->OnSocketConnectError(this, error);
	if (m_state >= 0)
		m_state = SS_CONNECTION_FAILED;
}


	/** An error has occured on the socket whilst receiving. */
void  OpSocksSocket::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	if (m_listener)
		m_listener->OnSocketReceiveError(this, error);
	if (m_state >= 0)
		m_state = SS_CONNECTION_FAILED;
}

	/** An error has occured on the socket whilst sending. */
void  OpSocksSocket::OnSocketSendError(OpSocket* socket, OpSocket::Error error)
{
	if (m_listener)
		m_listener->OnSocketSendError(this, error);
	if (m_state >= 0)
		m_state = SS_CONNECTION_FAILED;
}
	/** An error has occured on the socket whilst closing. */
void  OpSocksSocket::OnSocketCloseError(OpSocket* socket, OpSocket::Error error)
{
	if (m_listener)
		m_listener->OnSocketCloseError(this, error);
	if (m_state >= 0)
		m_state = SS_CONNECTION_CLOSED;
}

#ifdef SOCKET_LISTEN_SUPPORT
OP_STATUS  OpSocksSocket::Listen(OpSocketAddress* socket_address, int queue_size)
{
	OP_ASSERT(!"Listen() support not provided with SOCKS");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS  OpSocksSocket::Accept(OpSocket* socket)
{
	OP_ASSERT(!"No SOCKS Listen() support, so how did Accept() get called ?");
	return OpStatus::ERR_NOT_SUPPORTED;
}

void  OpSocksSocket::OnSocketConnectionRequest(OpSocket* socket)
{
	OP_ASSERT(!"No SOCKS Listen() support, so who called OnSocketConnectionRequest() ?");
}

void  OpSocksSocket::OnSocketListenError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(!"No SOCKS Listen() support, so who called OnSocketListenError() ?");
}
#endif // SOCKET_LISTEN_SUPPORT

#undef NEEDS_NB_INBUF
#undef LOG_SOCKS_ACTIVITY
#undef DEBUG_OUT

#endif //SOCKS_SUPPORT
