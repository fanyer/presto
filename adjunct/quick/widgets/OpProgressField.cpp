/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "OpProgressField.h"
#include "modules/util/gen_str.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/util/timecache.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/pi/OpLocale.h"
#include "modules/widgets/OpEdit.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/dragdrop/dragdrop_manager.h"

/***********************************************************************************
**
**	OpProgressField
**
***********************************************************************************/

OP_STATUS OpProgressField::Construct(OpProgressField** obj, ProgressType type)
{
	*obj = OP_NEW(OpProgressField, (type));
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpProgressField::OpProgressField(ProgressType type) 
: m_progress_type(type)
, m_doc_percent(0)
, m_doc_total(0)
, m_total_size(0)
, m_images_total(0)
, m_images_loaded(0)
, m_update(FALSE)
, m_update_when_layout(FALSE)
, m_is_customizing(FALSE)
{
	SetSpyInputContext(this, FALSE);

	if (m_progress_type == PROGRESS_TYPE_STATUS)
	{
		SetGrowValue(2);
	}
	else
	{
		SetJustify(JUSTIFY_CENTER, FALSE);
	}

	if (m_progress_type == PROGRESS_TYPE_CLOCK)
	{
		StartTicker();
	}

	const char* skin = NULL;

	switch (m_progress_type)
	{
		case PROGRESS_TYPE_DOCUMENT:	skin = "Progress Document Skin"; break;
		case PROGRESS_TYPE_IMAGES:		skin = "Progress Images Skin"; break;
		case PROGRESS_TYPE_TOTAL:		skin = "Progress Total Skin"; break;
		case PROGRESS_TYPE_SPEED:		skin = "Progress Speed Skin"; break;
		case PROGRESS_TYPE_ELAPSED:		skin = "Progress Elapsed Skin"; break;
		case PROGRESS_TYPE_STATUS:		skin = "Progress Status Skin"; break;
		case PROGRESS_TYPE_GENERAL:		skin = "Progress General Skin"; break;
		case PROGRESS_TYPE_CLOCK:		skin = "Progress Clock Skin"; break;
		case PROGRESS_TYPE_PLUGIN_DOWNLOAD:	skin = "Progress Plugin Download Skin"; break;
	}

	GetBorderSkin()->SetImage(skin);

	OpStatus::Ignore(g_languageManager->GetString(Str::SI_IDSTR_PROGRESS_DOCUMENT, m_string_progress_document));
	OpStatus::Ignore(g_languageManager->GetString(Str::SI_IDSTR_PROGRESS_IMAGES, m_string_progress_images));
	OpStatus::Ignore(g_languageManager->GetString(Str::SI_IDSTR_PROGRESS_TOTAL, m_string_progress_total));
	OpStatus::Ignore(g_languageManager->GetString(Str::SI_IDSTR_PERSEC, m_string_progress_persec));
	OpStatus::Ignore(g_languageManager->GetString(Str::SI_IDSTR_PROGRESS_SPEED, m_string_progress_speed));
	OpStatus::Ignore(g_languageManager->GetString(Str::SI_IDSTR_PROGRESS_TIME, m_string_progress_time));

	Str::LocaleString string_id = Str::NOT_A_STRING;

	// only get the string for the specific progress bar type we need
	switch (m_progress_type)
	{
		case PROGRESS_TYPE_DOCUMENT:	string_id = Str::S_PROGRESS_TYPE_DOCUMENT; break;
		case PROGRESS_TYPE_IMAGES:		string_id = Str::S_PROGRESS_TYPE_IMAGES; break;
		case PROGRESS_TYPE_TOTAL:		string_id = Str::S_PROGRESS_TYPE_TOTAL; break;
		case PROGRESS_TYPE_SPEED:		string_id = Str::S_PROGRESS_TYPE_SPEED; break;
		case PROGRESS_TYPE_ELAPSED:		string_id = Str::S_PROGRESS_TYPE_ELAPSED; break;
		case PROGRESS_TYPE_STATUS:		string_id = Str::S_PROGRESS_TYPE_STATUS; break;
		case PROGRESS_TYPE_GENERAL:		string_id = Str::S_PROGRESS_TYPE_GENERAL; break;
		case PROGRESS_TYPE_CLOCK:		string_id = Str::S_PROGRESS_TYPE_CLOCK; break;
	}
	RETURN_VOID_IF_ERROR(g_languageManager->GetString(string_id, m_string_progress_types[m_progress_type]));

	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(0, 300);
}

OP_STATUS OpProgressField::SetPluginMimeType(const OpStringC& mime_type)
{
	// There should be no need to change the mime-type, with the way the
	// OpProgressField with the PROGRESS_TYPE_PLUGIN_DOWNLOAD type is used
	// this means a problem in the calling code.
	// Feel free to remove this assert if future code changes a mime-type
	// for an OpProgressField for a good reason.
	OP_ASSERT(m_plugin_mime_type.IsEmpty());

	// We can only set the mime-type to track for type PROGRESS_TYPE_PLUGIN_DOWNLOAD
	if (m_progress_type != PROGRESS_TYPE_PLUGIN_DOWNLOAD)
		return OpStatus::ERR;

	// We won't be able to track anything if the mime-type is empty,
	// we don't allow that to happen.
	if (mime_type.IsEmpty())
		return OpStatus::ERR;

	m_doc_percent = 0;
	RETURN_IF_ERROR(g_plugin_install_manager->AddListener(this));

	return m_plugin_mime_type.Set(mime_type);
}

void OpProgressField::OnFileDownloadProgress(const OpStringC& mime_type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long estimate)
{
	OP_ASSERT(m_progress_type == PROGRESS_TYPE_PLUGIN_DOWNLOAD);
	OP_ASSERT(m_plugin_mime_type.HasContent());

	// See if this call is about the mime-type we're tracking
	if (m_plugin_mime_type.CompareI(mime_type))
		return;

	m_doc_percent = (float(downloaded_size) / total_size) * 100;
	m_doc_total = 0;
	InvalidateAll();
}

OP_STATUS OpProgressField::SetText(const uni_char* text)
{
	if (!text)
		text = UNI_L("");

	const uni_char *old_text = m_widget_string.Get();

	if(old_text && !uni_strcmp(old_text, text))
	{
		// no need to set the text if it hasn't changed
		return OpStatus::OK;
	}
	// We don't need the underlying OpLabel in PROGRESS_TYPE_DOCUMENT, PROGRESS_TYPE_GENERAL or
	// PROGRESS_TYPE_TOTAL mode as it uses DrawProgressBar to do this
	if (UseLabel())
		RETURN_IF_ERROR(OpLabel::SetText(text));

	OP_STATUS status = m_widget_string.Set(text, this);
	if (!UseLabel())
		Invalidate(GetBounds());
	return status;
}

OP_STATUS OpProgressField::SetLabel(const uni_char* newlabel, BOOL center)
{
	if (!newlabel)
		newlabel = UNI_L("");

	const uni_char *old_text = m_widget_string.Get();

	if(old_text && !uni_strcmp(old_text, newlabel))
	{
		// no need to set the text if it hasn't changed
		return OpStatus::OK;
	}
	// We don't need the underlying OpLabel in PROGRESS_TYPE_DOCUMENT, PROGRESS_TYPE_GENERAL or
	// PROGRESS_TYPE_TOTAL mode as it uses DrawProgressBar to do this
	if (UseLabel())
		RETURN_IF_ERROR(OpLabel::SetLabel(newlabel, center));

	OP_STATUS status = m_widget_string.Set(newlabel, this);
	if (!UseLabel())
		Invalidate(GetBounds());
	return status;
}

void OpProgressField::OnDeleted()
{
	if (m_plugin_mime_type.HasContent())
		// We can't do much if removing the listener fails.
		OpStatus::Ignore(g_plugin_install_manager->RemoveListener(this));

	SetSpyInputContext(NULL, FALSE);
	StopTicker();
	OpLabel::OnDeleted();
}

/***********************************************************************************
**
**	StartTicker
**
***********************************************************************************/

OpProgressField::Ticker*	OpProgressField::s_ticker = NULL;

void OpProgressField::StartTicker()
{
	if (!s_ticker)
	{
		if ((s_ticker = OP_NEW(Ticker, ())) == NULL)
			return;
	}
	else
	{
		if (s_ticker->m_progress_fields.Find(this) >= 0) return;
	}
	s_ticker->m_progress_fields.Add(this);
}

/***********************************************************************************
**
**	StopTicker
**
***********************************************************************************/

void OpProgressField::StopTicker()
{
	if (s_ticker)
	{
		s_ticker->m_progress_fields.RemoveByItem(this);
		if (s_ticker->m_progress_fields.GetCount() == 0)
		{
			OP_DELETE(s_ticker);
			s_ticker = NULL;
		}
	}
}

/***********************************************************************************
**
**	OnPaint
**
***********************************************************************************/

void OpProgressField::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (UseLabel())
	{
		OpLabel::OnPaint(widget_painter, paint_rect);
	}
	else
	{
		painter_manager->DrawProgressbar(GetBounds(), m_doc_percent, m_doc_total,
			&m_widget_string, GetBorderSkin()->GetImage(), GetForegroundSkin()->GetImage());
	}
}

