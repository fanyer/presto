/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef LAYOUT_SELFTEST_UTILS_H
#define LAYOUT_SELFTEST_UTILS_H

#include "modules/doc/frm_doc.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/box/box.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/content/content.h"
#include "modules/layout/content/multicol.h"

static inline Box* GetBoxById(FramesDocument* doc, const uni_char* id)
{
	HTML_Element* elm = doc->GetDocRoot()->GetElmById(id);

	if (!elm)
		return NULL;

	return elm->GetLayoutBox();
}

static inline MultiColumnContainer* GetMulticolById(FramesDocument* doc, const uni_char* id)
{
	HTML_Element* elm = doc->GetDocRoot()->GetElmById(id);

	if (!elm || !elm->GetLayoutBox() || !elm->GetLayoutBox()->GetContainer())
		return NULL;

	return elm->GetLayoutBox()->GetContainer()->GetMultiColumnContainer();
}

static inline int GetActualColumnCount(MultiColumnContainer* mc)
{
	ColumnRow* row = mc->GetFirstRow();

	if (!row || row->Suc())
		// This function requires 1 and only 1 row.

		return -1;

	return row->GetColumnCount();
}

static inline FloatedPaneBox* GetFloatedPaneBoxById(FramesDocument* doc, const uni_char* id)
{
	HTML_Element* elm = doc->GetDocRoot()->GetElmById(id);

	if (!elm || !elm->GetLayoutBox() || !elm->GetLayoutBox()->IsFloatedPaneBox())
		return NULL;

	return (FloatedPaneBox*) elm->GetLayoutBox();
}

#endif // LAYOUT_SELFTEST_UTILS_H
