/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"
#include "ContactPropertiesDialog.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/panels/ContactsPanel.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/locale/oplanguagemanager.h"

#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/engine.h"


/***********************************************************************************
**
**	ContactPropertiesDialog
**
***********************************************************************************/

OP_STATUS ContactPropertiesDialog::Init(DesktopWindow* parent_window, HotlistModel* model, HotlistModelItem* item, HotlistModelItem* parent)
{
	m_model          = model;
	m_contact_id     = item->GetID();
	m_parent_id      = parent ? parent->GetID() : 0;
	m_temporary_item = model && (item->GetTemporaryModel() == model) ? item : 0;

	if (m_model)
	{
		OP_STATUS err = m_model->AddListener(this);
		if (OpStatus::IsError(err))
		{
			Close(TRUE);
			return err;
		}
	}

	return Dialog::Init(parent_window);
}

void ContactPropertiesDialog::OnInit()
{
	HotlistModelItem* hmi = GetContact();
	if(hmi)
	{
		const char* ltr_text_widgets[] = {
			"Email_edit",
			"Homepage_edit",
			"Phone_edit",
			"Fax_edit",
			"Otheremail_edit",
			"PictureURL_edit",
		};
		for (unsigned i = 0; i < ARRAY_SIZE(ltr_text_widgets); ++i)
		{
			OpEdit* edit = GetWidgetByName<OpEdit>(ltr_text_widgets[i], WIDGET_TYPE_EDIT);
			if (edit)
				edit->SetForceTextLTR(TRUE);
		}

		HotlistManager::ItemData item_data;
		g_hotlist_manager->GetItemValue( hmi, item_data );

		OpString email1, email2;
		email1.Set(item_data.mail);

		int separator_pos;
		if ((separator_pos = email1.FindFirstOf(UNI_L(","))) != KNotFound)
		{
			uni_char* buf = email1.CStr();
			uni_char* b = &buf[separator_pos];
			for( ; *b == ','; b++ ) {};
			if( *b )
			{
				email2.Set(b);
			}
			email1[separator_pos] = 0;
		}

		SetWidgetText("Name_edit", item_data.name.CStr());
		SetWidgetText("Email_edit",	email1.CStr());
		SetWidgetText("Homepage_edit", item_data.url.CStr());
		SetWidgetText("Notes_edit",	item_data.description.CStr());
		SetWidgetText("PictureURL_edit", item_data.pictureurl.CStr());
		SetWidgetText("Address_edit", item_data.postaladdress.CStr());
		SetWidgetText("Phone_edit",	item_data.phone.CStr());
		SetWidgetText("Fax_edit", item_data.fax.CStr());
		SetWidgetText("Otheremail_edit", email2.CStr());
		SetWidgetText("Nicknames_edit", item_data.shortname.CStr());

		m_personalbar_position = item_data.personalbar_position;
		   
#ifdef JABBER_SUPPORT
		SetWidgetText("label_for_Nicknames_edit", UNI_L("Jabber ID")); // Gave this label the name label_for_Nicknames_edit in dialog.ini. It didn't have one earlier.
#endif // JABBER_SUPPORT

		OpListBox* listbox = (OpListBox*) GetWidgetByName("Icon_listbox");
		if( listbox )
		{
			for(INT32 i = 0; i < 40; i++) // FIXME: BAD hardcoding
			{
				uni_char buffer[32];
				uni_snprintf(buffer, 31, UNI_L("Contact%i"), i);
				
				INT32 got_index;
				listbox->AddItem(buffer, -1, &got_index);

				OpString8 image_name;
				image_name.Set(listbox->ih.GetItemAtNr(got_index)->string.Get());
				
				listbox->ih.SetImage(got_index, image_name.CStr(), listbox);
				if( item_data.image.CStr() && uni_stricmp(buffer,item_data.image.CStr()) == 0)
				{
					listbox->SelectItem(got_index, TRUE);
				}
			}
			OpEdit* edit = (OpEdit*) GetWidgetByName("Name_edit");
			if( edit )
			{
				edit->GetForegroundSkin()->SetImage(item_data.direct_image_pointer);
				edit->GetForegroundSkin()->SetRestrictImageSize(TRUE);
			}
		}

		OpWebImageWidget* image_widget = (OpWebImageWidget*) GetWidgetByName("Picture_image");
		if( image_widget )
		{
			if (!item_data.pictureurl.IsEmpty())
			{
				image_widget->SetImage(item_data.pictureurl.CStr());
			}
		}

		// Dialog caption
		OpString title;
		g_languageManager->GetString(Str::D_PROPERTIES_DIALOG_CAPTION, title);
		SetTitle(title.CStr());
	}
}

