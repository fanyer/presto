/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file prefs-capabilities.h
 *
 * Defines the capabilites available in this version of the prefs module.
 */

#ifndef PREFS_CAPABILITIES_H
#define PREFS_CAPABILITIES_H

// Major API versions.
#define PREFS_CAP_3				///< This is the version 3 PrefsManager. 2004-02 on Smurf 1.

// Features.
#define PREFS_CAP_DOWNLOAD				///< PrefsLoadManager support is available. 2004-06 on Smurf 1.
#define PREFS_CAP_GET_DEF_FROM_LANG		///< Can use locale strings as defaults. 2004-08 on Barbabapa 4.
#define PREFS_CAP_OPERACONFIG			///< Supports generating opera:config. 2005-08 on Smurf 1.

// Definitions for the locations and availability of collections. "I"
// means the collection is defined inside the prefs module, "E" means that
// it is defined elsewhere.
#define PREFS_CAP_3_I_APP		///< PrefsCollectionApp in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_CORE		///< PrefsCollectionCore in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_E_COREGOGI	///< PrefsCollectionCoregogi in platforms/core. 2006-12 on Smurf 1.
#define PREFS_CAP_3_E_DATABASE	///< PrefsCollectionDatabase in modules/database. 2009-05
#define PREFS_CAP_3_I_DISPLAY	///< PrefsCollectionDisplay in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_DOC		///< PrefsCollectionDoc in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_EPOC		///< PrefsCollectionEPOC in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_FILES		///< PrefsCollectionFiles in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_FONTCOLOR	///< PrefsCollectionFontsAndColors in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_JAVA		///< PrefsCollectionJava in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_JS		///< PrefsCollectionJS in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_E_M2		///< PrefsCollectionM2 in desktop_util module.
#define PREFS_CAP_3_E_MACOS		///< PrefsCollectionMacOS in mac module.
#define PREFS_CAP_3_E_MSWIN		///< PrefsCollectionMSWIN in windows module.
#define PREFS_CAP_3_E_NAT		///< PrefsCollectionNatTraversal in nat_traversal module. 2006-12 on Smurf 1.
#define PREFS_CAP_3_I_NETWORK	///< PrefsCollectionNetwork in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_I_PRINT		///< PrefsCollectionPrint in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_E_QNX		///< PrefsCollectionQNX in photon module.
#define PREFS_CAP_3_I_SYNC		///< PrefsCollectionSync in prefs module. 2006-12 on Smurf 1.
#define PREFS_CAP_3_I_TOOLS		///< PrefsCollectionTools in prefs module. 2006-08 on Smurf 1.
#define PREFS_CAP_3_E_UI		///< PrefsCollectionUI in desktop_util module.
#define PREFS_CAP_3_I_UNIX		///< PrefsCollectionUnix in prefs module. 2004-03 on Smurf 1.
#define PREFS_CAP_3_E_VOICE		///< PrefsCollectionVoice in voice module.
#define PREFS_CAP_3_I_WEBSERVER	///< PrefsCollectionWebserver in prefs module. 2005-01 on Smurf 1.

// Minor API changes.
#define PREFS_CAP_LISTEN_OVERRIDE		///< HostOverrideChanged API available. 2004-05 on Smurf 1.
#define PREFS_CAP_SERVERNAME			///< Support ServerName pointers for host overrides. 2004-05 on Smurf 1.
#define PREFS_CAP_DIR_IN_PCFILES		///< Use PrefsCollectionFiles::WriteDirectoryL(). 2005-06 on Smurf 1.
#define PREFS_CAP_DEFAULT_OVERRIDES		///< Separate downloaded overrides from user-entered. 2005-12 on Smurf 1.
#define PREFS_CAP_DL_OVERRIDE_STATUS	///< HostOverrideStatus has status values for downloaded overrides. 2006-01 on Smurf 1.
#define PREFS_CAP_NOTIFIER				///< PrefsNotifier object available. 2006-10 on Smurf 1.
#define PREFS_CAP_NOTIFIER_STATIC		///< PrefsNotifier only has static methods. 2007-03 on Smurf 1.
#define PREFS_CAP_APPLY_RAM_CACHE		///< PrefsCollectionCore::ApplyRamCacheSettings() exists. 2009-06 on Smurf 1.

