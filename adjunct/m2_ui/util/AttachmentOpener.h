/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 *
 */

#ifndef ATTACHMENT_OPENER_H
#define ATTACHMENT_OPENER_H

#ifdef M2_SUPPORT

class URL;
class DesktopFileChooser;
class DesktopWindow;

namespace AttachmentOpener
{
			/** Open a given attachment with the default application
			  * @param url - pointing to the attachment you want to open
			  * @param chooser - in case we need to open a file chooser
			  * @ parent_window - parent window for the file chooser dialog
			  */
	BOOL	OpenAttachment(URL* url, DesktopFileChooser* chooser, DesktopWindow* parent_window);
};

#endif // M2_SUPPORT
#endif // ATTACHMENT_OPENER_H