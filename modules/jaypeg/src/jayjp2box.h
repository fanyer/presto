/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef JAYJP2BOX_H
#define JAYJP2BOX_H

#include "modules/jaypeg/jaypeg.h"
#ifdef JAYPEG_JP2_SUPPORT

#define JP2_BOX_SIGNATURE	0x6A502020 // verified
#define JP2_BOX_PROFILE		0x66747970 // verified
#define JP2_BOX_HEADER		0x6A703268 // verified
# define JP2_BOX_HEADER_IMAGE	0x69686472
// Bits per component
# define JP2_BOX_HEADER_BPC	0x62706363
// Color specification (color space)
# define JP2_BOX_HEADER_COL_SPEC 0x636F6C72
// Palette
# define JP2_BOX_HEADER_PAL	0x7066C72
// Component definition
# define JP2_BOX_HEADER_COMP_DEF 0x63646566
// Resolution
# define JP2_BOX_HEADER_RES	0x72657320
// Capture resolution
# define JP2_BOX_HEADER_CAP_RES	0x72657363
// Default display res
# define JP2_BOX_HEADER_DISP_RES 0x72657364
#define JP2_BOX_CODESTREAM	0x6A703263
// Intellectual property
#define JP2_BOX_IP		0x6A703269
#define JP2_BOX_XML		0x786D6C20 // verified
#define JP2_BOX_UUID		0x75756964
#define JP2_BOX_UUIDINFO	0x75696E66
# define JP2_BOX_UUIDINFO_LIST	0x75637374
# define JP2_BOX_UUIDINFO_URL	0x75726C20


class JayJP2Box
{
public:
	JayJP2Box();
	int init(JayJP2Box *prev, class JayStream *stream);
	int initMarker(JayJP2Box *prev, class JayStream *stream);

	void setLength(unsigned int len);
	
	unsigned int reduceLength(unsigned int len);
	bool lengthLeft(unsigned int len);
	bool isEmpty();
	JayJP2Box *getParent();
	unsigned int getType();
	unsigned int getMarker();

	void makeInvalid();
private:
	unsigned int lolength;
	unsigned int hilength;
	unsigned int type;
	JayJP2Box *parent;
};
#endif // JAYPEG_JP2_SUPPORT
#endif

