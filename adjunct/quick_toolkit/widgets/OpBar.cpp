/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "OpBar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/managers/opsetupmanager.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefsfile/prefsfile.h"

#ifdef SEARCH_BAR_SUPPORT
#include "modules/softcore/registrationmanager.h"
#endif // SEARCH_BAR_SUPPORT

#if defined(_MACINTOSH_)
# include "modules/display/vis_dev.h"
# include "platforms/mac/pi/CocoaOpWindow.h"
#endif // _MACINTOSH_


/***********************************************************************************
**
**	OpBar
**
***********************************************************************************/

OpBar::OpBar(PrefsCollectionUI::integerpref alignment_prefs_setting, PrefsCollectionUI::integerpref autoalignment_prefs_setting) :
	m_used_width(0),
	m_used_height(0),
	m_used_alignment(ALIGNMENT_OFF),
	m_used_collapse(COLLAPSE_NORMAL),
	m_alignment(ALIGNMENT_OFF),
	m_auto_alignment(FALSE),
	m_non_fullscreen_alignment(ALIGNMENT_OFF),
	m_non_fullscreen_auto_alignment(FALSE),
	m_old_visible_alignment(ALIGNMENT_TOP),
	m_auto_visible(FALSE),
	m_alignment_prefs_setting(alignment_prefs_setting),
	m_auto_alignment_prefs_setting(autoalignment_prefs_setting),
	m_collapse(COLLAPSE_NORMAL),
	m_changed_alignment_while_customizing(FALSE),
	m_changed_style_while_customizing(FALSE),
	m_changed_content_while_customizing(FALSE),
	m_master(FALSE),
	m_supports_read_and_write(TRUE)
{
	SetSkinned(TRUE);
	SetAlignment(ALIGNMENT_TOP);
}

/***********************************************************************************
**
**	GetResultingAlignment
**
***********************************************************************************/

OpBar::Alignment OpBar::GetResultingAlignment()
{
	if (g_application->IsCustomizingHiddenToolbars())
	{
		return GetOldVisibleAlignment();
	}

	return m_auto_alignment && !m_auto_visible ? ALIGNMENT_OFF : m_alignment;
}

/***********************************************************************************
**
**	IsVertical
**
***********************************************************************************/

BOOL OpBar::IsVertical()
{
	Alignment alignment = GetResultingAlignment();

	return alignment == ALIGNMENT_LEFT || alignment == ALIGNMENT_RIGHT;
}

/***********************************************************************************
**
**	IsHorizontal
**
***********************************************************************************/

BOOL OpBar::IsHorizontal()
{
	Alignment alignment = GetResultingAlignment();

	return alignment == ALIGNMENT_TOP || alignment == ALIGNMENT_BOTTOM;
}

/***********************************************************************************
**
**	IsOff
**
***********************************************************************************/

BOOL OpBar::IsOff()
{
	Alignment alignment = GetResultingAlignment();

	return alignment == ALIGNMENT_OFF;
}

/***********************************************************************************
**
**	SetAlignment
**
***********************************************************************************/

BOOL OpBar::SetAlignment(Alignment alignment, BOOL write_to_prefs)
{
	if (alignment == ALIGNMENT_OFF)
	{
		SetAutoAlignment(FALSE);
	}

	if (alignment > ALIGNMENT_OLD_VISIBLE)
	{
		alignment = ALIGNMENT_TOP;
	}
	else if (alignment == ALIGNMENT_OLD_VISIBLE)
	{
		alignment = m_old_visible_alignment;
	}

	if (m_alignment == alignment)
	{
		return FALSE;
	}

	if (alignment != ALIGNMENT_OFF)
	{
		m_old_visible_alignment = alignment;
	}

	m_alignment = alignment;

	SkinType skin_type = SKINTYPE_DEFAULT;

	switch (m_alignment)
	{
		case ALIGNMENT_LEFT:	skin_type = SKINTYPE_LEFT; break;
		case ALIGNMENT_TOP:		skin_type = SKINTYPE_TOP; break;
		case ALIGNMENT_RIGHT:	skin_type = SKINTYPE_RIGHT; break;
		case ALIGNMENT_BOTTOM:	skin_type = SKINTYPE_BOTTOM; break;
	}

	GetBorderSkin()->SetType(skin_type);

	if (write_to_prefs && !IsFullscreen())
	{
		WriteAlignment();
	}

	Relayout();

	OnAlignmentChanged();

	return TRUE;
}


