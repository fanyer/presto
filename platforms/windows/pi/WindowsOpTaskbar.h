/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPTASKBAR_H
#define WINDOWSOPTASKBAR_H

#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

#include "platforms/windows/utils/com_helpers.h"

class MailDesktopWindow;

class WindowsOpWindow;
struct ITaskbarList3;
struct ICustomDestinationList;
struct IObjectArray;

#define DWM_SIT_DISPLAYFRAME    0x00000001  // Display a window frame around the provided bitmap

// Window attributes
enum DWMWINDOWATTRIBUTE
{
	DWMWA_NCRENDERING_ENABLED = 1,      // [get] Is non-client rendering enabled/disabled
	DWMWA_NCRENDERING_POLICY,           // [set] Non-client rendering policy
	DWMWA_TRANSITIONS_FORCEDISABLED,    // [set] Potentially enable/forcibly disable transitions
	DWMWA_ALLOW_NCPAINT,                // [set] Allow contents rendered in the non-client area to be visible on the DWM-drawn frame.
	DWMWA_CAPTION_BUTTON_BOUNDS,        // [get] Bounds of the caption button area in window-relative space.
	DWMWA_NONCLIENT_RTL_LAYOUT,         // [set] Is non-client content RTL mirrored
	DWMWA_FORCE_ICONIC_REPRESENTATION,  // [set] Force this window to display iconic thumbnails.
	DWMWA_FLIP3D_POLICY,                // [set] Designates how Flip3D will treat the window.
	DWMWA_EXTENDED_FRAME_BOUNDS,        // [get] Gets the extended frame bounds rectangle in screen space
	DWMWA_HAS_ICONIC_BITMAP,            // [set] Indicates an available bitmap when there is no better thumbnail representation.
	DWMWA_DISALLOW_PEEK,                // [set] Don't invoke Peek on the window.
	DWMWA_LAST
};

// Non-client rendering policy attribute values
enum DWMNCRENDERINGPOLICY
{
	DWMNCRP_USEWINDOWSTYLE, // Enable/disable non-client rendering based on window style
	DWMNCRP_DISABLED,       // Disabled non-client rendering; window style is ignored
	DWMNCRP_ENABLED,        // Enabled non-client rendering; window style is ignored
	DWMNCRP_LAST
};

enum JumpListType
{
	JUMPLIST_TYPE_SPEEDDIAL,
	JUMPLIST_TYPE_FREQUENT,
	JUMPLIST_TYPE_TASKS
};

#define MAX_URL_JUMPLIST_ENTRIES	64

class JumpListInformationBase;

struct JumpListThreadData
{
	JumpListThreadData() : jumplists(NULL), event_quit(NULL) {}

	JumpListInformationBase** jumplists;	// pointer to an array of jumplist data classes
	HANDLE event_quit;							// event set when the thread is shut down. Needed for synchronous jump list updates
};

class JumpListInformationBase
{
public:
	JumpListInformationBase(const uni_char *header_title) : m_full_program_path(NULL), m_max_items(10), m_header_title(NULL)
	{ 
		uni_char process_name[_MAX_PATH];

		// Get the absolute path for Opera.exe
		if(!GetModuleFileName(NULL, process_name, _MAX_PATH))
		{
			return;
		}
		m_full_program_path = uni_strdup(process_name);
		m_header_title = header_title ? uni_strdup(header_title) : NULL;

		for(UINT32 i = 0; i < MAX_URL_JUMPLIST_ENTRIES; i++)
		{
			entries[i] = NULL;
		}
	}
	virtual ~JumpListInformationBase() 
	{
		op_free((void *)m_header_title);
		op_free((void *)m_full_program_path);

		for(UINT32 i = 0; i < MAX_URL_JUMPLIST_ENTRIES; i++)
		{
			OP_DELETE(entries[i]);
		}
	}

	virtual JumpListType GetType() = 0;

	struct URLEntry
	{
		URLEntry() : title(NULL), address(NULL), arguments(NULL), icon_path(NULL) { } 

		URLEntry(const uni_char* _title, const uni_char* _address, const uni_char* _arguments, const uni_char* _icon_path) : title(NULL), address(NULL), arguments(NULL), icon_path(NULL)
		{
			if(_title)		
				title = uni_strdup(_title);
			else if (_address)
				title = uni_strdup(_address);

			if(_address)	address = uni_strdup(_address);
			if(_arguments)	arguments = uni_strdup(_arguments);
			if(_icon_path)	icon_path = uni_strdup(_icon_path);
		}
		~URLEntry() 
		{
			op_free(title);
			op_free(address);
			op_free(arguments);
			op_free(icon_path);
		}
		uni_char* title;
		uni_char* address;
		uni_char* arguments;
		uni_char* icon_path;
	};

