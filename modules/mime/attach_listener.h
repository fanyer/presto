/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _ATTACH_LISTENER_H_
#define _ATTACH_LISTENER_H_

#if defined(NEED_URL_MIME_DECODE_LISTENERS)

#include "adjunct/desktop_util/adt/oplist.h"

class MIMEDecodeListener
{
	public:
		virtual ~MIMEDecodeListener() {}
		virtual OP_STATUS OnMessageAttachmentReady(URL &url) = 0;
};

#endif // M2

#endif // _ATTACH_LISTENER_H_
