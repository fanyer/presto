/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */
#include "core/pch.h"

#include "adjunct/quick/controller/ModeManagerController.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"


void ModeManagerController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Mode Manager Dialog"));
	LEAVE_IF_ERROR(InitOptions());
}

OP_STATUS ModeManagerController::InitOptions()
{
	//Preference bindings for Checkboxes.
	RETURN_IF_ERROR(GetBinder()->Connect("Enable_frames_checkbox", *g_pcdisplay, PrefsCollectionDisplay::FramesEnabled));
	RETURN_IF_ERROR(GetBinder()->Connect("Enable_inline_frames_checkbox", *g_pcdisplay, PrefsCollectionDisplay::IFramesEnabled));
	RETURN_IF_ERROR(GetBinder()->Connect("Show_frame_border_checkbox", *g_pcdisplay, PrefsCollectionDisplay::ShowActiveFrame));
	RETURN_IF_ERROR(GetBinder()->Connect("Forms_styling_checkbox", *g_pcdisplay, PrefsCollectionDisplay::EnableStylingOnForms));
	RETURN_IF_ERROR(GetBinder()->Connect("Scrollbars_styling_checkbox", *g_pcdisplay, PrefsCollectionDisplay::EnableScrollbarColors));
	
	RETURN_IF_ERROR(GetBinder()->Connect("Author_page_style_sheet_checkbox", *g_pcdisplay, PrefsCollectionDisplay::DM_AuthorCSS));
	RETURN_IF_ERROR(GetBinder()->Connect("Author_page_fonts_and_colors_checkbox", *g_pcdisplay, PrefsCollectionDisplay::DM_AuthorFonts));
	RETURN_IF_ERROR(GetBinder()->Connect("Author_my_style_sheet_checkbox", *g_pcdisplay, PrefsCollectionDisplay::DM_UserCSS));
	RETURN_IF_ERROR(GetBinder()->Connect("Author_my_fonts_and_colors_checkbox", *g_pcdisplay, PrefsCollectionDisplay::DM_UserFonts));
	RETURN_IF_ERROR(GetBinder()->Connect("Author_my_link_style_checkbox", *g_pcdisplay, PrefsCollectionDisplay::DM_UserLinks));

	RETURN_IF_ERROR(GetBinder()->Connect("User_page_style_sheet_checkbox", *g_pcdisplay, PrefsCollectionDisplay::UM_AuthorCSS));
	RETURN_IF_ERROR(GetBinder()->Connect("User_page_fonts_and_colors_checkbox", *g_pcdisplay, PrefsCollectionDisplay::UM_AuthorFonts));
	RETURN_IF_ERROR(GetBinder()->Connect("User_my_style_sheet_checkbox", *g_pcdisplay, PrefsCollectionDisplay::UM_UserCSS));
	RETURN_IF_ERROR(GetBinder()->Connect("User_my_fonts_and_colors_checkbox", *g_pcdisplay, PrefsCollectionDisplay::UM_UserFonts));
	RETURN_IF_ERROR(GetBinder()->Connect("User_my_link_style_checkbox", *g_pcdisplay, PrefsCollectionDisplay::UM_UserLinks));

	RETURN_IF_ERROR(GetBinder()->Connect("My_style_sheet_chooser", PrefsCollectionFiles::LocalCSSFile));

	RETURN_IF_ERROR(GetBinder()->Connect("Default_mode_dropdown", *g_pcdisplay, PrefsCollectionDisplay::DocumentMode));

	return OpStatus::OK;
}