	// add an frequent entry to the list. All data is copied.
	OP_STATUS AddEntry(UINT32 offset, const uni_char* title, const uni_char* address, const uni_char* arguments, const uni_char* icon_path)
	{
		if(offset > MAX_URL_JUMPLIST_ENTRIES - 1)
			return OpStatus::ERR_OUT_OF_RANGE;

		entries[offset] = OP_NEW(URLEntry, (title, address, arguments, icon_path));
		if(!entries[offset])
			return OpStatus::ERR_NO_MEMORY;

		return OpStatus::OK;
	}
	URLEntry* GetEntry(UINT32 offset)
	{
		if(offset > MAX_URL_JUMPLIST_ENTRIES - 1)
			return NULL;

		return entries[offset];
	}

	const uni_char*		m_full_program_path;
	UINT32				m_max_items;
	const uni_char*		m_header_title;				// title of the jump list section header

private:
	URLEntry* entries[MAX_URL_JUMPLIST_ENTRIES];	// use a static array to avoid accessing core from the thread
};

class JumpListInformationSpeedDial : public JumpListInformationBase
{
public:
	JumpListInformationSpeedDial(const uni_char *header_title) : JumpListInformationBase(header_title) { }

	virtual JumpListType GetType() { return JUMPLIST_TYPE_SPEEDDIAL; }
};

class JumpListInformationFrequent : public JumpListInformationBase
{
public:
	JumpListInformationFrequent(const uni_char *header_title) : JumpListInformationBase(header_title) { }
	virtual ~JumpListInformationFrequent() { }

	virtual JumpListType GetType() { return JUMPLIST_TYPE_FREQUENT; }
};

class JumpListInformationTasks : public JumpListInformationBase
{
public:
	JumpListInformationTasks(const uni_char *header_title) : JumpListInformationBase(header_title)  { }

	virtual JumpListType GetType() { return JUMPLIST_TYPE_TASKS; }
};


class WindowsSevenTaskbarMessageListener
{
public:
	virtual ~WindowsSevenTaskbarMessageListener() {}

	virtual void OnMessageSendIconicThumbnail(WindowsOpWindow *window, UINT32 width, UINT32 height) = 0;
	virtual void OnMessageSendIconicLivePreviewBitmap(WindowsOpWindow *window, UINT32 width, UINT32 height) = 0;
	virtual void OnMessageActivateWindow(WindowsOpWindow *window, BOOL activate) = 0;
	virtual void OnMessageCloseWindow(WindowsOpWindow *window) = 0;
	virtual BOOL CanCloseWindow(WindowsOpWindow *window) = 0;
};