UINT32 ContactPropertiesDialog::OnOk()
{
	HotlistModelItem* hmi = GetContact();
	if(hmi)
	{
		HotlistManager::ItemData item_data;
		OpString email1, email2;

		GetWidgetText("Name_edit", item_data.name);
		GetWidgetText("Email_edit",	email1);
		GetWidgetText("Otheremail_edit", email2);
		GetWidgetText("Homepage_edit", item_data.url);
		GetWidgetText("Notes_edit",	item_data.description);
		GetWidgetText("Fax_edit", item_data.fax);
		GetWidgetText("Phone_edit",	item_data.phone);
		GetWidgetText("Address_edit", item_data.postaladdress);
		GetWidgetText("PictureURL_edit", item_data.pictureurl);
		GetWidgetText("Nicknames_edit", item_data.shortname);

		item_data.personalbar_position = m_personalbar_position;

		OpListBox* listbox = (OpListBox*) GetWidgetByName("Icon_listbox");
		if( listbox )
		{
			item_data.image.Set(listbox->ih.GetItemAtNr(listbox->ih.focused_item)->string.Get());
		}

		if( email1.IsEmpty())
		{
			item_data.mail.Set(email2);
		}
		else
		{
			item_data.mail.Set(email1);
			if (!email2.IsEmpty())
			{
				// both contain something..
				item_data.mail.Append(UNI_L(","));
				item_data.mail.Append(email2);
			}
		}
		if(g_hotlist_manager->RegisterTemporaryItem( hmi, m_parent_id))
		{
			// New item
			g_hotlist_manager->SetItemValue( hmi, item_data, TRUE );
		}
		else
		{
			// The item was only modified
			g_hotlist_manager->SetItemValue( hmi, item_data, TRUE );
			
			// Create a new M2 index each time we add some emails (to start the search again)
			if (hmi->GetM2IndexId() != 0)
			{
				Index * old_index = g_m2_engine->GetIndexById(hmi->GetM2IndexId());
				if (old_index)
				{
					hmi->SetM2IndexId(0);
					Index * new_index = g_m2_engine->GetIndexById(hmi->GetM2IndexId(TRUE));
					if (new_index)
					{
						new_index->ToggleIgnored(old_index->IsIgnored());
						new_index->ToggleWatched(old_index->IsWatched());
					}
				}
				else
				{
                    hmi->SetM2IndexId(0);
                }
				
			}
		}
	}

	return 0;
}


void ContactPropertiesDialog::OnCancel()
{
	HotlistModelItem* hmi = GetContact();
	if(hmi)
	{
		g_hotlist_manager->RemoveTemporaryItem(hmi);
	}
}


void ContactPropertiesDialog::OnClose(BOOL user_initiated)
{
	if( m_model )
	{
		m_model->RemoveListener(this);
	}
	Dialog::OnClose(user_initiated);
}


HotlistModelItem* ContactPropertiesDialog::GetContact()
{
	if( m_model )
	{
		if( m_temporary_item )
		{
			return m_temporary_item;
		}
		else
		{
			return m_model->GetItemByID( m_contact_id );
		}
	}
	else
	{
		return 0;
	}
}


void ContactPropertiesDialog::OnTreeChanged(OpTreeModel* tree_model)
{
	DesktopWindow::Close(FALSE,TRUE);
}


void ContactPropertiesDialog::OnTreeDeleted(OpTreeModel* tree_model)
{
	m_model = 0;
}
