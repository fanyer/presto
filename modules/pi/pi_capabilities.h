/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file pi_capabilities.h
 *
 * This file contains the capabilities exported by the pi module.
 *
 * @author rikard@opera.com
*/

#ifndef PI_CAPABILITIES_H
#define PI_CAPABILITIES_H

/* this capability is defined to determine if the new angstrom_2 features are available */
#define PI_CAP_ANGSTROM_2

/* OpLocale and OpFactory::CreateOpLocale() exist */
#define PI_CAP_OPLOCALE

/* OpSystemInfo::ExecuteApplication() exists */
#define PI_CAP_EXECUTE_APPLICTION

/* GetFullPath() and GetFileLength() exist in OpFolderLister */
#define PI_CAP_EXTENDED_FOLDER_LISTER

/* OpLowLevelFile::IsWritable() exists */
#define PI_CAP_FILE_WRITABLE

/* OPFILE_APPEND and OPFILE_WRITE_UPDATE exist in OpFileOpenMode */
#define PI_CAP_FILE_OPEN_MODES

/* GetTimeUTC exists */
#define PI_CAP_UTC_TIME

/* OpRect has a constructor that takes a RECT* */
#define PI_CAP_RECT_OPRECT_CONV

/* OpLowLevelFile::GetFileInfo() exists */
#define PI_CAP_FILE_INFO

/* OpFileChooser and DragManager listeners report memory failure */
#define PI_CAP_FILEDRAG_MEMSAFE

/* OpLowLevelFile::Flush() exists */
#define PI_CAP_FLUSH

/* DRAG_SUPPORT-defines surrounding OpDragListener and OpView::SetDragListener(). */
#define PI_CAP_DRAG_DEFINE

/* OpView has OnHighlightRectChanged. */
#define PI_CAP_VIEW_HAS_HIGHLIGHTRECT_CHANGED

/* OpLowLevelFile reports more errors */
#define PI_CAP_MORE_FILE_STATUS

/* OpSystemInfo::GetSystemIp() exists. */
#define PI_CAP_SYSTEM_IP

/* OpLowLevelFile::SetFileLength() exists. */
#define PI_CAP_SET_FILE_LENGTH

/* OpLowLevelFile::SetFilePos() takes a seek_mode parameter */
#define PI_CAP_SET_FILE_POS_EXTENDED

/* Ability to update send progress as the data are sent to the socket */
#define PI_CAP_SOCKET_SENDPROGRESS

/* OpFileChooser uses OpWindow  pointer */
#define PI_CAP_CHOOSER_OPWINDOW

/* OpSystemInfo has GetOSStr() and GetPlatformStr() */
#define PI_CAP_GETOSSTR

/* OpFontManager::UpdateGlyphMask() exists. */
#define PI_CAP_HAS_UPDATEGLYPHMASK

/* Network interfaces changed and moved to network/. */
#define PI_CAP_NETWORK

/* OpFileLengthToString exists in OpSystemInfo */
#define PI_CAP_OPFILELENGTH_CONVERTERS

/* OpFileChooserListener::OnCancelled() exists. */
#define PI_CAP_FILECHOOSER_CANCEL

/* double OpSystemInfo::DaylightSavingsTimeAdjustmentMS(double t) exists */
#define PI_CAP_DST_ADJUSTMENT

/* GetRuntimeMS exists */
#define PI_CAP_RUNTIMEMS

/* OpSystemInfo has GetDriveLetters() and GetCurrentDriveLetter() */
#define PI_CAP_DRIVELETTERS

/* factories are replaced with static create methods in the interfaces themselves */
#define PI_CAP_NO_FACTORIES

/* OpSystemInfo has GetDnsSuffixes */
#define PI_CAP_DNS_SUFFIXES

/* OpBitmap has SetOpacity */
#define PI_CAP_BITMAP_HAS_SETOPACITY

/* OpPainter has DrawBitmapClippedOpacity */
#define PI_CAP_PAINTER_BITMAPOPACITY

