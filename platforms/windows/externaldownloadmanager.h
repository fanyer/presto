/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Author: shuais
*/

#ifndef EXTERNAL_DOWANLOAD_MANAGER_H
#define EXTERNAL_DOWANLOAD_MANAGER_H

#include "adjunct/quick/managers/DownloadManagerManager.h"

class ThunderDownloadManager : public ExternalDownloadManager
{
public:
	static ExternalDownloadManager* Construct();
	virtual OP_STATUS Run(const uni_char* url);
};

class QQDownloadManager : public ExternalDownloadManager
{
public:
	static ExternalDownloadManager* Construct();
	virtual OP_STATUS Run(const uni_char* url);
};

class FlashgetDownloadManager : public ExternalDownloadManager
{
public:
	static ExternalDownloadManager* Construct();
	virtual OP_STATUS Run(const uni_char* url);
};

BOOL GetProtocolInfo(const uni_char* protocol, OpString& name, OpString& application, Image& icon);

#endif //EXTERNAL_DOWANLOAD_MANAGER_H
