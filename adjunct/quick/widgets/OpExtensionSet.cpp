/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpExtensionButton.h"
#include "adjunct/quick/widgets/OpExtensionSet.h"

#include "modules/util/OpLineParser.h"


DEFINE_CONSTRUCT(OpExtensionSet);

OpExtensionSet::OpExtensionSet()
{
	SetName("Extensions Toolbar");
	GetBorderSkin()->SetImage("Extensions Toolbar Skin", "Toolbar Skin");
	SetStandardToolbar(FALSE); // This blocks 'reset to default' in UI
}

void OpExtensionSet::OnReadContent(PrefsSection *section)
{
	m_widgets.DeleteAll();

	if (section)
	{
		const PrefsEntry *entry = section->Entries();
		for(; entry != NULL; entry = entry->Suc())
		{
			OpLineParser line(entry->Key());

			OpString8 item_type;
			RETURN_VOID_IF_ERROR(
					line.GetNextTokenWithoutTrailingDigits(item_type));

			if(item_type.CompareI("ExtensionButton") == 0)
			{
				OpExtensionButton* button;

				INT32 extension_id;
				if (OpStatus::IsSuccess(line.GetNextValue(extension_id)) &&
					OpStatus::IsSuccess(
						OpExtensionButton::Construct(&button, extension_id)))
				{
					AddWidget(button);
				}
			}
		}
	}
}

void OpExtensionSet::OnWriteContent(PrefsFile* prefs_file, const char* name)
{
	INT32 count = GetWidgetCount();
	OP_STATUS err;

	if (count == 0)
	{
		// force an empty section to be created
		TRAP(err,
			prefs_file->WriteIntL(name, "Dummy", 0);
			prefs_file->DeleteKeyL(name, "Dummy");
		);
	}

	for (INT32 i = 0; i < count; i++)
	{
		OpWidget* widget = GetWidget(i);
		if (widget->GetType() == WIDGET_TYPE_EXTENSION_BUTTON)
		{
			OpLineString key;
			OpString value;
			key.WriteTokenWithTrailingDigits("ExtensionButton", i);
			key.WriteValue(static_cast<OpExtensionButton*>(widget)->GetId());

			OpString toolbar_name;
			if (OpStatus::IsSuccess(toolbar_name.Set(name)))
				TRAP(err, prefs_file->WriteStringL(toolbar_name, key.GetString(), value));
		}
	}
	g_application->SettingsChanged(SETTINGS_EXTENSION_TOOLBAR_CONTENTS);
}

BOOL OpExtensionSet::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			OP_ASSERT(child_action);

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_REMOVE:
				{
					// allow remove only if not inside
					// toolbar named in prefs
					OpString8 extensionset_parent;
					extensionset_parent.Set(g_pcui->GetStringPref(
						PrefsCollectionUI::ExtensionSetParent));
					child_action->SetEnabled( !GetParent() ||
						!GetParent()->IsNamed(extensionset_parent) );
					return TRUE;
				}
			}
			break;
		}
	}
	return OpToolbar::OnInputAction(action);
}

