/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_SCHEME_H_
#define _URL_SCHEME_H_

#include "modules/util/smartptr.h"
#include "modules/url/url_scheme2.h"
#include "modules/url/url_sn.h"

struct protocol_selentry;
struct URL_Name_Components_st;

class URL_Scheme_Authority_Components : public Link, public OpReferenceCounter
{
public:
	const protocol_selentry *scheme_spec;
	OpStringS8	username;
	OpStringS8	password;
	ServerName_Pointer server_name;
	uint16		port;
	uint32		max_length;

#define URL_NAME_LinkID_None 0
#define URL_NAME_LinkID 1
#define URL_NAME_LinkID_Unique 2

public:

	URL_Scheme_Authority_Components();
	virtual ~URL_Scheme_Authority_Components();

	OP_STATUS Construct(URL_Name_Components_st *components);
	BOOL Match(URL_Name_Components_st *components);

	/** Calculates the number of characters is max needed for OutputString, excluding the terminating NULL. */
	unsigned GetMaxOutputStringLength() const;
	/** Concatenates scheme and authority portion to the already null terminated outputbuffer */
	void OutputString(URL::URL_NameStringAttribute password_hide, char *url_buffer, size_t buffer_len, int linkid) const;
	/** Concatenates scheme and authority portion to the already null terminated outputbuffer */
	void OutputString(URL::URL_UniNameStringAttribute password_hide, uni_char *url_buffer, size_t buffer_len) const;
};

typedef OpSmartPointerWithDelete<URL_Scheme_Authority_Components> URL_Scheme_Authority_Components_pointer;


#endif // _URL_SCHEME_H_

