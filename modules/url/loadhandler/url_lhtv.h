/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef HAS_ATVEF_SUPPORT

#ifndef URL_LHTV_H
#define URL_LHTV_H

class URL_Tv_LoadHandler
    : public URL_LoadHandler
{
private:

    void HandleCallback();

public:

    URL_Tv_LoadHandler(URL_Rep *url_rep, MessageHandler *mh);

    ~URL_Tv_LoadHandler();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
    {
	// Nothing, but is declared pure virtual in class SComm
    };

    virtual CommState Load();

    virtual unsigned ReadData(char *buffer, unsigned buffer_len)
    {
	return 0;
    };

    virtual void EndLoading(BOOL force = FALSE)
    {
    };

    CommState ConnectionEstablished()
    {
	return COMM_LOADING;
    };
};

#endif /* URL_LHTV_H */

#endif // HAS_ATVEF_SUPPORT