class WindowsSevenTaskbarIntegration: 
	public SpeedDialListener, 
	public SpeedDialEntryListener, 
	public DesktopTransferManager::TransferListener,
	public OpWorkspace::WorkspaceListener, 
	public DesktopWindowListener,
	public Application::BrowserDesktopWindowListener, 
	public WindowsSevenTaskbarMessageListener,
	public MessageObject
{
public:
	WindowsSevenTaskbarIntegration();
	virtual ~WindowsSevenTaskbarIntegration();

	OP_STATUS	Init();
	void		Shutdown();

	/** Called on startup to clear the icon cache for the jump list, 
		it will be rebuilt when the jump list is re-created again.
	*/
	OP_STATUS ClearJumplistIconCache();

	// DesktopTransferManager::TransferListener
	/** Called when data is downloaded. */
	virtual void OnTransferProgress(TransferItemContainer* transferItem, OpTransferListener::TransferStatus status);

	/** Called when an item is added. */
	virtual void OnTransferAdded(TransferItemContainer* transferItem) {}

	/** Called when an item is removed. */
	virtual void OnTransferRemoved(TransferItemContainer* transferItem) {}

	/** Called when new "unread" transfers are done */
	virtual void OnNewTransfersDone() {}

	// SpeedDialListener
	virtual void OnSpeedDialAdded(const DesktopSpeedDial &sd);
	virtual void OnSpeedDialRemoving(const DesktopSpeedDial &sd);
	virtual void OnSpeedDialReplaced(const DesktopSpeedDial &old_sd, const DesktopSpeedDial &new_sd);
	virtual void OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry);
	virtual void OnSpeedDialPartnerIDAdded(const uni_char* partner_id) {}
	virtual void OnSpeedDialPartnerIDDeleted(const uni_char* partner_id) {}
	virtual void OnSpeedDialsLoaded(){}

	// SpeedDialEntryListener
	virtual void OnSpeedDialUIChanged(const DesktopSpeedDial &sd);
	virtual void OnSpeedDialExpired();

	HRESULT CreateJumpList(BOOL async);

	// WorkspaceListener
	virtual void		OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window);
	virtual void		OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window);
	virtual void		OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate);
	virtual void		OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) {}
	virtual void		OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window) {};
	virtual void		OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated) {}
	virtual void		OnDesktopWindowOrderChanged(OpWorkspace* workspace);
	virtual void		OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void		OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void		OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window) {}
	virtual void		OnWorkspaceAdded(OpWorkspace* workspace) {}
	virtual void		OnWorkspaceDeleted(OpWorkspace* workspace);
	virtual void		OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void		OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group){};
	virtual void		OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group){};

	// public DesktopWindowListener
	virtual void		OnDesktopWindowContentChanged(DesktopWindow* desktop_window);

	// BrowserDesktopWindowListener
	virtual void		OnBrowserDesktopWindowCreated(BrowserDesktopWindow *window);
	virtual void		OnBrowserDesktopWindowActivated(BrowserDesktopWindow *window) {}
	virtual void		OnBrowserDesktopWindowDeactivated(BrowserDesktopWindow *window) {}
	virtual void		OnBrowserDesktopWindowDeleting(BrowserDesktopWindow *window);

	// WindowsSevenTaskbarMessageListener
	virtual void		OnMessageSendIconicThumbnail(WindowsOpWindow *window, UINT32 width, UINT32 height);
	virtual void		OnMessageSendIconicLivePreviewBitmap(WindowsOpWindow *window, UINT32 width, UINT32 height);
	virtual void		OnMessageActivateWindow(WindowsOpWindow *window, BOOL activate);
	virtual void		OnMessageCloseWindow(WindowsOpWindow *window);
	virtual BOOL		CanCloseWindow(WindowsOpWindow *window);

	HRESULT ActivateTaskbarThumbnail(HWND hWnd, HWND parent_hwnd);
	HRESULT CreateTaskbarThumbnail(HWND hWnd, HWND parent_hwnd);
	HRESULT RemoveTaskbarThumbnail(HWND hWnd);
	
	// MessageObject
	void				HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void	ScheduleJumplistUpdate();

	HRESULT SetNotifyIcon(const uni_char* icon);

	static HRESULT CreateShellLinkObject(OpComPtr<IShellLink>& link, JumpListInformationBase* information, const uni_char *url, const uni_char *title, const uni_char *arguments = NULL, const uni_char *icon_path = NULL);


private:
	void		WaitForThreadToQuit();
	static void __cdecl JumplistThreadFunc( void* pArguments );

	void UpdateProgress();

	HRESULT AddSpeedDialCategoryToList(JumpListInformationBase& information, OpComPtr<ICustomDestinationList> dest_list, OpComPtr<IObjectArray> poaRemoved);
	HRESULT AddFrequentCategoryToList(JumpListInformationBase& information, OpComPtr<ICustomDestinationList> dest_list, OpComPtr<IObjectArray> poaRemoved);
	HRESULT AddTasks(JumpListInformationBase& information, OpComPtr<ICustomDestinationList> dest_list, OpComPtr<IObjectArray> poaRemoved);

	OP_STATUS BuildFrequentCategoryList(JumpListInformationFrequent* information);
	OP_STATUS BuildSpeedDialCategoryList(JumpListInformationSpeedDial* information);
	OP_STATUS BuildTasksCategoryList(JumpListInformationTasks* information);

	// Get the favicon for the given url and store it in the jumplist cache, return the filename
	// in filename.
	OP_STATUS GenerateICOFavicon(OpStringC url, OpString& filename);

