/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

//Partially processed Link HTTP headers
// Source in url_pd.cpp

#ifndef _URL_LINK_H_
#define _URL_LINK_H_

class HTTP_Link_Relations : public Link
{
private:
	OpStringS8 original_string;
	OpStringS8 link_uri;

	ParameterList parameters;

public:

	HTTP_Link_Relations();
	virtual ~HTTP_Link_Relations();

	void InitL(OpStringC8 link_header);
#ifdef DISK_CACHE_SUPPORT
	void InitL(DataFile_Record *link_record);
#endif

	OpStringC8 &OriginalString(){return original_string;}
	OpStringC8 &LinkURI(){return link_uri;}

	ParameterList &GetParameters(){return parameters;}

	HTTP_Link_Relations *Suc(){return (HTTP_Link_Relations *) Link::Suc();}
	HTTP_Link_Relations *Pred(){return (HTTP_Link_Relations *) Link::Pred();}

private:
	void Clear();
	void InitInternalL();

};

#endif // _URL_LINK_H_