/***********************************************************************************
**
**	SetCollapse
**
***********************************************************************************/

BOOL OpBar::SetCollapse(Collapse collapse, BOOL write_to_prefs)
{
	collapse = TranslateCollapse(collapse);

	if (m_collapse == collapse)
		return FALSE;

	m_collapse = collapse;

	if (write_to_prefs && !IsFullscreen())
	{
		WriteAlignment();
	}

	Relayout();
	return TRUE;
}

/***********************************************************************************
**
**	SetAutoVisible
**
***********************************************************************************/

BOOL OpBar::SetAutoVisible(BOOL auto_visible)
{
	if (m_auto_visible == auto_visible)
		return FALSE;

	m_auto_visible = auto_visible;

	if (!m_auto_alignment)
		return TRUE;

	Relayout();
	return TRUE;
}

/***********************************************************************************
**
**	SetAutoAlignment
**
***********************************************************************************/

BOOL OpBar::SetAutoAlignment(BOOL auto_alignment, BOOL write_to_prefs)
{
	if (m_auto_alignment == auto_alignment)
		return FALSE;

	m_auto_alignment = auto_alignment;

	if (write_to_prefs && !IsFullscreen())
	{
		WriteAlignment();
	}

	Relayout();

	OnAutoAlignmentChanged();

	return TRUE;
}

/***********************************************************************************
**
**	GetRequiredSize
**
***********************************************************************************/

void OpBar::GetRequiredSize(INT32& width, INT32& height)
{
	OnLayout(TRUE, 0x40000000, 0x40000000, width, height);
}


void OpBar::GetRequiredSize(INT32 max_width, INT32 max_height, INT32& width, INT32& height)
{
	OnLayout(TRUE, max_width, max_height, width, height);
}

/***********************************************************************************
**
**	GetWidthFromHeight
**
***********************************************************************************/

INT32 OpBar::GetWidthFromHeight(INT32 height, INT32* used_height)
{
	INT32 dummy_used_height;
	INT32 used_width;

	OnLayout(TRUE, 0x40000000, height, used_width, used_height ? *used_height : dummy_used_height);

	return used_width;
}

/***********************************************************************************
**
**	GetHeightFromWidth
**
***********************************************************************************/

INT32 OpBar::GetHeightFromWidth(INT32 width, INT32* used_width)
{
	INT32 dummy_used_width;
	INT32 used_height;

	OnLayout(TRUE, width, 0x40000000, used_width ? *used_width : dummy_used_width, used_height);

	return used_height;
}

/***********************************************************************************
**
**	LayoutToAvailableRect
**
***********************************************************************************/