private:
	BOOL	m_has_active_transfers;		// TRUE if we have active transfers
	BOOL	m_posted_progress_update;	// TRUE if a progress update message has been sent already
	BOOL	m_jumplist_created;			// TRUE when we can update the jumplist
	ITaskbarList3*	m_pTaskbarList;
	OpVector<OpWorkspace> m_workspaces;	// In some cases, we don't get to unregister us as listeners so we need to do it on exit instead
	OpVector<DesktopSpeedDial> m_speeddials; // Same as above
	HANDLE	m_event_quit;				// event set when the thread should shut down
};
/*
class WindowsNotifier : public DesktopNotifier
{
public:
	virtual				~WindowsNotifier(){}

	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, const OpStringC8& image, OpInputAction* action, BOOL auto_close, BOOL show_instantly, OpRect* custom_screen_rect = NULL, BOOL3 left_aligned = YES, BOOL3 top_aligned = NO, BOOL managed = FALSE, BOOL wrapping = FALSE, DesktopNotifier::NotificationType notification_type = DesktopNotifier::SERIALIZED_NOTIFICATION) = 0;
	virtual OP_STATUS Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL auto_close, BOOL show_instantly, OpRect* custom_screen_rect = NULL, BOOL3 left_aligned = YES, BOOL3 top_aligned = NO, BOOL managed = FALSE, BOOL wrapping = FALSE) = 0;

	virtual OP_STATUS AddButton(const OpStringC& text, OpInputAction* action) { OP_ASSERT(!"Don't call this, it's not available on Windows"); }

	virtual void StartShow() = 0;

	virtual BOOL IsManaged() { return FALSE; }	// windows controls the notifications

	virtual DesktopNotifier::NotificationType GetNotificationType() = 0;

	// notifier is controlled by the OS, so no listener can be given any kind of notification
	virtual void AddListener(DesktopNotifierListener* listener) {}
	virtual void RemoveListener(DesktopNotifierListener* listener) {}
	virtual void SetReceiveTime(time_t time) {}

};
*/
class WindowsOpTaskbar : public AccountListener, public MailNotificationListener, public OpPrefsListener
#ifdef IRC_SUPPORT
	, public ChatListener
#endif // IRC_SUPPORT
#ifdef WEBSERVER_SUPPORT
	, public MessageObject
#endif
{
public:
	WindowsOpTaskbar();
	~WindowsOpTaskbar();

	void AppHidden();
	void AppShown();

#ifdef WEBSERVER_SUPPORT
	// unite attention state changed
	virtual void	 HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif

	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	void NeedNewMessagesNotification(Account* account, unsigned count);
	void NeedNoMessagesNotification(Account * account) {}
	void NeedSoundNotification(const OpStringC& sound_path) {}
	void OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active);

	void OnAccountAdded(UINT16 account_id);
	void OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
	void OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) {}
	void OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
	void OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
	void OnFolderLoadingCompleted(UINT16 account_id) {}
#ifdef IRC_SUPPORT
	void OnChatStatusChanged(UINT16 account_id) {}
	void OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat) {}
	void OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) {}
	void OnChatReconnecting(UINT16,const ChatInfo &) {}
	void OnChatRoomJoining(UINT16 account_id, const ChatInfo& room) {}
	void OnChatRoomJoined(UINT16 account_id, const ChatInfo& room) {}
	void OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
	void OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
	BOOL OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {return TRUE;}
	void OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {}
	void OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
	void OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
	void OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value) {}
	void OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) {}
	void OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room) {}
	void OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) {}
	void OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread);
#endif //IRC_SUPPORT
	static LONG APIENTRY WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifdef SELFTEST
	OP_STATUS Notify(OpString8& message);
#endif // SELFTEST

	void OnExitStarted();

private:

	enum Message
	{
		ID_EXIT_OPERA = 10000,
		ID_ACTIVATE_OPERA,
	};

	OP_STATUS InitTrayIcon(BOOL apphiden=FALSE);
	OP_STATUS UninitTrayIcon(BOOL force=FALSE);
	OP_STATUS Update(DWORD message);
	OP_STATUS InitWindows7TaskbarIntegration();
	void ShutdownWindows7TaskbarIntegration();

	HWND	m_hwnd;
	BOOL	m_unread_mail;
	BOOL	m_in_mailwindow;
	BOOL	m_unread_chat;
	BOOL	m_unite_attention;
	BOOL	m_trayicon_inited;
	OpVector<MailDesktopWindow> m_active_mail_windows;
	OpAutoPtr<WindowsSevenTaskbarIntegration>	m_windows7_taskbar_integration;
};

#endif // WINDOWSOPTASKBAR_H
