/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETHELPLOADER_H
#define	GADGETHELPLOADER_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/hardcore/mh/messobj.h"
#include "modules/url/url2.h"
#include "modules/util/opstring.h"

/**
 * Handles the loading of help URLs as requested by clicking a "Help" button in
 * a Quick dialog.
 *
 * The standard "Opera way" of handling OpInputAction::ACTION_SHOW_HELP (see
 * Application::OnInputAction()) is opening a URL like
 * "http://help.opera.com/help/unite.html" in a browser window.  This doesn't
 * fit in with standalone widgets, because they do not -- and should not -- have
 * a browser window.
 *
 * So the solution is to make standalone widgets open help URLs in a browser
 * window in a separate process.  But (!) this would work OK as long as the
 * browser window is an Opera browser.  When help.opera.com receives that
 * request, it first parses the UA string and replies with a "302 Moved"
 * redirecting to the final URL matching the user's platform, version and
 * language, along the lines of
 * "http://help.opera.com/Windows/10.00/en/unite.html".
 *
 * However, we don't want to force the user of a standalone widget to use Opera
 * for browsing.  That's the general strategy: we also handle clicking on a link
 * in a widget with the user's default browser.  In this case, if that browser
 * is not Opera, obviously the help system will not work.
 *
 * So the final solution is: within the widget process, request
 * "http://help.opera.com/help/unite.html" (actually, we request
 * "opera:/help/unite.html"), and keep loading it (without displaying it
 * anywhere, of course) until the headers are loaded.  At this point, all
 * redirections have been resolved and we can obtain the final help URL.  We
 * then stop loading the document in the widget process and open it with the
 * user's default browser.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetHelpLoader : public MessageObject
{
public:
	virtual ~GadgetHelpLoader();

	/**
	 * (Re)initializes the loader with a help topic and starts the loading.
	 *
	 * @param topic the help topic; this comes from the "Help" button's input
	 *		action
	 * @return status
	 */
	OP_STATUS Init(const OpStringC& topic);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1,
			MH_PARAM_2 par2);

private:
	OP_STATUS StartLoading(const OpStringC& topic);
	void StopLoading();
	BOOL IsLoading();

	URL m_url;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGETHELPLOADER_H
