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

#ifndef _URL_DEBUG_H_
#define _URL_DEBUG_H_

// Debug utility for printing URLs
class URL_Rep;
inline const char *DebugGetURLstring(URL_Rep *url){
	OpStringC8 url_string(url->GetAttribute(URL::KName_Escaped));
	
	return (url_string.HasContent() ? url_string.CStr() : "");
}
inline const char *DebugGetURLstring(URL &url){
	OpStringC8 url_string(url.GetAttribute(URL::KName_Escaped));
	
	return (url_string.HasContent() ? url_string.CStr() : "");
}


#endif // _URL_DEBUG_H_