/***********************************************************************************
**
**	OnAdded
**
***********************************************************************************/

void OpProgressField::OnAdded()
{
	UpdateProgress(TRUE);
	InvalidateAll();
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void OpProgressField::OnSettingsChanged(DesktopSettings* settings)
{
	OpLabel::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_CUSTOMIZE_UPDATE))
	{
		UpdateProgress(TRUE);
		InvalidateAll();
	}
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpProgressField::OnDragStart(const OpPoint& point)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(WIDGET_TYPE_PROGRESS_FIELD, point.x, point.y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		drag_object->SetID(m_progress_type);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
**
**	GetPreferedSize
**
***********************************************************************************/

void OpProgressField::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	INT32 left, top, right, bottom;

	GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);

	if (m_progress_type == PROGRESS_TYPE_CLOCK)
	{
		// Fix for bug #254351, changed padding from 8 to 4.
		// Not a real fix but works adamm 13-04-2007
		*w = GetTextWidth() + 4;
	}
	else
	{
		OpWidgetString dummy_scale_string;
		dummy_scale_string.Set(UNI_L("Document:10000%"), this);
		*w = dummy_scale_string.GetWidth() + left + right + 4;
	}

	*h = GetTextHeight() + 2;
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void OpProgressField::OnLayout()
{
	if (m_update_when_layout)
	{
		UpdateProgress(FALSE);
		m_update_when_layout = FALSE;
	}
}