/* Has outlines-flag in OpFontManager::CreateFont */
#define PI_CAP_HAS_CREATEFONT_WITH_OUTLINES

/* OpFont::GetOutline in a new shape */
#define PI_CAP_OPFONT_HAS_NEW_GETOUTLINE

/* OpDragManager supports dragging of several URLs (AddURL + GetURLs exist) */
#define PI_CAP_DRAG_SEVERAL_URLS

/* OpPainter has BeginOpacity and EndOpacity */
#define PI_CAP_PAINTER_HAS_OPACITY

/* OpSystemInfo has GetUserLanguage */
#define PI_CAP_USER_LANGUAGE

/* OpSystemInfo has GetUserCountry */
#define PI_CAP_USER_COUNTRY

/* OpPainter has a new improved DrawBitmapTiled */
#define PI_CAP_DRAW_BITMAP_TILED_EXTENDED

/* OP_SYSTEM_COLOR has tooltip colors */
#define PI_CAP_TOOLTIP_COLORS

/* OpWindow has GetRestoreState() and SetRestoreState() */
#define PI_CAP_HAS_GETSET_RESTORESTATE

/* OpTaskBar has SetUnattendedChatCount */
#define PI_CAP_OPTASKBAR_HAS_CHATCOUNT

/* OpSystemInfo has GetFileTypeName */
#define PI_CAP_OPSYSTEMINFO_GETFILETYPENAME

/* OpSystemInfo::GetFileHandler has contentType */
#define PI_CAP_OPSYSTEMINFO_GETFILEHANDLER_CONTENTTYPE

/* OpWindow has SetFloating, SetAlwaysBelow and SetShowInWindowList */
#define PI_CAP_OPWINDOW_BELOW_FLOATING

/* OpLocale has NumberFormat methods for double, int and OpFileLength */
#define PI_CAP_LOCALE_NUMBER_FORMAT

/* OpLowLevelFile has NOT_REGULAR file mode flag */
#define PI_CAP_HAS_NOT_REGULAR_FILEMODE

/* OP_SYSTEM_COLOR has colors for highlighted text (ie. incremental search results) */
#define PI_CAP_HIGHLIGHT_COLORS

/* OpSystemInfo has GetAutoProxyURLL and GetProxyExceptionsL */
#define PI_CAP_EXTENDED_PROXY_INFO

/* OpPluginWindow supports OpPluginWindowListener */
#define PI_CAP_PLUGIN_LISTENER

/* OpInputMethodString has properties for caret & underline. */
#define PI_CAP_IMESTRING_PROPS

/* Support for generating a GUID (Globally Unique Identifier). */
#define PI_CAP_GUID

/* OpPluginWindow::Construct() has a 'transparent' parameter. */
#define PI_CAP_PLUGIN_TRANSPARENT

/* OpUiInfo has API to check if mouse is right-handed. */
#define PI_CAP_MOUSE_RIGHTHANDED

/* The class OpThread / OpSynchronization object is removed. */
#define PI_CAP_SYNCOBJECT_REMOVED

/* OpFontManager has method GetFamily() if HAS_FONT_FOUNDRY is defined. */
#define PI_CAP_OPFONT_GETFAMILY

/* OpLowLevelFile::Eof() behaves like C89 feof(), instead of just
   checking if file position is at the last byte. */
#define PI_CAP_C89_EOF

/* OpSystemInfo::IsFullKeyboardAccessActive() exists */
#define PI_CAP_FULL_KEYBOARD_ACCESS

/* The IS_OP_KEY macros has moved to the hardcore module (and is in
   part generated by the keys system.) */
#define PI_CAP_NO_IS_OP_KEY_MACROS

/* OpLocale::Use24HourClock() exists */
#define PI_CAP_LOCALE_USE_24HOUR_CLOCK

/* OpLowLevelFile has GetLocalizedPath() method. */
#define PI_CAP_GET_LOCALIZED_PATH

