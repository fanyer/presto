/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file prefstypes.h
 * Types used in the preference module.
 *
 * Various typedef, enum and #define data types used by the preferences code
 * to store preferences. The preferences code is quite agnostic as to what
 * it stores, these types are really defined here as a courtesy for the
 * caller and for historical purposes.
 */

#ifndef PREFSTYPES_H
#define PREFSTYPES_H

#ifdef PREFS_HAVE_WORKSPACE
/** Which buttons to display in MDI windows.
  *
  * Value type for PrefsCollectionUnix::WorkspaceButtonType.
  */
enum MdiButtonType
{
	MDI_BUTTON_CLOSE    = 0x01,
	MDI_BUTTON_MINIMIZE = 0x02,
	MDI_BUTTON_MAXIMIZE = 0x04,
	MDI_BUTTON_RESTORE  = 0x08,
	MDI_BUTTON_ALL      = 0xFF
};
#endif // PREFS_HAVE_WORKSPACE

/** Where to display (JavaScript) popup windows.
  *
  * Value type for PrefsCollectionJS::TargetDestination.
  */
enum PopupDestination
{
	POPUP_WIN_NEW = 0,
	POPUP_WIN_CURRENT = 1,
	POPUP_WIN_BACKGROUND = 2,
	POPUP_WIN_IGNORE = 3
};

/** Display images or not.
  *
  * Value type for PrefsCollectionDoc::ShowImageState. Also used as data type
  * in the core code.
  */
typedef enum {
	FIGS_OFF = 1,
	FIGS_SHOW,
	FIGS_ON
} SHOWIMAGESTATE;

/** Check modification time for cache.
  *
  * Value type for PrefsCollectionNetwork::CheckFigModification,
  * PrefsCollectionNetwork::CheckDocModification, and
  * PrefsCollectionNetwork::CheckOtherModification.
  */
enum CHMOD
{
	NEVER,
	ALWAYS,
	TIME
};

/** What to do with HTTP errors.
  *
  * Value type for PrefsCollectionNetwork::HttpErrorStrategy.
  */
enum HTTPERRORSTRATEGY
{
	HTTP_ERROR_ALWAYS_SHOW,
	HTTP_ERROR_NEVER_SHOW,
	HTTP_ERROR_SHOW_CUSTOM
};

/** Check expiry type.
  *
  * Value type for PrefsCollectionNetwork::CheckExpiryHistory and
  * PrefsCollectionNetwork::CheckExpiryLoad.
  */
enum CheckExpiryType
{
	CHECK_EXPIRY_NORMAL = 0,
	CHECK_EXPIRY_USER = 1,
	CHECK_EXPIRY_NEVER = 2
};

/** Flags for Trusted protocol and source viewer usage. */
enum ViewerMode
{
	UseDefaultApplication = 1,	///< Use default (system-defined) application.
	UseCustomApplication = 2,	///< Use custom (user-specified) application.
	UseInternalApplication = 3,	///< Handle protocol internally.
	UseWebService = 4			///< Use a web service to handle this protocol.
};

struct TrustedProtocolData
{
	enum Flags
	{
		TP_Protocol    = 0x01,
		TP_Filename    = 0x02,
		TP_WebHandler  = 0x04,
		TP_Description = 0x08,
		TP_ViewerMode  = 0x10,
		TP_InTerminal  = 0x20,
		TP_UserDefined = 0x40,
		TP_All         = 0x7F // Expand as required
	};

	TrustedProtocolData():flags(0) {}

	INT32 flags;

	OpStringC protocol;
	OpStringC filename;
	OpStringC web_handler;
	OpStringC description;
	ViewerMode viewer_mode;
	BOOL in_terminal;
	BOOL user_defined;
};


#endif // PREFSTYPES_H