OpRect OpBar::LayoutToAvailableRect(const OpRect& rect, BOOL compute_rect_only, INT32 banner_x, INT32 banner_y)
{
	OpRect shrunken_rect = rect;
	OpRect bar_rect = rect;

	BOOL show = FALSE;

	Alignment alignment = GetResultingAlignment();

	if (alignment != ALIGNMENT_OFF && alignment != ALIGNMENT_FLOATING)
	{
		show = TRUE;

		INT32 used_width;
		INT32 used_height;

		if (banner_x && alignment == ALIGNMENT_TOP && bar_rect.x + bar_rect.width > banner_x && bar_rect.y < banner_y)
		{
			bar_rect.width = banner_x - bar_rect.x;
		}

		if (banner_y && alignment == ALIGNMENT_RIGHT && bar_rect.y < banner_y)
		{
			bar_rect.height = (bar_rect.y + bar_rect.height) - banner_y;
			bar_rect.y = banner_y;
		}

		OnLayout(TRUE, bar_rect.width, bar_rect.height, used_width, used_height);

		switch (alignment)
		{
			case ALIGNMENT_LEFT:
				bar_rect.width = used_width;
				shrunken_rect.x += used_width;
				shrunken_rect.width -= used_width;
				break;

			case ALIGNMENT_RIGHT:
				bar_rect.x += bar_rect.width - used_width;
				bar_rect.width = used_width;
				shrunken_rect.width -= used_width;
				break;

			case ALIGNMENT_TOP:
				bar_rect.height = used_height;
				shrunken_rect.y += used_height;
				shrunken_rect.height -= used_height;
				break;

			case ALIGNMENT_BOTTOM:
				bar_rect.y += bar_rect.height - used_height;
				bar_rect.height = used_height;
				shrunken_rect.height -= used_height;
				break;
		}

		if (!compute_rect_only)
		{
			if (IsFloating())
				SetOriginalRect(bar_rect);
			else
				SetRect(bar_rect);
		}
	}

	if (!compute_rect_only)
	{
		SetVisibility(show);
	}

	return shrunken_rect;
}

/***********************************************************************************
**
**	SetName
**
***********************************************************************************/

void OpBar::OnNameSet()
{
	// The name of the widget has been set
	// Read its configuration
	Read();
}

/***********************************************************************************
**
**	RestoreToDefaults
**
***********************************************************************************/

void OpBar::RestoreToDefaults()
{
	Read(OpBar::ALL, TRUE);

	if (!GetSupportsReadAndWrite() || m_master)
		return;

	if (g_application->IsCustomizingToolbars())
	{
		m_changed_alignment_while_customizing = TRUE;
		m_changed_content_while_customizing = TRUE;
		m_changed_style_while_customizing = TRUE;
		return;
	}

	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);

	// delete all the sections and commit it to the file
	OpString8 name;
	RETURN_VOID_IF_ERROR(GetSectionName(".alignment", name));
	TRAPD(err, prefs_file->DeleteSectionL(name.CStr()));
	RETURN_VOID_IF_ERROR(GetSectionName(".style", name));
	TRAP(err, prefs_file->DeleteSectionL(name.CStr()));
	RETURN_VOID_IF_ERROR(GetSectionName(".content", name));
	TRAP(err, prefs_file->DeleteSectionL(name.CStr()));

	//notify other toolbars or interested components that the setup has changed
	g_application->SettingsChanged(SETTINGS_TOOLBAR_CONTENTS);
}

/***********************************************************************************
**
**	Read
**
***********************************************************************************/

void OpBar::Read(SettingsType settings , BOOL master)
{
	if (!GetSupportsReadAndWrite())
		return;

	if (settings & OpBar::ALIGNMENT)
		ReadAlignment(master);

	if (settings & OpBar::STYLE)
		ReadStyle(master);

	if (settings & OpBar::CONTENTS)
		ReadContents(master);

	g_input_manager->UpdateAllInputStates(TRUE);
}

/***********************************************************************************
**
**	ReadAlignment
**
***********************************************************************************/

void OpBar::ReadAlignment(BOOL master)
{
	PrefsSection *section = NULL;

	OpString8 alignment_name;
	RETURN_VOID_IF_ERROR(GetSectionName(".alignment", alignment_name));

	TRAPD(err, section = g_setup_manager->GetSectionL(alignment_name.CStr(), OPTOOLBAR_SETUP, NULL, master || m_master));
	if(section)
	{
		OnReadAlignment(section);
		OP_DELETE(section);
	}
}

/***********************************************************************************
**
**	ReadStyle
**
***********************************************************************************/

void OpBar::ReadStyle(BOOL master)
{
	PrefsSection *section = NULL;

	OpString8 style_name;
	RETURN_VOID_IF_ERROR(GetSectionName(".style", style_name));

	TRAPD(err, section = g_setup_manager->GetSectionL(style_name.CStr(), OPTOOLBAR_SETUP, NULL, master || m_master));

	if(section)
	{
		OnReadStyle(section);
		OP_DELETE(section);
	}

}

