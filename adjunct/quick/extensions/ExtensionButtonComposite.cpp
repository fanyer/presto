/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef EXTENSION_SUPPORT

#include "adjunct/quick/extensions/ExtensionButtonComposite.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/widgets/OpExtensionSet.h"
#include "adjunct/quick/widgets/OpExtensionButton.h"

#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"

ExtensionButtonComposite::ExtensionButtonComposite()
{
	m_item.id = -1;

	// default icon is icon associated
	// with ACTION_EXTENSION_ACTION ("Extension action")
	// it is triggered, when m_icon is empty
	m_icon.Empty();
}

ExtensionButtonComposite::~ExtensionButtonComposite()
{
	OP_ASSERT(0 == m_components.GetCount());
	// nothing wrong happens if it's not empty, but it can happen
	// only when this class is deleted before UI (and we don't want this)
}

void ExtensionButtonComposite::Add(ExtensionButtonComponent *extension_button)
{
	// FIXME handle failed adding to vector
	if (OpStatus::IsSuccess(m_components.Add(extension_button)))
	{
		// update with correct values from m_item
		extension_button->UpdateIcon(m_icon);
		if (m_badge.displayed)
		{
			extension_button->SetBadgeTextColor(
					OpColorToColorref(m_badge.color));
			extension_button->SetBadgeBackgroundColor(
					OpColorToColorref(m_badge.background_color));
			extension_button->SetBadgeText(m_badge.text_content);
		}
		else
			extension_button->SetBadgeText(NULL);
		extension_button->SetDisabled(m_item.disabled);
		extension_button->SetTitle(m_item.title); // FIXME hmm is it ok? maybe we should copy it?
		extension_button->SetConnected(m_connected);
		g_desktop_extensions_manager->SetPopupSize(m_item.id,m_popup.width, m_popup.height);

		if (m_components.GetCount() == 1)
			m_icon.IncVisible(null_image_listener);
	}
}

void ExtensionButtonComposite::Remove(ExtensionButtonComponent *extension_button)
{
	m_components.RemoveByItem(extension_button);
	if (m_components.GetCount() == 0)
		m_icon.DecVisible(null_image_listener);
}

OP_STATUS ExtensionButtonComposite::CreateOpExtensionButton(ExtensionId id)
{
	OpExtensionSet* ext_set = NULL;
	OpExtensionButton* button = NULL;
	INT32 extension_id = INT32(id);
	RETURN_IF_ERROR(OpExtensionSet::Construct(&ext_set));
	OP_STATUS status = OpExtensionButton::Construct(&button, extension_id);
	if (OpStatus::IsSuccess(status))
	{
		ext_set->AddWidget(button);
		// WriteContent() is essential here, it will result in creation of new
		// button in all opera windows
		ext_set->WriteContent();
	}
	OP_DELETE(ext_set);
	return status;
}

OP_STATUS ExtensionButtonComposite::RemoveOpExtensionButton(ExtensionId id)
{
	OpExtensionSet* ext_set = NULL;
	OpExtensionButton* button = NULL;
	OpWidget *wgt = NULL;
	
	INT32 extension_id = INT32(id);
	RETURN_IF_ERROR(OpExtensionSet::Construct(&ext_set));

	for (int i = 0; i < ext_set->GetWidgetCount(); i++)
	{
		wgt = ext_set->GetWidget(i);
		
		if (wgt->GetType() == OpTypedObject::WIDGET_TYPE_EXTENSION_BUTTON)
		{
			button = static_cast<OpExtensionButton*>(wgt);
			if (button->GetId() == extension_id)
			{
				ext_set->RemoveWidget(i);
			}
		}
	}
	ext_set->WriteContent();
	OP_DELETE(ext_set);
	return OpStatus::OK;
}