// Preferences availability.
// Versioned.
#define PREFS_CAP_RENDERING_MODES 8		///< ERA preferences available. 2004-01 on Barbapapa 3.
#define PREFS_CAP_META_REFRESH 1		///< Meta refresh settings available. 2004-03 on Barbapapa 3.
#define PREFS_CAP_USER_JS 2				///< User Javascript settings available. 2004-04 on Smurf 1.
#define PREFS_CAP_TLS_1_1 1				///< TLS 1.1 toggle is available. 2004-08 on Barbapapa 3.
#define PREFS_CAP_DEVTOOLS 2			///< Developer tools prefs API ver. 2 available. 2008-02 on Smurf 1.
#define PREFS_CAP_CUSTOM_JVM 2			///< Custom JVM prefs API ver. 2 available
// Unversioned.
#define PREFS_CAP_LOCAL_CSS				///< Local CSS file support is available. 2004-04 on Smurf 1.
#define PREFS_CAP_BROWSERJSDOWNLAD		///< browser.js download settings available. 2005-05 on Barbapapa 4.
#define PREFS_CAP_CERTINFO_CSS			///< Certificate info CSS setting available. 2004-08 on Barbapapa 3.
#define PREFS_CAP_LICENSE_UNIX			///< Accept license settings for Unix. 2004-08 on Barbapapa 4.
#define PREFS_CAP_TOOLTIP_COLOR			///< Tooltip color preferences for Unix. 2004-08 on Barbapapa 4.
#define PREFS_CAP_TOOLTIP_COLOR_AS_COLOR///< Tooltip color preference as color preference. 2006-01 on Smurf 1.
#define PREFS_CAP_RESTOREDEXT			///< Restore extension preference for Unix. 2004-09 on Barbapapa 4.
#define PREFS_CAP_SMILEY				///< Use smiley image preference available. 2004-08 on Barbapapa 4.
#define PREFS_CAP_JSHIDEURL				///< Setting for JS allowed to hide URL field in popups. 2004-09 on Barbapapa 4.
#define PREFS_CAP_SCRIPTSPOOF			///< Special User Agent String used only by scripts. 2004-09 on Smurf 1.
#define PREFS_CAP_JS_DEBUGGER			///< Setting for JS debugger. 2004-10 on Smurf 1.
#define PREFS_CAP_JS_REMOTE_DEBUGGER	///< Settings for remote JS debugger. 2004-10 on Smurf 1.
#define PREFS_CAP_USE_OCSP				///< Setting for online cert validation. 2005-02 on Barbapapa 4.
#define PREFS_CAP_IDNA_WHITELIST		///< Setting for TLDs allowed to use IDNA. 2005-02 on Barbapapa 4.
#define PREFS_CAP_ES_ERRORLOG			///< Setting for filename to which error msgs may be logged. 2005-03 on Barbapapa 4.
#define PREFS_CAP_XML_PREFS				///< Settings for internal XML parser. 2005-03 on Smurf 1.
#define PREFS_CAP_HELP_URL				///< Help location is URL, not file path. 2005-03 on Smurf 1.
#define PREFS_CAP_MAILBODYFONT			///< Setting for e-mail body font. 2005-01 on Barbapapa 4.
#define PREFS_CAP_WEBSERVER				///< Opera web server preferences available. 2005-01 on Barbapapa 4.
#define PREFS_CAP_NAT_TRAVERSAL			///< Settings for NAT traversal available. 2006-11 on Smurf 1.
#define PREFS_CAP_DIALOGCONFIG			///< Location of dialog.ini is configurable. 2004-03 on Barbapapa 3.
#define PREFS_CAP_DELAYEDSCRIPTS		///< Delayed Script Execution settings available. 2004-11 on Barbapapa 4.
#define PREFS_CAP_MAXCONSOLEMSG			///< Max console messages setting available. 2005-08 on Smurf 1.
#define PREFS_CAP_MAXVERSIONRUN			///< Newest run version setting available. 2005-09 on Smurf 1.
#define PREFS_CAP_CONSOLE_LOG			///< Console log setting available. 2005-10 on Smurf 1.
#define PREFS_CAP_PERMITPORTS			///< Permitted ports setting available. 2005-11 on Smurf 1.
#define PREFS_CAP_HISTORYNAVIGATIONMODE	///< History Navigation Mode setting available. 2005-11 on Smurf 1.
#define PREFS_CAP_CANVAS				///< Canvas setting available. 2005-12 on Smurf 1.
#define PREFS_CAP_MARQUEE				///< Max marquee loops setting available. 2005-12 on Smurf 1.
#define PREFS_CAP_STYLEIMAGEFILE		///< External stylesheet for generated image page. 2006-01 on Smurf 1.
#define PREFS_CAP_SECURITYLEVEL			///< Minimum security level setting available. 2006-01 on Smurf 1.
#define PREFS_CAP_COLORSCHEME_COLOR		///< Skin color as color setting. 2006-02 on Smurf 1.
#define PREFS_CAP_USER_JS_HTTPS			///< User Javascript on HTTPS setting available. 2006-04 on Smurf 1.
#define PREFS_CAP_VIEWERMODE			///< ViewerMode enumeration available. 2006-04 on Smurf 1.
#define PREFS_CAP_SAVESUBFOLDER			///< Save with images use subfolder setting available. 2006-05 on Smurf 1.
#define PREFS_CAP_HORIZONTAL_SCROLLSTEP	///< Setting for horizontal scroll amount. 2006-05 on Barbapapa 4.
#define PREFS_CAP_LIMIT_PARAGRAPH_WIDTH ///< Limit paragraph width AKA msr-light. 2006-05 on Smurf 1.
#define PREFS_CAP_SETUPMANAGER_FILES	///< Support OpSetupManager files as file preferences. 2006-05 on Smurf 1.
#define PREFS_CAP_LOADDELAYEDTIMEOUT	///< Loading delayed timeout setting available. 2006-06 on Smurf 1.
#define PREFS_CAP_MESSAGECSS			///< Message style sheet setting available. 2006-07 on Smurf 1.
#define PREFS_CAP_SEARCHCSS				///< History search style sheet setting available. 2006-08 on Smurf 1.
#define PREFS_CAP_MARKCLOSEITEM			///< Mark Close Item setting available. 2006-08 on Smurf 1.
#define PREFS_CAP_SELECTCOLORS			///< Selection color settings available. 2006-09 on Smurf 1.
#define PREFS_CAP_HIGHLIGHT_COLORS		///< Highlight color settings available. 2006-09 on Smurf 1.
#define PREFS_CAP_WARNINGCSS			///< Warning style sheet setting available. 2006-10 on Smurf 1.
#define PREFS_CAP_FLEX_ROOT				///< Flex-root setting available. 2006-10 on Smurf 1.
#define PREFS_CAP_FLEX_ROOT_MIN_WIDTH	///< Flex-root min-width setting available. 2007-02 on Smurf 1.
#define PREFS_CAP_ENABLE_CONTENT_BLOCKER///< Enable content blocker setting available. 2007-04 on Smurf 1.
#define PREFS_CAP_SYNC_LOGGING			///< Option to enable logging of all sync requests. 2007-08 on Smurf 1.
#define PREFS_CAP_MATHML_CSSFILE		///< Local css file to style MathML elements.
#define PREFS_CAP_YIELD_REFLOW_PARAMS	///< Parameters to control reflow yielding
#define PREFS_CAP_HAVE_DECODEALL		///< Split up the TurboMode preference in two, this handles the case where images are decoded until the cache is full.
#define PREFS_CAP_URL_HOSTOVERIDE		///< URL can be used for HostOverrides
#define PREFS_CAP_LEFT_HANDED_UI		///< Left-handed UI and RTL Flips UI settings available.
#define PREFS_CAP_BOOKMARK_CSSFILE		///< Bookmark style sheet setting.
#define PREFS_CAP_PLUGINSCRIPTACCESS_PCAPP	///< Setting for plugin's script access moved to pcapp
#define PREFS_CAP_SPELLCHECK			///< Spellchecking preferences
#define PREFS_CAP_SPELLCHECK_ENABLED_BY_DEFAULT ///< Spellchecking is enabled by default
#define PREFS_CAP_ENCODING_DRIVEN_LINE_HEIGHT ///< default line height for different encodings
#define PREFS_CAP_EXCEPTIONS_HAVE_STACKTRACE ///< Exceptions have stacktrace setting available
#define PREFS_CAP_DEVTOOLS_URL			///< PrefsCollectionTools::DevToolsUrl
#define PREFS_CAP_ENABLE_PIPELINING	///< Setting for network's pipelining
#define PREFS_CAP_STRICT_EV_MODE		///< PrefsCollectionNetwork::StrictEVMode
#define PREFS_CAP_STYLE_SPEEEDDIAL	///< PrefsCollectionFiles::StyleSpeedDialFile
#define PREFS_CAP_SYNC_DATA_PROVIDER ///< PrefsCollectionSync::SyncDataProvider
#define PREFS_CAP_HAS_OBML_PREFS ///< PrefsCollectionDisplay::OBML_ImageQuality
#define PREFS_CAP_HAS_UAPROFURLENABLED	///< Preference UaProfUrlEnabled is available
#define PREFS_CAP_ADAPTIVE_ZOOM			///< Adaptive Zoom settings available. 2008-01 on Smurf 1.
#define PREFS_CAP_ENABLE_MOUSE_FLIPS	///< PrefsCollectionCore::EnableMouseFlips
#define PREFS_CAP_SHOW_MSIMG32_WARNING	///< PrefsCollectionMSWIN::ShowWarningMSIMG32
#define PREFS_CAP_OPERA_DEBUG_STYLE		///< PrefsCollectionFiles::StyleDebugFile
#define PREFS_CAP_INCREMENTAL_OBML      ///< PrefsCollectionDisplay::OBML_incremental_rendering exists
#define PREFS_CAP_TEMPLATE_WEBFEEDS_DISPLAY ///<PrefsCollectionFiles::TemplateWebfeedsDisplay
#define PREFS_CAP_HAS_SYNCLASTUSED_PREFS	///< PrefsCollectionSync::SyncLastUsed
#define PREFS_CAP_DEFAULTSEARCHTYPE_STRINGS ///<PrefsCollectionUI::DefaultSpeeddialSearchType and PrefsCollectionCore::DefaultSearchType is string prefs
#define PREFS_CAP_TLS1_2				///< PrefsCollectionNetwork::EnableTLS1_2 exists
#define PREFS_CAP_TYPED_HISTORY			///< Direct History has changed its file name
#define PREFS_CAP_OBML_MINIMUM_FONT_SIZE ///< PrefsCollectionDisplay::OBML_MinimumFontSize exists
#define PREFS_CAP_UNIX_PACKAGE_PROPS	///< PrefsCollectionUnix::PackageType and PrefsCollectionUnix::PackageQtLinkage
#define PREFS_CAP_MULTISTYLE_AWARE      ///< using SetPreferredFontForScript instead of ...Codepage if present
#define PREFS_CAP_ALLOW_SCRIPT_HISTORY	///< PrefsCollectionDoc::AllowScriptToNavigateInHistory is available
#define PREFS_CAP_ALWAYS_RELOAD_INTERRUPTED_IMAGES	///< PrefsCollectionDoc::AlwaysReloadInterruptedImages is available
#define PREFS_CAP_ALLOW_CROSS_DOMAIN_ACCESS ///< PrefsCollectionNetwork::AllowCrossDomainAccess is available
#define PREFS_CAP_OBML_TRANSCODER_COOKIE	///< Has PrefsCollectionDisplay::OBML_TranscoderCookie
#define PREFS_CAP_ON_DEMAND_PLUGIN ///< PrefsCollectionDisplay::EnableOnDemandPlugin and PrefsCollectionDisplay::EnableOnDemandPluginPlaceholder
#define PREFS_CAP_DICTIONARY_TIME		///< Has state information available for autoupdate;Holds the time of the last downloaded dictionaries.xml
#define PREFS_CAP_TURBO_MODE_NOTIFIY            ///< Turbo preference for showing notification when slow/fast network is detected (ShowNetworkSpeedNotification)
#define PREFS_CAP_TURBO_MODE_ENABLE             ///< Turbo mode can be enabled/disabled using OperaTurboMode option.
#define PREFS_CAP_UI_DEBUG_SKIN         ///< Has the DebugSkin prefs setting
#define PREFS_CAP_CRYPTO_METHOD_OVERRIDE ///< PrefsCollectionNetwork::CryptoMethodOverride
#define PREFS_CAP_USE_WEB_TURBO          ///< PrefsCollectionNetwork::UseWebTurbo
#define PREFS_CAP_TURBO_SETTINGS_FILE	 ///< PrefsCollectionFiles::WebTurboConfigFile
#define PREFS_CAP_UPNP_ENABLED           ///< Has PrefsCollectionWebserver::UPnPEnabled
#define PREFS_CAP_UPNP_DISCOVERY_ENABLED ///< Has PrefsCollectionWebserver::UPnPServiceDiscoveryEnabled
#define PREFS_CAP_ROBOTS_TXT_ENABLED        ///< Has PrefsCollectionWebserver::RobotsTxtEnabled
#define PREFS_CAP_SERVICE_DISCOVERY_ENABLED ///< Has PrefsCollectionWebserver::ServiceDiscoveryEnabled
#define PREFS_CAP_ACCOUNT_SAVE_PASSWORD	    ///< Has PrefsCollectionOperaAccount::SavePassword
#define PREFS_CAP_FULL_PATH_DEFAULT_DIR     ///< GetDefaultDirectoryPref can return a full path instead of name and parent
#define PREFS_CAP_JAVA_FILES_USE_OPFILE ///< Java file prefs that were expressed as strings are now expressed as files
#endif // PREFS_CAPABILITIES_H
