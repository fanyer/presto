/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author: Blazej Kazmierczak (bkazmierczak)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/controller/ExtensionUninstallController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/widgetruntime/GadgetUtils.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/gadgets/OpGadgetClass.h"


ExtensionUninstallController::ExtensionUninstallController(OpGadgetClass& gclass, const uni_char* extension_id):					
					m_gclass(&gclass),
                    m_extension_id(extension_id)
{
}

void ExtensionUninstallController::InitL()
{
	LEAVE_IF_NULL(m_extension_id);

	LEAVE_IF_ERROR(SetDialog("Extension Uninstall Dialog"));
	const TypedObjectCollection* widgets = m_dialog->GetWidgetCollection();

	ANCHORD(OpString, question);
	ANCHORD(OpString, name);
	const_cast<OpGadgetClass&>(*m_gclass).GetGadgetName(name);
	
	LEAVE_IF_ERROR(StringUtils::GetFormattedLanguageString(question, Str::D_EXTENSION_UNINSTALL_QUESTION,name.CStr()));
	LEAVE_IF_ERROR(SetWidgetText<QuickMultilineLabel>("Main_question",question));

	if (OpStatus::IsSuccess(ExtensionUtils::GetExtensionIcon(m_icon_img, m_gclass)))
	{
		OP_ASSERT(!m_icon_img.IsEmpty());
		QuickIcon* icon_widget = widgets->GetL<QuickIcon>("Extension_icon");
		icon_widget->SetBitmapImage(m_icon_img);
	}
}

void ExtensionUninstallController::OnOk()
{	
	g_desktop_extensions_manager->UninstallExtension(m_extension_id);
}