/***********************************************************************************
**
**	ReadContents
**
***********************************************************************************/

void OpBar::ReadContents(BOOL master)
{
	PrefsSection *section = NULL;

	OpString8 content_name;
	RETURN_VOID_IF_ERROR(GetSectionName(".content", content_name));

	TRAPD(err, section = g_setup_manager->GetSectionL(content_name.CStr(), OPTOOLBAR_SETUP, NULL, master || m_master));

	if(section)
	{
		OnReadContent(section);
		OP_DELETE(section);
	}
}

/***********************************************************************************
**
**	WriteAlignment
**
***********************************************************************************/

void OpBar::WriteAlignment()
{
	if (!GetSupportsReadAndWrite() || m_master)
		return;

	if (g_application->IsCustomizingToolbars())
	{
		m_changed_alignment_while_customizing = TRUE;
		return;
	}

	OpString8 name;
	RETURN_VOID_IF_ERROR(GetSectionName(".alignment", name));

	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);

	TRAPD(err, prefs_file->ClearSectionL(name.CStr()));

	OnWriteAlignment(prefs_file, name.CStr());
}

/***********************************************************************************
**
**	WriteStyle
**
***********************************************************************************/

void OpBar::WriteStyle()
{
	if (!GetSupportsReadAndWrite() || m_master)
		return;

	if (g_application->IsCustomizingToolbars())
	{
		m_changed_style_while_customizing = TRUE;
		return;
	}

	OpString8 name;
	RETURN_VOID_IF_ERROR(GetSectionName(".style", name));

	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);

	TRAPD(err, prefs_file->ClearSectionL(name.CStr()));

	OnWriteStyle(prefs_file, name.CStr());
}


/***********************************************************************************
**
**	WriteContent
**
***********************************************************************************/

void OpBar::WriteContent()
{
	if (!GetSupportsReadAndWrite() || m_master)
		return;

	if (g_application->IsCustomizingToolbars())
	{
		m_changed_content_while_customizing = TRUE;
		return;
	}

	OpString8 name;
	RETURN_VOID_IF_ERROR(GetSectionName(".content", name));

	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);

	TRAPD(err, prefs_file->ClearSectionL(name.CStr()));

	OnWriteContent(prefs_file, name.CStr());
}

/***********************************************************************************
**
**	OnReadAlignment
**
***********************************************************************************/

void OpBar::OnReadAlignment(PrefsSection *section)
{
	OP_MEMORY_VAR Alignment alignment = ALIGNMENT_TOP;
	OP_MEMORY_VAR BOOL auto_alignment = FALSE;
	Collapse collapse = COLLAPSE_NORMAL;

	if(m_alignment_prefs_setting != PrefsCollectionUI::DummyLastIntegerPref)
	{
		alignment = (Alignment) g_pcui->GetIntegerPref(m_alignment_prefs_setting);
	}

	if(m_auto_alignment_prefs_setting != PrefsCollectionUI::DummyLastIntegerPref)
	{
		auto_alignment = g_pcui->GetIntegerPref(m_auto_alignment_prefs_setting);
	}

	TRAPD(err, alignment = (Alignment) g_setup_manager->GetIntL(section, "Alignment", alignment));
	TRAP(err, auto_alignment = g_setup_manager->GetIntL(section, "Auto alignment", auto_alignment));
	TRAP(err, m_old_visible_alignment = (Alignment) g_setup_manager->GetIntL(section, "Old visible alignment", m_old_visible_alignment));
	TRAP(err, collapse = (Collapse) g_setup_manager->GetIntL(section, "Collapse", m_collapse));

	SetAlignment(alignment);
	SetAutoAlignment(auto_alignment);
	SetCollapse(collapse);
}

/***********************************************************************************
**
**	OnWriteAlignment
**
***********************************************************************************/

