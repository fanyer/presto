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

#ifndef _URL_REMAP_H_
#define _URL_REMAP_H_

/** Remap a resoved url to another, for example to associate a
*	known URL alias with the real resource.
*
*	NOTE: This function !!MUST NOT!! access the url module, directly or indirectly.
*
*	@param	original_url	The original URLs absolute URL form
*	@param	remapped_url	If the original URL is an alias write
*							the new URL into this string.
*							The resulting string SHOULD be an absolute URL,
*							if it isn't it will be resolved relative the the
*							same base URL used for resolving the original URL.
*	@param	context_id		Optional parameter with the URL cache context id 
*							which the remapper can use in it's determination of
*							if, or how, to remap. Controlled by API_URL_USE_REMAP_RESOLVING_CONTEXT_ID
*
*	@return	OP_STATUS		OpStatus::OK or other success if the operation was successful, if 
*							remapped_url is non-NULL (it may be zero-length) that string is used as described
*							If this status is an error then the operation failed, e.g. due to OOM.
*							NOTE: this replaces the BOOL from the old RemapUrlL API.
*/
OP_STATUS RemapUrl(const OpStringC &original_url, OpString &remapped_url
#ifdef NEED_URL_REMAP_RESOLVING_CONTEXT_ID
				   , URL_CONTEXT_ID context_id
#endif
				   );

#endif
