/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** @author Kajetan Switalski kswitalski@opera.com
*/
#ifndef LIBSSL_NEXTPROTOCOL_H
#define LIBSSL_NEXTPROTOCOL_H

#if defined _NATIVE_SSL_SUPPORT_
#include "modules/libssl/handshake/vectormess.h"

/**
 * This class represents NextProtocol message, which is a part of TLS Next Protocol Negotiation (NPN).
 * For more details about NPN see http://tools.ietf.org/html/draft-agl-tls-nextprotoneg-00.
 */
class SSL_NextProtocol_st : public SSL_VectorMessage
{
	SSL_varvector8 selected_protocol;
	SSL_varvector8 padding;
public:
	SSL_NextProtocol_st();

	/**
	 * Selects one of the protocols announced by the server, sets up the NextProtocol message and saves the chosen protocol 
	 * in pending->selected_next_protocol. The protocol is chosen based on pending->next_protocols_available (protocols announced 
	 * by the server and valus of preferences: PrefsCollectionNetwork::UseSpdy2, PrefsCollectionNetwork::UseSpdy3.
	 */
	virtual void SetUpMessageL(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_NextProtocol_st";}
#endif
};

#endif
#endif // LIBSSL_NEXTPROTOCOL_H