void OpBar::OnWriteAlignment(PrefsFile* prefs_file, const char* name)
{
	TRAPD(err, prefs_file->WriteIntL(name, "Alignment", GetNonFullscreenAlignment()));
	TRAP(err, prefs_file->WriteIntL(name, "Auto alignment", GetNonFullscreenAutoAlignment()));
	TRAP(err, prefs_file->WriteIntL(name, "Old visible alignment", m_old_visible_alignment));
	TRAP(err, prefs_file->WriteIntL(name, "Collapse", m_collapse));

	if(m_alignment_prefs_setting != PrefsCollectionUI::DummyLastIntegerPref)
	{
		TRAP(err, g_pcui->WriteIntegerL(m_alignment_prefs_setting, GetNonFullscreenAlignment()));
	}

	if(m_auto_alignment_prefs_setting != PrefsCollectionUI::DummyLastIntegerPref)
	{
		TRAP(err, g_pcui->WriteIntegerL(m_auto_alignment_prefs_setting, GetNonFullscreenAutoAlignment()));
	}

	g_application->SettingsChanged(SETTINGS_TOOLBAR_ALIGNMENT);
}

/***********************************************************************************
**
**	OnRelayout
**
***********************************************************************************/

void OpBar::OnRelayout(OpWidget* widget)
{
	if (widget != this)
		Relayout(TRUE, FALSE);
	else if (GetParentDesktopWindow())
		GetParentDesktopWindow()->OnRelayout(this);
}

/***********************************************************************************
**
**	OnRelayout
**
***********************************************************************************/