OP_STATUS ExtensionButtonComposite::UpdateUIItem(
		OpExtensionUIListener::ExtensionUIItem *item)
{
	OP_ASSERT(item->badge);
	OP_ASSERT(item->popup);

	OpBitmap* req_bitmap = item->favicon;
	OpBitmap* cmp_bitmap = m_item.favicon;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	INT32 scale = static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetMaximumHiresScaleFactor();
	if (scale > 100 && item->favicon_hires)
	{
		req_bitmap = item->favicon_hires;
		cmp_bitmap = m_item.favicon_hires;
	}
#endif
	// changing of icon goes like this:
	// - default icon stays as long as _only_ null bitmaps were received before
	// - every not null bitmap is new icon to use
	if (cmp_bitmap != req_bitmap && req_bitmap != NULL)
	{
		m_icon = imgManager->GetImage(req_bitmap);
		UpdateIcon(m_icon);
	}

	if (item->badge->displayed)
	{
		// changing badge nackground colour causes badge background bitmap copy
		// so we would like to avoid that if possible
		COLORREF new_color = OpColorToColorref(item->badge->background_color);

		SetBadgeBackgroundColor(new_color);
		SetBadgeTextColor(OpColorToColorref(item->badge->color));
		SetBadgeText(item->badge->text_content);
	}
	else
		SetBadgeText(NULL);

	// FIXME if enabled/disabled state changed
		SetDisabled(item->disabled);
	
	SetTitle(item->title);
	SetUIItem(*item);
	g_desktop_extensions_manager->SetPopupURL(m_item.id,m_popup.href);	
	g_desktop_extensions_manager->SetPopupSize(m_item.id,m_popup.width, m_popup.height);

	SetConnected(TRUE);
	return OpStatus::OK;
}

INT32 ExtensionButtonComposite::GetId()
{
	return m_item.id;
}

BOOL ExtensionButtonComposite::GetEnabled() const
{
	return !m_item.disabled;
}

OpExtensionUIListener::ExtensionUIItemNotifications* 
ExtensionButtonComposite::GetCallbacks()
{
	return m_item.callbacks;
}

OP_STATUS ExtensionButtonComposite::UpdateIcon(Image &image)
{
	// FIXME can it fail? :/ when? there can be OOM, but should we fail in such case?
	// maybe it should be void after all?
	for (UINT i = 0; i < m_components.GetCount(); i++)
	{
		m_components.Get(i)->UpdateIcon(image);
	}
	return OpStatus::OK;
}

void ExtensionButtonComposite::SetUIItem(OpExtensionUIListener::ExtensionUIItem item)
{
	OP_ASSERT(GetId() == -1 || GetId() == item.id);

	m_item = item;
	m_badge = *(item.badge);
	m_popup = *(item.popup);
}

void ExtensionButtonComposite::SetBadgeBackgroundColor(COLORREF color)
{
	for (UINT i = 0; i < m_components.GetCount(); i++)
	{
		m_components.Get(i)->SetBadgeBackgroundColor(color);
	}
}

void ExtensionButtonComposite::SetBadgeTextColor(COLORREF color)
{
	for (UINT i = 0; i < m_components.GetCount(); i++)
	{
		m_components.Get(i)->SetBadgeTextColor(color);
	}
}

void ExtensionButtonComposite::SetBadgeText(const OpStringC& text)
{
	for (UINT i = 0; i < m_components.GetCount(); i++)
	{
		m_components.Get(i)->SetBadgeText(text);
	}
}

void ExtensionButtonComposite::SetDisabled(BOOL disabled)
{
	for (UINT i = 0; i < m_components.GetCount(); i++)
	{
		m_components.Get(i)->SetDisabled(disabled);
	}
}

void ExtensionButtonComposite::SetTitle(const OpStringC& title)
{
	for (UINT i = 0; i < m_components.GetCount(); i++)
	{
		m_components.Get(i)->SetTitle(title);
	}
}

inline COLORREF ExtensionButtonComposite::OpColorToColorref(
		OpWindowCommander::OpColor color)
{
	return OP_RGBA(color.red, color.green, color.blue, color.alpha);
}

#endif // EXTENSION_SUPPORT

