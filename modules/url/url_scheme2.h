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

#ifndef _URL_SCHEME2_H_
#define _URL_SCHEME2_H_

class URL_Scheme_Authority_Components;
struct URL_Name_Components_st;

class URL_Scheme_Authority_List : public Head
{
public:
	virtual ~URL_Scheme_Authority_List();
	/** Find and, optionally, create a URL_Scheme_Authority_Components object that
	 *	matches the URL_Scheme_Authority_Components structure argument 
	 *	The components object is updated with the found/created pointer
	 */
	URL_Scheme_Authority_Components *FindSchemeAndAuthority(OP_STATUS &op_err, URL_Name_Components_st *components, BOOL create=FALSE);

};

#endif // _URL_SCHEME2_H_
