/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file DesktopPrefsTypes.h
 * Types used in desktop preferences (PrefsCollectionDesktopUI & PrefsCollectionM2).
 *
 * Various typedef, enum and #define data types used by the preferences
 * code in desktop to store preferences.
 * The preferences code is quite agnostic as to what it stores,
 * these types are really defined here as a courtesy for the
 * caller and for historical purposes.
 */

#ifndef DESKTOPPREFSTYPES_H
# define DESKTOPPREFSTYPES_H

/** What to allow in drag'n'drop.
  *
  * Value type for PrefsCollectionUI::EnableDrag.
  */
enum DragEnableState
{
	DRAG_LINK       = 0x01,
	DRAG_BOOKMARK   = 0x02,
	DRAG_LOCATION   = 0x04,
	DRAG_SAMEWINDOW = 0x08,
	DRAG_ALL        = 0xFF
};

/** How to handle recovery after a crash.
  *
  * Value type for PrefsCollectionUI::WindowRecoveryStrategy.
  */
enum WindowRecoveryStrategy
{
	Restore_RegularStartup = 0, ///< No recovery needed (start with saved win.)
# ifdef AUTOSAVE_WINDOWS
	Restore_AutoSavedWindows,   ///< Start using autosaved window file (if any)
# endif // AUTOSAVE_WINDOWS
	Restore_Homepage,           ///< Start with homepage
	Restore_NoWindows           ///< Start with no windows
};


/** System's mail handler.
  *
  * Value type for PrefsCollectionUI::MailHandler.
  */
enum MAILHANDLER
{
	_MAILHANDLER_FIRST_ENUM	= 0,
	MAILHANDLER_OPERA		= 1,
	MAILHANDLER_APPLICATION	= 2,
	MAILHANDLER_SYSTEM		= 3,
	MAILHANDLER_WEBMAIL		= 4,
//	insert above
	IGNORE_MAILHANDLER_PARAM,
	_MAILHANDLER_LAST_ENUM
};

/** What to display on startup.
  *
  * Value type for PrefsCollectionUI::StartupType.
  */
enum STARTUPTYPE
{
	_STARTUP_FIRST_ENUM = -1,

	STARTUP_HISTORY,
	STARTUP_WINHOMES,	///< start with last saved session
	STARTUP_HOME,
	STARTUP_CONTINUE,	///< Continue where interrupted
	STARTUP_NOWIN,		///< Open with no windows
	STARTUP_ONCE,
	STARTUP_BLANK,

	_STARTUP_LAST_ENUM
};

/** Window cycle type.
  *
  * Value type for PrefsCollectionUI::WindowCycleType.
  */
enum WindowCycleType
{
	WINDOW_CYCLE_TYPE_STACK = 0,
	WINDOW_CYCLE_TYPE_LIST,
	WINDOW_CYCLE_TYPE_MDI
};

/*
 * How Opera should be exited.
 * Value type for:
 * - PrefsCollectionUI::ShowExitDialog
 * - PrefsCollectionUI::WarnAboutActiveTransfersOnExit
 * - PrefsCollectionUI::OperaUniteExitPolicy
 */
enum ConfirmExitStrategy
{
    _CONFIRM_EXIT_STRATEGY_FIRST_ENUM = 0,

    // don't change the order of the first two, they need to be compliant
    // with the old boolean 'DoNotShowDialog'
    ExitStrategyExit = _CONFIRM_EXIT_STRATEGY_FIRST_ENUM, // has to be 0!
    ExitStrategyConfirm = 1, // has to be 1!
    ExitStrategyHide,

    _CONFIRM_EXIT_STRATEGY_LAST_ENUM
};
#endif // DESKTOPPREFSTYPES_H