/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef X11_OPFILECHOOSER_H
#define X11_OPFILECHOOSER_H

#if !defined WIDGETS_CAP_WIC_FILESELECTION || !defined WIC_CAP_FILESELECTION

typedef const class OpWindow SELECTOR_PARENT;

class X11OpFileChooser : public OpFileChooser
{
public:
	OP_STATUS ShowOpenSelector(SELECTOR_PARENT *parent, const OpString &caption,
							   const OpFileChooserSettings &settings);
	OP_STATUS ShowSaveSelector(SELECTOR_PARENT* parent, const OpString& caption,
							   const OpFileChooserSettings &settings);
	OP_STATUS ShowFolderSelector(SELECTOR_PARENT *parent, const OpString &caption,
								 const OpFileChooserSettings &settings);
};

#endif // !WIDGETS_CAP_WIC_FILESELECTION || !WIC_CAP_FILESELECTION

#endif // X11_OPFILECHOOSER_H
