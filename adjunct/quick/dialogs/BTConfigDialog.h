/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BTConfigDialog_H
#define BTConfigDialog_H

#include "SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/m2/src/util/buffer.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/bittorrent/bt-util.h"
#include "adjunct/bittorrent/connection.h"
#include "modules/windowcommander/OpTransferManager.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"

class BTConfigDialog : public Dialog,
	public OpTransferListener,
	public XMLTokenHandler,
	public XMLParser::Listener,
	public OpSocketListener
{
public:

	BTConfigDialog();
	virtual ~BTConfigDialog();

	Type				GetType()				{return DIALOG_TYPE_BITTORRENT_CONFIGURATION;}
	DialogType			GetDialogType()			{return TYPE_OK_CANCEL;}
	const char*			GetWindowName()			{return "BitTorrent Configuration Dialog";}
	const char*			GetHelpAnchor()			{return "bittorrent.html#prefs";}
	BOOL				GetModality()			{return TRUE;}
	BOOL				GetIsBlocking()			{return TRUE;}
	void				OnInit();
	void				OnCancel();
	UINT32				OnOk();
	BOOL				OnInputAction(OpInputAction *action);
	void				OnClick(OpWidget *widget, UINT32 id);

private:

	// MpSocketObserver.
    virtual void OnSocketInstanceNumber(int instance_number) { }
	virtual void OnSocketConnected(OpSocket* socket) { }
	virtual void OnSocketDataReady(OpSocket* socket) { }
	virtual void OnSocketClosed(OpSocket* socket) { }
	virtual void OnSocketConnectionRequest(OpSocket* socket) {};
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) {}
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error socket_error) { }
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, unsigned int bytes_sent) { }
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent) { }

	// OpTransferListener
	void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	void OnReset(OpTransferItem* transferItem) { }
	void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) {};
	// XML Parsing callbacks.
	void StartElementHandler(const XMLToken &token);
	void EndElementHandler(const XMLToken &token);
	void CharacterDataHandler(const uni_char *s, int len);

	// Implementation of the XMLTokenHandler API
	virtual XMLTokenHandler::Result	HandleToken(XMLToken &token);
	// Implementation of the XMLParser::Listener API
	virtual void					Continue(XMLParser *parser);
	virtual void					Stopped(XMLParser *parser);


private:

	P2PSocket			m_listensocket;
	OpString			m_content_string;
	OpString			m_return_status;
	OpString			m_source_ip_address;
	INT32				m_feed_id;
	OpEdit				*m_port;
	OpTransferItem*		m_transfer_item;

};

#endif // BTConfigDialog_H
