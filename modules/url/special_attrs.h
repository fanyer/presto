/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

#ifndef _URL_SPECIAL_ATTRS_H_
#define _URL_SPECIAL_ATTRS_H_

/** The handler for the URL::KHTTPSpecialMethodStr attribute
 *
 *	The applied string will, after preprocessing according to the requirments in step 2 through 4 of
 *	http://dev.w3.org/2006/webapi/XMLHttpRequest/#the-open-method (sec. 3.6.1) be used as the HTTP method
 *	sent to the server with the request for this URL.
 *
 *	Invalid methods (e.g. non-tokens, non-allowes values, or containing whitespace) will result in OpStatus::ERR_PARSING_FAILED being returned, 
 *	and the method becomes HTTP_METHOD_Invalid, which will result in a Bad request error without network accesss
 *	
 *	If the string is one of the known methods, that method value will be used, if it is any other valid value 
 *	the string will be used in the request. 
 *
 *	When a non-HEAD/GET method is used the URL is automatically made Unique. 
 */
class HTTP_MethodStrAttrHandler : public URL_DynamicStringAttributeHandler
{
public:
	virtual OP_STATUS OnSetValue(URL &url, OpString8 &in_out_value, BOOL &set_value) const;
	virtual OP_STATUS OnGetValue(URL &url, OpString8 &in_out_value) const;
};

#endif //_URL_SPECIAL_ATTRS_H_
