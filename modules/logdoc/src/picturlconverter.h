/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef PICTURLCONVERTER_H
#define PICTURLCONVERTER_H

class PictUrlConverter
{
public:
	static void			GetLocalUrlL(const uni_char *pict_url, int pict_url_len,
							 OpString &out_url_str);
	static OP_STATUS	GetLocalUrl(const uni_char *pict_url, int pict_url_len,
							 OpString &out_url_str);
};

#endif // PICTURLCONVERTER_H