/* OpSystemInfo::GetDnsAddress() takes an additional argument that
   specifies which DNS server to get the address for. */
#define PI_CAP_MULTIPLE_DNS_ADDR

/* OpSocketListener::OnSocketDataSendProgressUpdate() is no longer pure virtual.
   The method will no longer exist when URL_CAP_TRUST_ONSOCKETDATASENT is announced. */
#define PI_CAP_SOCKET_ONE_SENT_CALLBACK

/* The pi module is ready to remove the 'secure' parameter (that used to exist
   in a bunch of OpSocket methods) from all methods but Create(). However,
   URL_CAP_OPSOCKET_SECURE_CREATE_ONLY and SCOPE_CAP_OPSOCKET_SECURE_CREATE_ONLY
   need to be announced before this can take place. */
#define PI_CAP_OPSOCKET_SECURE_CREATE_ONLY

/* OpSocketListener::OnSocketListenError() exists, and a new OpSocket::Listen()
   method that takes two arguments (address and queue size) has been
   added. Bind(), SetLocalPort(), and Listen(UINT) are deprecated. */
#define PI_CAP_SOCKET_LISTEN_CLEANUP

/* OpLocale has CompareStringsL(). CompareStrings() is deprecated. */
#define PI_CAP_COMPARESTRINGSL

/* OpLocale::CompareStringsL() has a parameter that specifies whether or not to
   do case-sensitive comparison. */
#define PI_CAP_COMPARESTRINGSL_CASE

/* OpTaskbar::Init() is gone. */
#define PI_CAP_TASKBAR_NO_INIT_METHOD

/* DrawRect() and DrawEllipse() in OpPainter have a parameter that
   specifies the outline width. OpPainter::DrawPolygon() exists. */
#define PI_CAP_PAINTER_OUTLINE_IMPROVED

/* OpBitmap::InitStatus() is non-virtual and deprecated. */
#define PI_CAP_OPBITMAP_NO_INITSTATUS

/* OpWindow::Init() has an 'effect' parameter. */
#define PI_CAP_OPWINDOW_EFFECT

/* OpSocketAddress::FromString() returns OP_STATUS. */
#define PI_CAP_SOCKADDR_FROMSTRING_RET

/* The OpPainter::SUPPORTS enum has SUPPORTS_PLATFORM. */
#define PI_CAP_PLATFORM_PAINTER

/* Asynchronous methods in OpLowLevelFile return OP_STATUS instead of void. */
#define PI_CAP_ASYNC_FILE_OP_OPSTATUS

/* IM_WIDGETINFO struct has OpRect widget_rect */
#define PI_CAP_IM_WIDGETINFO_HAS_WIDGET_RECT

/* OpAsyncFontListener is available (when PLATFORM_FONTSWITCHING is defined). */
#define PI_CAP_ASYNC_FONTLISTENER

/* OpMainThread::Init() is gone. */
#define PI_CAP_MAINTHREAD_NO_INIT_METHOD

/* OpPaintListener::BeforePaint() returns FALSE if reflow yielded. */
#define PI_CAP_BEFOREPAINT_RETURNS

/* OP_SYSTEM_FONT_UI_ADDRESSFIELD, OP_SYSTEM_FONT_UI_TREEVIEW and
   OP_SYSTEM_FONT_UI_TREEVIEW_HEADER are available. */
#define PI_CAP_ADDITIONAL_UI_FONTS

/* OpSocketAddress::IsHostValid() is available. */
#define PI_CAP_ISHOSTVALID

/* OpBitmap declares GetPalette() and GetIndexedLineData() if
   FEATURE_INDEXED_OPBITMAP and IMG_CAP_GETINDEXED_DATA are enabled. */
#define PI_CAP_GETINDEXED_DATA

/* GetSocketAddress() and GetLocalSocketAddress() in OpSocket return
   OP_STATUS instead of void. */
#define PI_CAP_GETSOCKETADDR_OOM

