/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995 - 2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */


#ifndef WEBSERVER_REFERENCES_H_
#define WEBSERVER_REFERENCES_H_
/* Used to solve a dangling pointer problem */


#include "modules/upnp/upnp_references.h"

/// Class to solve a compilation related to CORE-21620
class WebServerConnectionListenerReference
{
protected:
	// Reference object used to clean the pending pointers
	ReferenceObjectList<WebServerConnectionListenerReference> ref_obj_lsn;
	// Private constructor
	WebServerConnectionListenerReference(): ref_obj_lsn(this) { }
	
public:
	virtual ~WebServerConnectionListenerReference() { }
	
	// Return the reference object; 
	ReferenceObject<WebServerConnectionListenerReference> *GetReferenceObjectPtr() { return &ref_obj_lsn; }
};


#endif /*WEBSERVER_PRIVATE_GLOBAL_DATA_H_*/
