/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef INTERNATIONAL_FONTS_DIALOG_H
#define INTERNATIONAL_FONTS_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/windowcommander/WritingSystem.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

const UINT8 num_blocks = 46;

class InternationalFontsDialog : public Dialog
{
	public:
		InternationalFontsDialog();
		virtual ~InternationalFontsDialog();

		Type				GetType()				{return DIALOG_TYPE_INTERNATIONAL_FONTS;}
		const char*			GetWindowName()			{return "International Fonts Dialog";}
		const char*			GetHelpAnchor()			{return "fonts.html#international";}

		void				Init(DesktopWindow* parent_window);
		void				OnInit();
		void				OnChange(OpWidget* widget, BOOL changed_by_mouse);
		UINT32				OnOk();

	private:
		const OpFontInfo*	GetAutomaticFont(UINT8 block_nr, WritingSystem::Script script, BOOL monospace);
		BOOL				IsValidFont(OpFontInfo &fontinfo, UINT8 block_nr, WritingSystem::Script script);

		void				OnNormalFontChange();
		void				OnMonoTypeFontChange();
		void				OnEncodingChange();

		uni_char**			m_normal_list;
		uni_char**			m_mono_list;
		const uni_char*		m_cached_normal[num_blocks];
		const uni_char*		m_cached_mono[num_blocks];
		UINT32				m_normal_count;
		UINT32				m_mono_count;
		OpString			m_default_choice_str;
		OpString			m_no_fonts_str;
		uni_char			m_automatic_normal_str[100];
		uni_char			m_automatic_mono_str[100];

		SimpleTreeModel		m_normal_model;
		SimpleTreeModel		m_monotype_model;
};


#endif //this file