/* Error codes OpSocket::OUT_OF_MEMORY and OpHostResolver::OUT_OF_MEMORY exist. */
#define PI_CAP_NETWORK_ASYNC_OOM

/* OpMainThread::PostMessage() has a 'delay' parameter. */
#define PI_CAP_MAINTHREAD_DELAY

/* OpLowLevelFile has MakeDirectory() */
#define PI_CAP_MAKEDIR

/* OpFileInfo (and OpFileInfoChange) has 'creation_time'. */
#define PI_CAP_FILE_CREATION_TIME

/* OpWindow has method SetIcon(OpBitmap*) */
#define PI_CAP_OPWINDOW_SETICON

/* OpSocketAddress::GetNetType() and its associated enum OpSocketAddressNetType exist. */
#define PI_CAP_OPSOCKETADDRESS_NETTYPE

/* GetIllegalFilenameCharacters() and RemoveIllegalPathCharacters() (used to be
   in OpSystemInfo) have been removed. */
#define PI_CAP_NO_ILLEGALFILECHARS_FUNC

/* enum OP_SYSTEM_COLOR includes OP_SYSTEM_COLOR_ITEM_TEXT and OP_SYSTEM_COLOR_ITEM_TEXT_SELECTED. */
#define PI_CAP_COLOR_ITEM_TEXT

/* OpTaskbar has been removed from the pi module. */
#define PI_CAP_NO_OPTASKBAR

/* OpTv has been removed from the pi module. */
#define PI_CAP_NO_OPTV

/* OpPainter::SUPPORTS_ALPHA_COLOR exists. */
#define PI_CAP_SUPPORTS_ALPHA_COLOR

/* OpWindowListener has the OnVisibilityChanged() method. */
#define PI_CAP_OPWINDOWLISTENER_ONVISIBILITYCHANGED

/* OpFont has webfont changes. */
#define PI_CAP_HAS_WEBFONTS_API

/* OpLowLevelFile methods return ERR_NO_DISK, ERR_NO_ACCESS,
   ERR_FILE_NOT_FOUND and ERR_NOT_SUPPORTED when appropriate. */
#define PI_CAP_FILE_ERRORS

/* OpSystemInfo has PathsEqual(). */
#define PI_CAP_PATHSEQUAL

/* OpUiInfo has GetCaretWidth(). */
#define PI_CAP_GET_CARET_WIDTH

/* Supports ASCII key strings for actions/menus/dialogs/skin */
#define PI_CAP_INI_KEYS_ASCII

/* OpPoint provides comparison, negation, addition and subtraction operators. */
#define PI_CAP_OPPOINT_OPERATORS

/* OpRect provides methods to return any of the rectangle's corners as an OpPoint. */
#define PI_CAP_OPRECT_CORNER

/* Has OpMediaPlayer. */
#define PI_CAP_HAS_MEDIA_PLAYER

/* Removed stray const-qualifier from OpLocale::op_strftime() return type. */
#define PI_CAP_STRFTIME_NONCONST_RETURN

/* OpLocale has ConvertToLocaleEncoding() and ConvertFromLocaleEncoding(). */
#define PI_CAP_CONVERT_LOCALE_ENCODING

/* Has OpWindowAnimationListener and related classes and OpWindow methods. */
#define PI_CAP_WINDOW_ANIMATION_LISTENER

/* OpPainter::DrawString() takes an optional parameter word_width */
#define PI_CAP_LINEAR_TEXT_SCALE

/* Can resolve external hostnames, such as entries from /etc/hosts when using the PiHostResolver */
#define PI_CAP_RESOLVE_EXTERNAL

/* OpFontManager has the new webfont API based on platform-issued OpWebFontRef */
#define PI_CAP_PLATFORM_WEBFONT_REF

/* OpFileInfo has 'hidden'. */
#define PI_CAP_FILE_HIDDEN

/* OpSystemInfo::ExpandSystemVariablesInString() takes an OpString as out parameter */
#define PI_CAP_EXPANDSYSVARS_OPSTRING

#endif // PI_CAPABILITIES_H
