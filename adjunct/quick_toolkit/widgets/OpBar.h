/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_BAR_H
#define OP_BAR_H

#include "modules/widgets/OpWidget.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

/***********************************************************************************
**
**	OpBar
**
**	OpBar is a superclass for all bar classes and gives the user
**	of a bar different ways to layout a bar. For example:
**
**	Just call SetRect to put it somewhere
**	or call LayoutToAvailableRect() to make the bar take what it needs
**	or call any of the Get() functions to find out what it wants.. (TODO)
**
**	In other words, choose what makes your code the simplest.
**
**	The real bar subclasses, however, only implements the OnLayout hook, nothing more.
**
***********************************************************************************/

class OpBar : public OpWidget
{
	public:

		enum Alignment
		{
			ALIGNMENT_OFF = 0,
			ALIGNMENT_LEFT,
			ALIGNMENT_TOP,
			ALIGNMENT_RIGHT,
			ALIGNMENT_BOTTOM,
			ALIGNMENT_FLOATING,
			ALIGNMENT_OLD_VISIBLE
		};

		enum Wrapping
		{
			WRAPPING_OFF = 0,
			WRAPPING_NEWLINE,
			WRAPPING_EXTENDER
		};

		enum Collapse
		{
			COLLAPSE_SMALL = 0,
			COLLAPSE_NORMAL
		};

		enum SettingsType
		{
			ALL			= 0xffff,
			ALIGNMENT	= 0x0001,
			STYLE		= 0x0002,
			CONTENTS	= 0x0004		
		};

								OpBar(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref, PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);

		void					SetMaster(BOOL master) {m_master = master;}
		BOOL					GetMaster() {return m_master;}

		void					SetSupportsReadAndWrite(BOOL read_and_write) {m_supports_read_and_write = read_and_write;}
		BOOL					GetSupportsReadAndWrite() {return m_supports_read_and_write;}

		virtual Collapse		TranslateCollapse(Collapse collapse) {return collapse;}

		virtual BOOL			SetAlignment(Alignment alignment, BOOL write_to_prefs = FALSE);
		BOOL 					SetAutoAlignment(BOOL auto_alignment, BOOL write_to_prefs = FALSE);

		Alignment				GetAlignment() {return m_alignment;}
		BOOL 					GetAutoAlignment() const {return m_auto_alignment;}

		Alignment				GetNonFullscreenAlignment() {return IsFullscreen() ? m_non_fullscreen_alignment : GetAlignment();}
		BOOL					GetNonFullscreenAutoAlignment() {return IsFullscreen() ? m_non_fullscreen_auto_alignment : GetAutoAlignment();}

		Alignment				GetOldVisibleAlignment() {return m_old_visible_alignment;}
		Alignment				GetResultingAlignment();

		// Should we sync the alignment setting for this toolbar between windows, this should
		// be true for fixed-position bars like bookmark bar, address bar etc, and false for
		// appear-on-demand bar like find bar, content block bar etc.
		virtual	BOOL			SyncAlignment() {return m_alignment_prefs_setting != PrefsCollectionUI::DummyLastIntegerPref;}

		BOOL					SetAutoVisible(BOOL auto_visible);
		BOOL					GetAutoVisible() {return m_auto_visible;}

		BOOL					IsVertical();
		BOOL					IsHorizontal();
		BOOL					IsOff();
		BOOL					IsOn() {return !IsOff();}

		INT32					GetWidthFromHeight(INT32 height, INT32* used_height = NULL);
		INT32					GetHeightFromWidth(INT32 width, INT32* used_width = NULL);

		virtual void			GetRequiredSize(INT32& width, INT32& height);
		void 					GetRequiredSize(INT32 max_width, INT32 max_height, INT32& width, INT32& height);

		void					WriteContent();

		void					RestoreToDefaults();

		// Let the bar eat a portion of the rect passed according to its alignment, and return
		// a shrunken rect that does not contain the area this bar took. If the bar is set to be
		// off, it will eat nothing and instead ensure that it is not visible. 

		virtual	OpRect			LayoutToAvailableRect(const OpRect& rect, BOOL compute_rect_only = FALSE, INT32 banner_x = 0, INT32 banner_y = 0);

		OpRect					LayoutChildToAvailableRect(OpBar* child, const OpRect& rect, BOOL compute_rect_only = FALSE, INT32 banner_x = 0, INT32 banner_y = 0)
			{ return child->LayoutToAvailableRect(AdjustForDirection(rect), compute_rect_only, banner_x, banner_y); }

		virtual void			OnSettingsChanged(DesktopSettings* settings);
		virtual void			OnRelayout();
		virtual void			OnLayout();
		virtual void			OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height) {used_width = available_width; used_height = available_height;}
		virtual void 			OnFullscreen( BOOL fullscreen, Alignment fullscreen_alignment = ALIGNMENT_OFF );

		virtual void			OnDirectionChanged();

		/**
		 * Implementing OpWidget::OnNameSet so that OpBar will be
		 * notified when the name of the widget is set - when it
		 * is, we can read in the configuration of the bar.
		 */
		virtual void OnNameSet();

		// Implementing the OpWidgetListener interface

		virtual void			OnRelayout(OpWidget* widget);

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);

	protected:
		BOOL					SetCollapse(Collapse collapse, BOOL write_to_prefs = FALSE);
		Collapse				GetCollapse() {return m_collapse;}

		void					ReadAlignment(BOOL master);
		void					WriteStyle();

		virtual void			OnReadAlignment(PrefsSection* section);
		virtual void			OnWriteAlignment(PrefsFile* prefs_file, const char* name);

		//
		// Hooks
		//
		
		virtual void			OnReadStyle(PrefsSection *section) {}
		virtual void			OnReadContent(PrefsSection *section) {}

		virtual void			OnWriteStyle(PrefsFile* prefs_file, const char* name) {}
		virtual void			OnWriteContent(PrefsFile* prefs_file, const char* name) {}

		virtual void			OnSettingsChanged() {}
		virtual void			OnAlignmentChanged() {}
		virtual void			OnAutoAlignmentChanged() {}

	private:
		void					Read(SettingsType settings = OpBar::ALL, BOOL master = FALSE);
		void					ReadStyle(BOOL master);
		void					ReadContents(BOOL master);

		void					Write() {WriteAlignment(); WriteStyle(); WriteContent();}
		void					WriteAlignment();

		OP_STATUS				GetSectionName(const OpStringC8& suffix, OpString8& section_name);

		INT32						m_used_width;
		INT32						m_used_height;
		Alignment					m_used_alignment;
		Collapse					m_used_collapse;

		Alignment					m_alignment;
		BOOL						m_auto_alignment;

		Alignment					m_non_fullscreen_alignment;
		BOOL						m_non_fullscreen_auto_alignment;

		Alignment					m_old_visible_alignment;
		BOOL						m_auto_visible;

		PrefsCollectionUI::integerpref	m_alignment_prefs_setting;
		PrefsCollectionUI::integerpref	m_auto_alignment_prefs_setting;

		Collapse					m_collapse;

		BOOL						m_changed_alignment_while_customizing;
		BOOL						m_changed_style_while_customizing;
		BOOL						m_changed_content_while_customizing;
		BOOL						m_master;
		BOOL						m_supports_read_and_write;
};

#endif // OP_BAR_H
