/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef URL_EPH_H
#define URL_EPH_H

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT

#include "modules/url/url_man.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/datastream/fl_lib.h"
#include "platforms/gogi/src/gogi_protocol_handler.h"


class ImplProtocolHandlerListener;

class ExternalProtocolLoadHandler : public URL_LoadHandler
{
public:
	ExternalProtocolLoadHandler(URL_Rep *url_rep, MessageHandler *mh, URLType type);
	virtual ~ExternalProtocolLoadHandler();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	virtual	CommState Load();
	virtual unsigned ReadData(char *buf, unsigned len);
	virtual void EndLoading(BOOL force=FALSE) {};
	virtual void DeleteComm();

	OP_STATUS DataAvailable(void* data, UINT32 data_len);
	void LoadingFinished(unsigned int status = 0);
	void LoadingFailed(OP_STATUS error_code, unsigned int status = 0);
	OP_STATUS SetMimeType(const char* mime_type);

private:
	ImplProtocolHandlerListener* ph_listener;
	class GogiProtocolHandler* protocol_handler;
	class Window *window;
	DataStream_FIFO_Stream buffer;
	BOOL read_stop;
};


#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT

#endif // URL_EPH_H
