/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "OpProgressbar.h"
#include "modules/display/vis_dev.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpWidgetPainterManager.h"

/*****************************************************************************
**
**	OpProgressBar
**
*****************************************************************************/

DEFINE_CONSTRUCT(OpProgressBar)


OpProgressBar::OpProgressBar()
:	m_current_step(0),
	m_total_steps(0),
	m_timer(NULL),
	m_ms(200),
	m_type(Normal)
{
	SetSkinned(TRUE);
}

OpProgressBar::~OpProgressBar()
{
	StopIndeterminateBar();
}

/*
**  Makes a string with the percent loaded so far (ie : "25%") and places
**  it as a label in the progress bar.
**
** OpProgressBar::GetPercentage
 */
void OpProgressBar::SetPercentageLabel()
{
	double current = (double)(INT64) GetCurrentStep();
	double total      = (double)(INT64) GetTotalSteps();

	if(total < current)
	{
		return;
	}

	// FIXME: comparing double != 0.0 is not robust; chose an error bar !
	OP_MEMORY_VAR double percent = (current != 0.0 && total != 0.0) ? (current / total) * 100 : 0;

	INT32 percent_int = static_cast<INT32>(percent);
	if (percent_int == 0)
	{
		percent_int = 1;
	}

	OpString percent_string;
	percent_string.AppendFormat(UNI_L("%.1f%%"), percent);

	SetLabel(percent_string.CStr(), TRUE);
}

void OpProgressBar::SetType(ProgressType type)
{
	m_type = type;

	if (type == Spinner)
	{
		const char* image = m_spinner_image.HasContent() ? m_spinner_image.CStr() : "Thumbnail Reload Image";
		GetForegroundSkin()->SetImage(image);
		SetVisibility(FALSE);
	}
	else if (type == Percentage_Label)
	{
		SetPercentageLabel();
	}
}
void OpProgressBar::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpRect rect = GetBounds();

	switch (m_type)
	{
		case Spinner:
			// Draw the spinner image
			GetForegroundSkin()->Draw(vis_dev, rect);

			// This is supposed to flow through
		case Only_Label:
			OpLabel::OnPaint(widget_painter, paint_rect);
		break;

		//case Normal:
		default:
		{
			if (m_total_steps == 0)
				painter_manager->DrawProgressbar(rect, 0, m_current_step, m_type == Normal ? &m_widget_string : NULL, m_progressbar_skin_empty.CStr() , m_progressbar_skin_full.CStr());
			else
				painter_manager->DrawProgressbar(rect, (double) ((INT64)m_current_step * 100) / (double)(INT64)m_total_steps, 0, m_type == Normal ? &m_widget_string : NULL, m_progressbar_skin_empty.CStr(), m_progressbar_skin_full.CStr());
		}
		break;
	}
}

OP_STATUS OpProgressBar::SetText(const uni_char* text)
{
	if (!text)
		text = UNI_L("");

	const uni_char *old_text = m_widget_string.Get();

	if(old_text && !uni_strcmp(old_text, text))
	{
		// no need to set the text if it hasn't changed
		return OpStatus::OK;
	}
	// We don't need the underlying OpLabel in "normal" mode as it uses DrawProgressBar to do this
	if (m_type != Normal)
		RETURN_IF_ERROR(OpLabel::SetText(text));
	else
		RETURN_IF_ERROR(OpLabel::SetText(UNI_L("")));

	return m_widget_string.Set(text, this);
}

OP_STATUS OpProgressBar::SetLabel(const uni_char* newlabel, BOOL center)
{
	if (!newlabel)
		newlabel = UNI_L("");

	const uni_char *old_text = m_widget_string.Get();

	if(old_text && !uni_strcmp(old_text, newlabel))
	{
		// no need to set the text if it hasn't changed
		return OpStatus::OK;
	}
	// We don't need the underlying OpLabel in "normal" mode as it uses DrawProgressBar to do this
	if (m_type != Normal)
		RETURN_IF_ERROR(OpLabel::SetLabel(newlabel, center));
	else
		RETURN_IF_ERROR(OpLabel::SetLabel(UNI_L(""), center));

	return m_widget_string.Set(newlabel, this);
}

void OpProgressBar::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if (m_type == Spinner)
	{
		GetForegroundSkin()->GetSize(w, h);
	}
	else
	{
		OpLabel::GetPreferedSize(w, h, cols, rows);
	}
}

void OpProgressBar::SetProgress(OpFileLength current_step, OpFileLength total_steps)
{
	if (current_step > total_steps && total_steps)
		current_step = total_steps;

	if (current_step == m_current_step && total_steps == m_total_steps)
		return;

	m_current_step = current_step;
	m_total_steps = total_steps;
	m_delayed_trigger.InvokeTrigger();

	if (m_type == Percentage_Label)
	{
		SetPercentageLabel();
	}
}

void OpProgressBar::RunIndeterminateBar(UINT32 ms)
{
	if (m_type == Spinner)
		SetVisibility(TRUE);
	else
	{
		if (!m_timer)
		{
			if (!(m_timer = OP_NEW(OpTimer, ())))
				return;

			m_timer->SetTimerListener(this);
		}
		m_timer->Start(ms);
	}
}

void OpProgressBar::StopIndeterminateBar()
{
	if (m_type == Spinner)
		SetVisibility(FALSE);
	else
	{
		if (m_timer)
		{
			m_timer->Stop();
			OP_DELETE(m_timer);
			m_timer = NULL;
		}
	}
}

OP_STATUS OpProgressBar::SetProgressBarEmptySkin(const char* skin)
{ 
	OP_STATUS status = OpStatus::ERR;
	if (skin && *skin)
	{
		status = m_progressbar_skin_empty.Set(skin);
		GetBorderSkin()->SetImage(skin);
	}
	return status;
}

OP_STATUS OpProgressBar::SetProgressBarFullSkin(const char* skin)
{ 
	OP_STATUS status = OpStatus::ERR;
	if (skin && *skin)
	{
		status = m_progressbar_skin_full.Set(skin);
	}
	return status;
}


void OpProgressBar::OnTimeOut(OpTimer* timer)
{
	if (m_timer)
	{
		// 4097 is the magic number, it's stupid, since the IndpWidgetPainter assumes
		// it will be receiving bytes which are converted to Kb so we need a multiple
		// of this just to make it work but we also have to add 1 so native progress
		// bars work too! Yuk!!!
		SetProgress(m_current_step + 4097, m_total_steps);
		m_timer->Start(m_ms);
	}
}

void OpProgressBar::OnScaleChanged()
{
	OpLabel::OnScaleChanged();
	m_widget_string.NeedUpdate();
}
