/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef WEBMAILMANAGER_H
#define WEBMAILMANAGER_H

#include "modules/util/adt/opvector.h"
#include "adjunct/quick/managers/DesktopManager.h"
class PrefsFile;

class WebmailManager : public DesktopManager<WebmailManager>
{
public:
	enum HandlerSource
	{
		HANDLER_PREINSTALLED,  // Handler is shipped with Opera
		HANDLER_CUSTOM         // Handler is added by user (HTML5 protocol handler)
	};

public:
	/** Initialization, run this before calling any other functions on this object
	  */
	OP_STATUS					Init();

	/** @return Number of providers known to WebmailManager
	  */
	unsigned int				GetCount() const;

	/** Get the unique, persistent ID for a mail provider
	  * @param index A number less than GetCount()
	  * @return Unique, persistent ID for provider at index, can be used for functions that have ID parameter
	  */
	unsigned int				GetIdByIndex(unsigned int index) const;

	/** Get the human-readable name of a mail provider
	  * @param id ID of provider to get name for
	  * @param name Human-readable name of provider, e.g. "Gmail"
	  */
	OP_STATUS					GetName(unsigned int id, OpString& name) const;

	/** Get the human-readable URL of the favicon mail provider
	  * @param id ID of provider to get name for
	  * @param name Human-readable URL of provider favicon, e.g. "http://mail.google.com/favicon.ico"
	  */
	OP_STATUS					GetFaviconURL(unsigned int id, OpString& favicon_url) const;

	/** Get the human-readable URL of the provider
	 * @param id ID of provider to get name for
	 * @param name Human-readable URL of provider"
	 */
	OP_STATUS                   GetURL(unsigned int id, OpString& url) const;

	/** Retrieve a URL to use for using a mailto: URL with a specific provider
	  * @param id ID of provider to get URL for
	  * @param mailto_url mailto: URL to use
	  * @param target_url Outputs URL that will compose new mail
	  */
	OP_STATUS					GetTargetURL(unsigned int id, const URL& mailto_url, URL& target_url) const;

	/** Retrieve a URL as a string to use for using a mailto: URL with a specific provider
	  * @param id ID of provider to get URL as string for
	  * @param mailto_url mailto: URL as string to use in
	  * @param target_url Outputs URL as string that will compose new mail
	  */
	OP_STATUS 					GetTargetURL(unsigned int id, const OpStringC8& mailto_url_str, OpString8& target_url_str) const;

	/** Called when a web handler for the mailto protocol is added
	  * @param url Web address to the handler (page)
	  * @param description User firendly handler name
	  * @param set_default If TRUE, Opera will set this handler as default mail handler
	  * @return OpStatus::OK on success, otherwise OpStatus::ERR_NO_MEMORY on insufficient memory available
	  */
	OP_STATUS  					OnWebHandlerAdded(OpStringC url, OpStringC description, BOOL set_default);

	/** Called when a web handler for the mailto protocol is removed
	 * @param url Web address to the handler (page)
	 */
	void						OnWebHandlerRemoved(OpStringC url);

	/** Check if a given web handler has been added
	 * @param url Web address to the handler (page)
	 * @return TRUE if the handler represented by the url has been added by OnWebHandlerAdded, otherwise FALSE
	 */
	BOOL						IsWebHandlerRegistered(OpStringC url);

	/** Retrieve the source of the web handler
	  * @param id ID of provider to get source for
	  * @param source How the handler was added to list, HANDLER_PREINSTALLED or HANDLER_CUSTOM
	  * @return OpStatus::OK on success, otyherwise OpStatus::ERR on invalid ID
	  */
	OP_STATUS  					GetSource(unsigned int id, HandlerSource& source);

protected:
	// Constructor and destructor protected to allow instantiation for testing

	OP_STATUS					Init(PrefsFile* prefsfile);

	/** Replace %character with a string in a string and put it in a string
	  * @param to_replace - character that is fto replace
	  * @param string - the string that contains the %character and that will be changed
	  * @param string_to_insert - string to insert instead of the %character
	  * @return TRUE if it did replace something
	  */
	BOOL						ReplaceFormat(const char to_be_replaced, OpString8& string, const OpStringC8 string_to_insert) const;

private:
	/** Return an identifier that has is not being used or 0 if none is available
	  * @return The identifer
	  */
	unsigned int				GetFreeId() const;

private:
	struct WebmailProvider
	{
		unsigned int	id;
		OpString		name;
		OpString8		url;
		OpString8		favicon_url;
		BOOL			is_webhandler; // Only to be set for custom web handlers
	};

	WebmailManager::WebmailProvider*	GetProviderById(unsigned int id) const;

	OpAutoVector<WebmailProvider> m_providers;

	enum Attribute
	{
		MAILTO_TO,
		MAILTO_CC,
		MAILTO_BCC,
		MAILTO_SUBJECT,
		MAILTO_BODY,
		MAILTO_URL,
		MAILTO_ATTR_COUNT // don't remove, should be last
	};
};

#endif //WEBMAILMANAGER_H
