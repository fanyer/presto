/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/doc/documentmenumanager.h"
#include "modules/doc/documentinteractioncontext.h"

DocumentMenuItem::DocumentMenuItem()
		: ObjectManager<DocumentMenuItem>::Object(g_menu_manager)
{
}

void DocumentMenuManager::OnDocumentMenuItemClicked(UINT32 id, OpDocumentContext* doc_context)
{
	// @note As menu items are generally defined in different components than menu manager
	// in multi-process phase 2 the implementation of this function will have to find channel
	// to the component in which the menu item was created and send a message to it.
	// So it will look something like that
	// if (OpChannel* channel = FindChannelForItem(id))
	//     channel->PostMessage(OpDocumentMenuItmClickedMessage::Make(context));
	DocumentInteractionContext* context = static_cast<DocumentInteractionContext*>(doc_context);
	if (DocumentMenuItem* item = FindMenuItemById(id))
		item->OnClick(context, context->GetDocument(), context->GetHTMLElement());
}
