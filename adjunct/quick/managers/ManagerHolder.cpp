/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/quick/managers/DesktopManager.h"

// include managers here
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/DesktopCookieManager.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/FeatureManager.h"
#include "adjunct/quick/managers/MailManager.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "adjunct/quick/managers/MouseGestureUI.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/managers/SecurityUIManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/quick/managers/DownloadManagerManager.h"
#include "adjunct/quick/managers/ToolbarManager.h"
#include "adjunct/quick/managers/TipsManager.h"
#ifdef PLUGIN_AUTO_INSTALL
#include "adjunct/quick/managers/PluginInstallManager.h"
#endif // PLUGIN_AUTO_INSTALL
#include "adjunct/quick/managers/RedirectionManager.h"

// This is where the manager type constants are mapped to C++ types.
//
// List all the managers here in any order.  The order and dependencies
// are defined in the ManagerType enum, and are handled later on in
// InitManagers(INT32).
ManagerHolder::ManagerDescr ManagerHolder::s_manager_descrs[] = {
#ifdef AUTO_UPDATE_SUPPORT
	CreateManagerDescr<AutoUpdateManager>(TYPE_AUTO_UPDATE, "Autoupdate"),
#endif
	CreateManagerDescr<DesktopCookieManager>(TYPE_COOKIE, "Cookie"),
	CreateManagerDescr<DesktopBookmarkManager>(TYPE_BOOKMARK, "Bookmarks"),
#ifdef GADGET_SUPPORT
	CreateManagerDescr<DesktopGadgetManager>(TYPE_GADGET, "Gadgets"),
#endif
	CreateManagerDescr<DesktopHistoryModel>(TYPE_HISTORY, "History"),
	CreateManagerDescr<DesktopSecurityManager>(TYPE_SECURITY, "Security"),
	CreateManagerDescr<DesktopTransferManager>(TYPE_TRANSFER, "Transfers"),
	CreateManagerDescr<FavIconManager>(TYPE_FAV_ICON, "Favicons"),
	CreateManagerDescr<FeatureManager>(TYPE_FEATURE, "Feature"),
	CreateManagerDescr<HotlistManager>(TYPE_HOTLIST, "Hotlist"),
#ifdef M2_SUPPORT
	CreateManagerDescr<MailManager>(TYPE_MAIL, "Mail"),
#endif
#ifdef OPERA_CONSOLE
	CreateManagerDescr<MessageConsoleManager>(TYPE_CONSOLE, "Console"),
#endif // OPERA_CONSOLE
	CreateManagerDescr<NotificationManager>(TYPE_NOTIFICATION, "Notifications"),
	CreateManagerDescr<OperaAccountManager>(TYPE_OPERA_ACCOUNT, "Account"),
#ifdef WEB_TURBO_MODE
	CreateManagerDescr<OperaTurboManager>(TYPE_OPERA_TURBO, "Turbo"),
#endif
#ifdef VEGA_OPPAINTER_SUPPORT
	CreateManagerDescr<QuickAnimationManager>(TYPE_QUICK_ANIMATION, "Animation"),
	CreateManagerDescr<MouseGestureUI>(TYPE_MOUSE_GESTURE_UI, "Mouse Gesture UI"),
#endif
	CreateManagerDescr<SecurityUIManager>(TYPE_SECURITY_UI, "Security UI"),
#ifdef SUPPORT_SPEED_DIAL
	CreateManagerDescr<SpeedDialManager>(TYPE_SPEED_DIAL, "Speed Dial"),
#endif
#ifdef SUPPORT_DATA_SYNC
	CreateManagerDescr<SyncManager>(TYPE_SYNC, "Sync"),
#endif
	CreateManagerDescr<WebmailManager>(TYPE_WEBMAIL, "Webmail"),
#ifdef WEBSERVER_SUPPORT
	CreateManagerDescr<WebServerManager>(TYPE_WEB_SERVER, "Webserver"),
#endif
	CreateManagerDescr<SessionAutoSaveManager>(TYPE_SESSION_AUTO_SAVE, "Session"),
	CreateManagerDescr<DownloadManagerManager>(TYPE_EXTERNAL_DOWNLOAD_MANAGER, "External download"),
	CreateManagerDescr<ToolbarManager>(TYPE_TOOLBAR, "Toolbar manager"),
	CreateManagerDescr<DesktopExtensionsManager>(TYPE_EXTENSIONS, "Extensions"),
	CreateManagerDescr<TipsManager>(TYPE_TIPS, "Tips Manager"),
#ifdef PLUGIN_AUTO_INSTALL
	CreateManagerDescr<PluginInstallManager>(TYPE_PLUGIN_INSTALL, "Plugin install"),
#endif
	CreateManagerDescr<RedirectionManager>(TYPE_REDIRECTION, "Redirection"),
	CreateManagerDescr<DesktopClipboardManager>(TYPE_CLIPBOARD, "Clipboard")
};


int ManagerHolder::ManagerDescrLessThan(const void* lhs, const void* rhs)
{
	return reinterpret_cast<const ManagerDescr*>(lhs)->type
			- reinterpret_cast<const ManagerDescr*>(rhs)->type;
}

ManagerHolder::~ManagerHolder()
{
	if (m_destroy_managers)
	{
		OP_PROFILE_METHOD("Destructed managers");
		// Destroy in reverse order of when they were created
		for (int i = ARRAY_SIZE(s_manager_descrs) - 1; i >= 0; --i)
			(*s_manager_descrs[i].destroyer)();
	}


	{
		// Commit all pref changed made by the managers on destruction
		OP_PROFILE_METHOD("Committed prefs");
		TRAPD(err, g_prefsManager->CommitL());
	}
}


OP_STATUS ManagerHolder::InitManagers(INT32 type_mask)
{
	op_qsort(s_manager_descrs, ARRAY_SIZE(s_manager_descrs),
			sizeof(ManagerDescr), ManagerDescrLessThan);

	for (size_t i = 0; i < ARRAY_SIZE(s_manager_descrs); ++i)
	{
		const ManagerDescr& descr = s_manager_descrs[i];

		if (descr.type == (type_mask & descr.type))
		{
			OP_PROFILE_METHOD(descr.name);

			GenericDesktopManager* manager = (*descr.creator)();
			RETURN_OOM_IF_NULL(manager);
			m_destroy_managers = true;

			const OP_STATUS init_status = manager->Init();
			OP_ASSERT(OpStatus::IsSuccess(init_status));
			if (OpStatus::IsError(init_status))
			{
				char buf[256];
				op_snprintf(buf, 256, "'%s' failed init: %d", descr.name, init_status);
#ifdef MSWIN
				MessageBoxA(NULL, buf, "Opera", MB_OK | MB_ICONERROR);
#else
				fputs("opera: ", stderr);
				fputs(buf, stderr);
				fputs("\n", stderr);
#endif
				OP_PROFILE_MSG(buf);
			}
			RETURN_IF_ERROR(init_status);
		}
	}

	return OpStatus::OK;
}
