/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OPEN_URL_SETTING_H
#define OPEN_URL_SETTING_H

#include "modules/url/url2.h"
#include "modules/history/direct_history.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DesktopWindow;
class FramesDocument;
class DesktopWindowCollectionItem;

// This class shall only be used for actions that are
// entered by the user.
class OpenURLSetting
{
public:
	OpenURLSetting()
	  : m_src_commander           (NULL),
		m_document                (0),
		m_from_address_field      (FALSE),
		m_save_address_to_history (FALSE),
		m_new_window              (NO),
		m_new_page                (NO),
		m_in_background           (NO),
		m_target_window           (NULL),
		m_target_position         (-1),
		m_target_parent           (NULL),
		m_context_id              ((URL_CONTEXT_ID)-1),
		m_ignore_modifier_keys    (FALSE),
		m_digits_are_speeddials   (FALSE),
		m_is_hotlist_url		  (FALSE),
		m_is_remote_command		  (FALSE),
		m_is_privacy_mode		  (FALSE),
		m_force_new_tab			  (FALSE),
		m_type					  (DirectHistory::TEXT_TYPE),
		m_has_src_settings		  (FALSE)
	{}

	OpenURLSetting(const OpenURLSetting& src)
	{
		*this = src;
	}

	OpenURLSetting& operator=(const OpenURLSetting& src)
	{
		m_address.Set(src.m_address);
		m_typed_address.Set(src.m_typed_address);
		m_src_commander           = src.m_src_commander;
		m_document                = src.m_document;
		m_from_address_field      = src.m_from_address_field;
		m_save_address_to_history = src.m_save_address_to_history;
		m_new_window              = src.m_new_window;
		m_new_page                = src.m_new_page;
		m_in_background           = src.m_in_background;
		m_target_window           = src.m_target_window;
		m_target_position         = src.m_target_position;
		m_target_parent           = src.m_target_parent;
		m_context_id              = src.m_context_id;
		m_ignore_modifier_keys    = src.m_ignore_modifier_keys;
		m_digits_are_speeddials   = src.m_digits_are_speeddials;
		m_is_hotlist_url		  = src.m_is_hotlist_url;
		m_is_remote_command		  = src.m_is_remote_command;
		m_is_privacy_mode		  = src.m_is_privacy_mode;
		m_force_new_tab			  = src.m_force_new_tab;
		m_type					  = src.m_type;
		m_has_src_settings		  = src.m_has_src_settings;
		m_src_css_mode			  = src.m_src_css_mode;
		m_src_scale				  = src.m_src_scale;
		m_src_encoding			  = src.m_src_encoding;
		m_src_image_mode		  = src.m_src_image_mode;

		return *this;
	}
	
	OpString		m_address;
	URL				m_url;
	URL				m_ref_url;
	
	// Contains a pointer to the Window that opened this tab. Used to decide some settings 
	// for the new tab(privacy mode, CSS mode and image setting etc) and for the logic on 
	// what tab to activate next. This is only guaranteed to be valid when opening a new tab
	// synchorously, after that it shouldn't be used as a pointer.
	OpWindowCommander*	m_src_commander;
	FramesDocument*		m_document;
	
	BOOL			m_from_address_field;		///< Url from address bar always open in the current tab except when shift key was pressed.
	BOOL            m_save_address_to_history;	///< Should address be saved to typed history if not opened in private tab
	BOOL3			m_new_window;				///< Should the url be openend in a new window. MAYBE means let Opera choose.
	BOOL3			m_new_page;					///< Should the url be openend in a new page (a tab). MAYBE means let Opera choose.
	BOOL3			m_in_background;			///< Should the url be openend in the background (window is not activated). MAYBE means let Opera choose.
	DesktopWindow*	m_target_window;			///< Pointer to the DesktopWindow in which the url should be opened and, if available, pointer to the window that has actually been opened
	INT32           m_target_position;			///< Desired position in the tab order
	DesktopWindowCollectionItem* m_target_parent; ///< Desired parent in the tab order
	URL_CONTEXT_ID  m_context_id;				///< ID that is used by core to know where the URL came from
	BOOL			m_ignore_modifier_keys;		///< Should shift, control (command) keys be checked to see if the user want to force it in a new window and/or background?
	BOOL			m_digits_are_speeddials;	///< Should numbers be translated to speeddials?
	BOOL			m_is_hotlist_url;			///< Set to TRUE for urls opened by clicking a bookmark
	BOOL			m_is_remote_command;		///< Set to TRUE if urls are supplied by the platform remote command channel (dde, mac-events, X-events, etc.)
	BOOL			m_is_privacy_mode;			///< Set to TRUE if urls should be opened in privacy mode
	BOOL			m_force_new_tab;			///< Set to TRUE to force open a new tab even if the URL is empty

	// Information used to resolve the url, used to save the original input from user 
	// which will be saved to typed history if needed
	OpString		m_typed_address;			///< If this isn't empty it's what was originally typed by user
	DirectHistory::ItemType	m_type;				///< If m_typed_address isn't empty this is the type of m_typed_address

	// Source window setttings, use them when m_src_commander is NULL
	BOOL                                 m_has_src_settings; ///< Set to TRUE if below src settings have to be used
	OpDocumentListener::CssDisplayMode   m_src_css_mode;
	INT32                                m_src_scale;
	OpWindowCommander::Encoding          m_src_encoding;
	OpDocumentListener::ImageDisplayMode m_src_image_mode;
};

#endif // OPEN_URL_SETTING_H