/***********************************************************************************
**
**	IsUpdateNeeded
**
***********************************************************************************/

BOOL OpProgressField::IsUpdateNeeded()
{
	if ((!GetParentDesktopWindow() || g_application->IsCustomizingToolbars()) && !m_is_customizing)
		return TRUE;
	if (!(!GetParentDesktopWindow() || g_application->IsCustomizingToolbars()) && m_is_customizing)
		return TRUE;

	if (m_progress_type == PROGRESS_TYPE_CLOCK)
		return TRUE;

	if (!GetTargetDocumentDesktopWindow() || (!m_update && m_progress_type != PROGRESS_TYPE_ELAPSED))
		return FALSE;

	OpWindowCommander *win_comm = GetTargetDocumentDesktopWindow()->GetWindowCommander();
	BOOL is_loading = win_comm->IsLoading();
 	if (!is_loading)
		return FALSE;

	return TRUE;

}

/***********************************************************************************
**
**	UpdateProgress
**
***********************************************************************************/

void OpProgressField::UpdateProgress(BOOL force)
{
	if (!GetParentDesktopWindow() || g_application->IsCustomizingToolbars())
	{
		SetText(m_string_progress_types[m_progress_type].CStr());
		m_is_customizing = TRUE;
		return;
	}
	m_is_customizing = FALSE;

	if (m_progress_type == PROGRESS_TYPE_CLOCK)
	{
		time_t now = g_timecache->CurrentTime();

		struct tm *datelocal = localtime(&now);

		uni_char buf[128];

		unsigned int len = g_oplocale->op_strftime(buf, 128,
												   g_oplocale->Use24HourClock() ? UNI_L("%H:%M") : UNI_L("%I:%M %p"),
												   datelocal);
		buf[len] = 0;

		SetText(buf);
		return;
	}

	if (!GetTargetDocumentDesktopWindow() || (!m_update && !force && m_progress_type != PROGRESS_TYPE_ELAPSED))
		return;

	m_update = FALSE;

	OpWindowCommander *win_comm = GetTargetDocumentDesktopWindow()->GetWindowCommander();
	BOOL is_loading = win_comm->IsLoading();
 	if (!force && !is_loading)
 		return;

	INT32 tim = g_timecache->CurrentTime() - WindowCommanderProxy::GetProgressStartTime(win_comm);

	switch (m_progress_type)
	{
		case PROGRESS_TYPE_DOCUMENT:
		case PROGRESS_TYPE_GENERAL:
		{
			m_doc_total = WindowCommanderProxy::GetDocProgressCount(win_comm);

			if (m_doc_total < 0)
			{
				m_doc_total = 0;
			}

			URL url = WindowCommanderProxy::GetCurrentURL(win_comm);
			OP_MEMORY_VAR INT32 total_size = (INT32)url.GetContentSize();

			if(total_size == 0 && url.Status(TRUE) != URL_LOADING)
			{
				total_size = m_doc_total;
			}

			m_doc_percent = 0;

			uni_char str[32];

			if (total_size)
			{
				m_doc_percent = (INT32) ((INT64)m_doc_total * (INT64)100 / (INT64)total_size);
				if (m_doc_percent > 100)
				{
					m_doc_percent = 100;
				}
				uni_snprintf(str, 31, UNI_L("%d%c"), m_doc_percent, '%');
				m_doc_total = 0;
			}
			else
			{
				StrFormatByteSize( str, 31, m_doc_total, SFBS_ABBRIVATEBYTES);
			}

			OpString out_str;
			out_str.AppendFormat(UNI_L("%s   %s"), m_string_progress_document.CStr(), str);

			SetText(out_str.CStr());

			// fall through to if general type and document is complete

			if (m_progress_type != PROGRESS_TYPE_GENERAL || m_doc_percent != 100)
				break;
		}
	case PROGRESS_TYPE_IMAGES:
		{
			OpString out_str;
			out_str.AppendFormat(UNI_L("%s   %d/%d"), m_string_progress_images.CStr(), m_images_loaded, m_images_total);

			SetText(out_str.CStr());

			break;
		}
	case PROGRESS_TYPE_TOTAL:
		{
			uni_char str[60];

			m_total_size = WindowCommanderProxy::GetProgressCount(win_comm);

			if (m_total_size < 0)
			{
				m_total_size = 0;
			}

			StrFormatByteSize( str, 60-1, m_total_size, SFBS_ABBRIVATEBYTES);

			OpString out_str;
			out_str.AppendFormat(UNI_L("%s   %s"), m_string_progress_total.CStr(), str);
			SetText(out_str.CStr());
			break;
		}
	case PROGRESS_TYPE_SPEED:
		{
			INT32 size = WindowCommanderProxy::GetProgressCount(win_comm);

			uni_char str[60];

			if (size < 0 || tim<=0)
			{
				uni_strcpy( str, UNI_L("?"));
			}
			else
			{
				size = size / tim;
				StrFormatByteSize( str, 60-1, size, SFBS_ABBRIVATEBYTES);

				uni_strcat( str, m_string_progress_persec.CStr());
			}

			OpString out_str;
			out_str.AppendFormat(UNI_L("%s   %s"), m_string_progress_speed.CStr(), str);
			SetText(out_str.CStr());

			break;
		}
	case PROGRESS_TYPE_ELAPSED:
		{
			uni_char str[30];

			int h  = tim / 3600;
			int m  = (tim - h * 3600) / 60;
			int s  = (tim - h * 3600) % 60;
			if( h > 0 )
			{
				uni_sprintf(str, UNI_L("%d:%02d:%02d"), h,m,s);
			}
			else
			{
				uni_sprintf(str, UNI_L("%d:%02d"), m,s);
			}

			/*
			int min = tim / 60;
			int sec = tim % 60;

			uni_sprintf(str, UNI_L("%d:"), min);
			if (sec < 10)
			uni_sprintf(str+uni_strlen(str), UNI_L("0%d"), sec);
			else
			uni_sprintf(str+uni_strlen(str), UNI_L("%d"), sec);
			*/

			OpString out_str;
			out_str.AppendFormat(UNI_L("%s   %s"), m_string_progress_time.CStr(), str);
			SetText(out_str.CStr());

			break;
		}
	case PROGRESS_TYPE_STATUS:
		{
			SetText(WindowCommanderProxy::GetProgressMessage(win_comm));
			break;
		}
	}
}