void OpBar::OnRelayout()
{
	if (GetResultingAlignment() == m_used_alignment && GetCollapse() == m_used_collapse)
	{
		if (m_used_alignment == ALIGNMENT_OFF)
			return;

		INT32 used_width;
		INT32 used_height;

		OpRect rect = GetBounds();

		OnLayout(TRUE, rect.width, rect.height, used_width, used_height);

		if (used_width == m_used_width && used_height == m_used_height)
		{
			return;
		}
	}

	OpWidget::OnRelayout();
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void OpBar::OnLayout()
{
	OpRect rect = GetBounds();

#if defined(_MACINTOSH_) 
	/**
	* If a toolbar intersects the growbox corner (always visible) we must shrink
	* a little so that text or icons are not obscured by the growbox.
	*/
	CocoaOpWindow* cocoa_window = (CocoaOpWindow*) GetParentOpWindow();
	if(cocoa_window && GetVisualDevice() && !GetVisualDevice()->IsPrinter())
	{
		OpPoint deltas = cocoa_window->IntersectsGrowRegion(GetScreenRect());
		if(IsVertical())
		{
			rect.height -= deltas.y;
		}
		else
		{
			rect.width -= deltas.x;
		}
	}
#endif

	m_used_alignment = GetResultingAlignment();
	m_used_collapse = GetCollapse();
	OnLayout(FALSE, rect.width, rect.height, m_used_width, m_used_height);
}

/***********************************************************************************
**
**	OnFullscreen
**
***********************************************************************************/

void OpBar::OnFullscreen(BOOL fullscreen, Alignment fullscreen_alignment)
{
	if (GetAlignment() == ALIGNMENT_FLOATING)
		return;

	if(fullscreen)
	{
		m_non_fullscreen_alignment = GetAlignment();
		m_non_fullscreen_auto_alignment = GetAutoAlignment();
		SetAlignment(fullscreen_alignment);
	}
	else
	{
		SetAlignment(m_non_fullscreen_alignment);
		SetAutoAlignment(m_non_fullscreen_auto_alignment);
	}
}

void OpBar::OnDirectionChanged()
{
	Alignment alignment = m_alignment;

	if (GetRTL())
	{
		if (alignment == ALIGNMENT_LEFT)
			alignment = ALIGNMENT_RIGHT;
		if (m_old_visible_alignment == ALIGNMENT_LEFT)
			m_old_visible_alignment = ALIGNMENT_RIGHT;
	}
	else
	{
		if (alignment == ALIGNMENT_RIGHT)
			alignment = ALIGNMENT_LEFT;
		if (m_old_visible_alignment == ALIGNMENT_RIGHT)
			m_old_visible_alignment = ALIGNMENT_LEFT;
	}

	SetAlignment(alignment, TRUE);
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void OpBar::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_TOOLBAR_SETUP))
	{
		Read(OpBar::CONTENTS);
		Read(OpBar::STYLE);
		if (SyncAlignment())
			Read(OpBar::ALIGNMENT);
	}
	else
	{
		if (settings->IsChanged(SETTINGS_TOOLBAR_CONTENTS))
		{
			Read(OpBar::CONTENTS);
		}
		else if (settings->IsChanged(SETTINGS_EXTENSION_TOOLBAR_CONTENTS) && GetType() == WIDGET_TYPE_EXTENSION_SET)
		{
			/* The SETTINGS_TOOLBAR_CONTENTS also covers extension toolbar so
			   don't read the contents again if it was done already. */
			Read(OpBar::CONTENTS);
		}

		if (settings->IsChanged(SETTINGS_TOOLBAR_STYLE))
		{
			Read(OpBar::STYLE);
		}

		if (settings->IsChanged(SETTINGS_TOOLBAR_ALIGNMENT))
		{
			if (SyncAlignment())
				Read(OpBar::ALIGNMENT);
		}
	}

	if (settings->IsChanged(SETTINGS_CUSTOMIZE_END_CANCEL))
	{
		if (m_changed_alignment_while_customizing || m_changed_style_while_customizing || m_changed_content_while_customizing)
		{
			Read();
			m_changed_alignment_while_customizing = FALSE;
			m_changed_style_while_customizing = FALSE;
			m_changed_content_while_customizing = FALSE;
		}
	}

	if (settings->IsChanged(SETTINGS_CUSTOMIZE_END_OK))
	{
		if (m_changed_alignment_while_customizing)
		{
			WriteAlignment();
			m_changed_alignment_while_customizing = FALSE;
		}

		if (m_changed_style_while_customizing)
		{
			WriteStyle();
			m_changed_style_while_customizing = FALSE;
		}

		if (m_changed_content_while_customizing)
		{
			WriteContent();
			m_changed_content_while_customizing = FALSE;
		}
	}

	OnSettingsChanged();
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpBar::OnInputAction(OpInputAction* action)
{
	if (GetMaster() || !IsActionForMe(action))
		return FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SET_ALIGNMENT:
				{
					Alignment alignment = (Alignment) child_action->GetActionData();

					if (alignment == ALIGNMENT_OLD_VISIBLE)
						alignment = GetOldVisibleAlignment();

					child_action->SetSelected(GetAlignment() == alignment);

					return TRUE;
				}
				case OpInputAction::ACTION_SET_AUTO_ALIGNMENT:
				{
					child_action->SetSelected(GetAutoAlignment() == child_action->GetActionData());
					return TRUE;
				}
				case OpInputAction::ACTION_SET_COLLAPSE:
				{
					Collapse collapse = (Collapse) child_action->GetActionData();

					child_action->SetSelected(GetCollapse() == collapse);

					if (TranslateCollapse(collapse) != collapse)
						child_action->SetEnabled(FALSE);

					return TRUE;
				}

			}
			break;
		}

		case OpInputAction::ACTION_SET_ALIGNMENT:
		{
			return SetAlignment((Alignment) action->GetActionData(), TRUE);
		}

		case OpInputAction::ACTION_SET_AUTO_ALIGNMENT:
		{
			return SetAutoAlignment(action->GetActionData(), TRUE);
		}

		case OpInputAction::ACTION_SET_COLLAPSE:
		{
			return SetCollapse((Collapse) action->GetActionData(), TRUE);
		}
	}

	return FALSE;
}

OP_STATUS OpBar::GetSectionName(const OpStringC8& suffix, OpString8& section_name)
{
	if (GetName().IsEmpty())
		return OpStatus::ERR;
	RETURN_IF_ERROR(section_name.Set(GetName()));
	return section_name.Append(suffix);
}
