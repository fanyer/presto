/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/widgets/DocumentView.h"

/* static */
DocumentView::DocumentViewTypes DocumentView::GetType(const uni_char* str)
{
	// str might be NULL if it's a search, but that means it's not any
	// collection view either
	if (str)
	{
		if (uni_stricmp(str, "opera:speeddial") == 0)
			return DOCUMENT_TYPE_SPEED_DIAL;
#ifdef NEW_BOOKMARK_MANAGER
		else if (uni_stricmp(str, "opera:collectionview") == 0)
			return DOCUMENT_TYPE_COLLECTION;
#endif
	}
	// anything else and we're a browser view
	return DOCUMENT_TYPE_BROWSER;
}

/* static */
bool DocumentView::IsUrlRestrictedForViewFlag(const uni_char* str, DocumentViewFlags flag)
{
	return !(GetType(str) == DocumentView::DOCUMENT_TYPE_SPEED_DIAL);
}

/* static */
void DocumentView::GetThumbnailImage(const uni_char* str, Image &img)
{
	if (GetType(str) == DocumentView::DOCUMENT_TYPE_COLLECTION)
	{
		OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Bookmarks Panel Window Thumbnail Icon");
		if (skin_elm)
			img = skin_elm->GetImage(0);
	}
}
