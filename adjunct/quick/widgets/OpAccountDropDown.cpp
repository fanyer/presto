/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT


#include "OpAccountDropdown.h"
#include "adjunct/quick/Application.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/dragdrop/dragdrop_manager.h"

/***********************************************************************************
**
**	OpAccountDropDown
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpAccountDropDown)

OpAccountDropDown::OpAccountDropDown()
{
	RETURN_VOID_IF_ERROR(init_status);

	SetListener(this);

	if (g_application->ShowM2())
	{
		g_m2_engine->AddEngineListener(this);
	}

	PopulateAccountDropDown();
}

void OpAccountDropDown::OnDeleted()
{
	if (g_application->ShowM2())
	{
		g_m2_engine->RemoveEngineListener(this);
	}
	OpDropDown::OnDeleted();
}

/***********************************************************************************
**
**	PopulateAccountDropDown
**
***********************************************************************************/

void OpAccountDropDown::PopulateAccountDropDown()
{
	if (!g_application->ShowM2())
		return;

	Clear();

	AccountManager* account_manager = g_m2_engine->GetAccountManager();
	Account* account = NULL;

	int i, got_index;

	int active_account;
	OpString active_account_category;
	OpStatus::Ignore(account_manager->GetActiveAccount(active_account, active_account_category));


	OpString entry_string;

	g_languageManager->GetString(Str::S_ACCOUNT_SELECTOR_ALL, entry_string);
	if (OpStatus::IsSuccess(AddItem(entry_string.CStr(), -1, &got_index, 0)))
	{
		if (active_account == 0)
		{
			SelectItem(got_index, TRUE);
		}
	}

	g_languageManager->GetString(Str::S_ACCOUNT_SELECTOR_MAIL, entry_string);
	if (OpStatus::IsSuccess(AddItem(entry_string.CStr(), -1, &got_index, -3)))
	{
		if (active_account == -3)
		{
			SelectItem(got_index, TRUE);
		}
	}

	g_languageManager->GetString(Str::S_ACCOUNT_SELECTOR_NEWS, entry_string);
	if (OpStatus::IsSuccess(AddItem(entry_string.CStr(), -1, &got_index, -4)))
	{
		if (active_account == -4)
		{
			SelectItem(got_index, TRUE);
		}
	}

	BOOL seperator = FALSE;

	OpAutoVector<OpString> categories;

	for (i = 0; i < account_manager->GetAccountCount(); i++)
	{
		account = account_manager->GetAccountByIndex(i);

		if (account)
		{
			OpString category;
			account->GetAccountCategory(category);

			if (category.HasContent())
			{
				BOOL used_already = FALSE;

				for (UINT32 j = 0; j < categories.GetCount(); j++)
				{
					if (categories.Get(j)->CompareI(category.CStr()) == 0)
					{
						used_already = TRUE;
						break;
					}
				}

				if (!used_already)
				{
					if (!seperator)
					{
						if (OpStatus::IsSuccess(AddItem(NULL, -1, &got_index)))
							ih.GetItemAtNr(got_index)->SetSeperator(TRUE);
						seperator = TRUE;
					}

					OpString* string = OP_NEW(OpString, ());
					if (string)
					{
						string->Set(category.CStr());

						categories.Add(string);
					}

					if (OpStatus::IsSuccess(AddItem(category.CStr(), -1, &got_index, -1)))
					{
						if (active_account == -1 && category.CompareI(active_account_category) == 0)
						{
							SelectItem(got_index, TRUE);
						}
					}
				}
			}
		}
	}

	seperator = FALSE;

	for (i = 0; i < account_manager->GetAccountCount(); i++)
	{
		account = account_manager->GetAccountByIndex(i);

		if (account)
		{
			switch (account->GetIncomingProtocol())
			{
				case AccountTypes::NEWS:
				case AccountTypes::IMAP:
				case AccountTypes::POP:
				{
					if (!seperator)
					{
						if (OpStatus::IsSuccess(AddItem(NULL, -1, &got_index)))
							ih.GetItemAtNr(got_index)->SetSeperator(TRUE);
						seperator = TRUE;
					}

					int id = account->GetAccountId();

					if (OpStatus::IsSuccess(AddItem(account->GetAccountName().CStr(), -1, &got_index, id)))
					{
						if (id == active_account)
						{
							SelectItem(got_index, TRUE);
						}
					}
					break;
				}
			}
		}
	}
}

/***********************************************************************************
**
**	OnActiveAccountChanged
**
***********************************************************************************/

void OpAccountDropDown::OnActiveAccountChanged()
{
	PopulateAccountDropDown();
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void OpAccountDropDown::OnSettingsChanged(DesktopSettings* settings)
{
	OpDropDown::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_ACCOUNT_SELECTOR))
	{
		PopulateAccountDropDown();
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpAccountDropDown::OnInputAction(OpInputAction* action)
{
	return OpDropDown::OnInputAction(action);
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void OpAccountDropDown::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == this && g_application->ShowM2())
	{
		g_m2_engine->GetAccountManager()->SetActiveAccount((INTPTR) GetItemUserData(GetSelectedItem()), GetItemText(GetSelectedItem()));
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpAccountDropDown::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == this)
	{
		((OpWidgetListener*)GetParent())->OnMouseEvent(this, pos, x, y, button, down, nclicks);
	}
	else
	{
		OpDropDown::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpAccountDropDown::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(OpTypedObject::DRAG_TYPE_ACCOUNT_DROPDOWN, x, y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

#endif
