/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/controller/ExtensionPrefsController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"

ExtensionPrefsController::ExtensionPrefsController(SpeedDialThumbnail& thumbnail)
	: m_thumbnail(&thumbnail)
{
}

void ExtensionPrefsController::InitL()
{
	OP_ASSERT(m_thumbnail->GetParentOpSpeedDial());
	QuickOverlayDialog* dialog = OP_NEW_L(QuickOverlayDialog, ());
	dialog->SetBoundingWidget(*m_thumbnail->GetParentOpSpeedDial());

	dialog->SetResizeOnContentChange(FALSE);
	LEAVE_IF_ERROR(SetDialog("Extension Preferences Dialog", dialog));

	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*m_thumbnail));
	dialog->SetDialogPlacer(placer);
	// Turn show/hide animations off until we can make them work for this
	// dialog smoothly.
	dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_NONE);
}

OP_STATUS ExtensionPrefsController::LoadPrefs(OpWindowCommander* commander, const OpStringC& url)
{
	OP_ASSERT(commander != NULL);
	if (commander == NULL)
		return OpStatus::ERR_NULL_POINTER;

	QuickBrowserView* browser_view = m_dialog->GetWidgetCollection()->Get<QuickBrowserView>("preferences_view");
	if (!browser_view)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpPage> page(OP_NEW(OpPage, (commander)));
	RETURN_OOM_IF_NULL(page.get());
	RETURN_IF_ERROR(page->Init());
	RETURN_IF_ERROR(browser_view->GetOpBrowserView()->SetPage(page.release()));

	commander->OpenURL(url.CStr(), FALSE);

	return OpStatus::OK;
}
