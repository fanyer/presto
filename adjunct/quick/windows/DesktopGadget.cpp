/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "adjunct/quick/windows/DesktopGadget.h"

#include "adjunct/desktop_pi/desktop_menu_context.h"

#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/handlers/DownloadItem.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/dialogs/jsdialogs.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"	// for EscapeURLIfNeeded

#include "adjunct/widgetruntime/GadgetTooltipHandler.h"
#include "adjunct/widgetruntime/GadgetUtils.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/pi/system/OpPlatformViewers.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/WidgetWindow.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver_request.h"
#endif // WEBSERVER_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "adjunct/desktop_pi/desktop_file_chooser.h"
# include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/quick/dialogs/AskPluginDownloadDialog.h"
#ifdef DOM_GEOLOCATION_SUPPORT
#include "adjunct/widgetruntime/GadgetPermissionListener.h"
#endif //DOM_GEOLOCATION_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
#include "adjunct/widgetruntime/widgets/OpGadgetControlButtons.h"
#define DELAYED_STDBUTTON_HIDE 1000 // ms
#define WCB_FEATURE UNI_L("http://xmlns.opera.com/wcb")
#define WCB_FEATURE_PARAM UNI_L("widgetcontrolbuttons")
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT

#ifdef WIDGETS_UPDATE_SUPPORT
#include "adjunct/widgetruntime/widgets/OpUpdateToolbar.h"
#endif //WIDGETS_UPDATE_SUPPORT

#ifdef _MACINTOSH_
#include "adjunct/embrowser/EmBrowser_main.h"
#endif

#ifdef PLUGIN_AUTO_INSTALL
#include "adjunct/quick/managers/PluginInstallManager.h"
#endif // PLUGIN_AUTO_INSTALL

/**
 * Toplevel container window for DesktopGadget object.
 *
 * @see DesktopGadget
 *
 * @author Patryk Obara
 */
class GadgetContainerWindow
		: public DesktopWindow
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		, public OpTimerListener
		, public GadgetButtonMouseListener
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT
#ifdef WIDGETS_UPDATE_SUPPORT
		, public GadgetUpdateListener
#endif //WIDGETS_UPDATE_SUPPORT
{
	public:

	GadgetContainerWindow(DesktopGadget* gadget_doc_window)
		: m_gadget_document_window(gadget_doc_window)
		, m_decorated(FALSE)
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		, m_wcb_enabled(TRUE)
		, m_buttons_window(NULL)
		, m_no_hover_from_menu(FALSE)
		, m_block_wcb_hiding(FALSE)
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT
#ifdef WIDGETS_UPDATE_SUPPORT
		, m_updater(NULL)
		, m_update_bar(NULL)
#endif //WIDGETS_UPDATE_SUPPORT
	{}


	virtual ~GadgetContainerWindow()
	{
		// gadget_doc_window is child of this window, but owns it
		m_gadget_document_window = NULL;

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		OP_DELETE(m_buttons_window);
		m_buttons_window = NULL;
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT
	}


	OP_STATUS Init(BOOL decorated)
	{
		m_decorated = decorated;
#ifdef DECORATED_GADGET_SUPPORT
		if(m_decorated)
		{	
			m_wcb_enabled = FALSE;
			RETURN_IF_ERROR( DesktopWindow::Init(OpWindow::STYLE_DECORATED_GADGET));
		}
		else
#endif // DECORATED_GADGET_SUPPORT
		{
			RETURN_IF_ERROR( DesktopWindow::Init(OpWindow::STYLE_GADGET)); // toplevel container window
		}

		// No window skin on gadgets
		SetSkin(NULL, NULL);
		
#ifdef _MACINTOSH_ 
		SetParentInputContext(m_gadget_document_window);
#endif // _MACINTOSH_

		return OpStatus::OK;
	}


	/// Adjusts gadget position inside container window
	void AdjustGadgetPosition()
	{
		UINT32 pos_x = 0, pos_y = 0;
		AdjustXY(pos_x, pos_y);
		m_gadget_document_window->GetOpWindow()->SetInnerPos(pos_x, pos_y);
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		if(m_buttons_window)
		{
			OpRect rect = m_buttons_widget->GetRect();
			OpRect main_wnd;
			GetRect(main_wnd);
			m_buttons_window->GetWindow()->SetInnerPos(main_wnd.width-rect.width,0);
		}
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT
	}


	BOOL HandleDragStart(INT32 x, INT32 y)
	{
		return FALSE;
	}


	BOOL HandleDragMove(INT32 x, INT32 y)
	{
		INT32 pos_x, pos_y;
    	GetWindow()->GetOuterPos(&pos_x, &pos_y);
		GetWindow()->SetOuterPos(pos_x+x, pos_y+y);
		return FALSE;
	}


	BOOL HandleDragStop()
	{
		return FALSE;
	}


	/**
	 * Calling this method results in resizing container inner/outer sizes
	 * to width and height reported by document.
	 *
	 * Gadget's inner and outer sizes are always equal.
	 * For gadgets in application mode inner size of container window should be
	 * equal to gadget's size. For other gadgets size of container depend on
	 * state of control buttons.
	 *
	 * @param width Width of gadget's document window.
	 * @param height Height of gadget's document window.
	 */
	void Resize(UINT32 width, UINT32 height)
	{
		AdjustXY(width, height);
		if(m_wcb_enabled)
		{
    		width = MAX(width, WCB_PANEL_WIDTH - WCB_UPDATE_BUTON_SIZE);
    		height= MAX(height, WCB_BUTTON_SIZE);
		}
		GetOpWindow()->SetInnerSize(width, height);
	}

	// == DesktopWindow interface ======================

	void OnResize(INT32 width, INT32 height)
	{
		// This OnResize may originate from platform code
		// (resizing through window manager).
		UINT32 pos_x = 0, pos_y = 0;
		AdjustXY(pos_x, pos_y);
		height -= pos_y;
		if( m_gadget_document_window->IsInitialized() )
		{
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
			if(m_buttons_window)
			{
				OpRect rect = m_buttons_widget->GetRect();
				m_buttons_window->GetWindow()->SetInnerPos(width-rect.width,0);
			}
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT
			m_gadget_document_window->SetInnerSize(width, height);
		}
	}


	void OnClose(BOOL user_initiated)
	{
	}


#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT

	OP_STATUS InitializeControlButtons()
	{
#ifdef WIDGETS_UPDATE_SUPPORT	
		m_wcb_enabled = !m_decorated; // if update support enabled, single update button should appear even if 
									  // widget developer turns off that feature
#else
		m_wcb_enabled = !m_decorated && m_gadget_document_window->HasControlButtonsEnabled();
#endif 
		if(m_wcb_enabled)
		{
			BOOL wcb_feature = m_gadget_document_window->HasControlButtonsEnabled();

			RETURN_OOM_IF_NULL(m_buttons_window = OP_NEW(DesktopWidgetWindow, ()));			
			if (OpStatus::IsError(m_buttons_window->Init(OpWindow::STYLE_TRANSPARENT, "Gadget Buttons Window", GetWindow())))
			{
				OP_DELETE(m_buttons_window);
				return OpStatus::ERR;
			}

			// needed for popupmenu to work (!)
			m_buttons_window->GetWidgetContainer()->SetParentInputContext(m_gadget_document_window);

			RETURN_OOM_IF_NULL(m_buttons_widget = OP_NEW(GadgetControlButtons,()));
			if (OpStatus::IsError(m_buttons_widget->Init(!wcb_feature)))
			{
				OP_DELETE (m_buttons_widget);
				return OpStatus::ERR;
			}

			m_buttons_widget->SetMouseListener(this);

			UINT32 panel_width = wcb_feature ? WCB_PANEL_WIDTH - WCB_UPDATE_BUTON_SIZE : WCB_PANEL_WIDTH_UPDATE ;

			m_buttons_widget->SetRect(OpRect(0, 0,panel_width,WCB_BUTTON_SIZE));
			m_buttons_window->GetWidgetContainer()->GetRoot()->AddChild(m_buttons_widget);	
			m_buttons_window->GetWindow()->SetInnerSize(panel_width, WCB_BUTTON_SIZE);
			m_buttons_window->Show(FALSE);

			// make document window not resizable below 1px height size
			GetOpWindow()->SetMinimumInnerSize( panel_width, 
				WCB_BUTTON_SIZE + WCB_SEPARATOR_WIDTH + 1);
		}

		m_hide_stdbutton_timer.SetTimerListener(this);
		return OpStatus::OK;
	}


    BOOL PreClose()
    {
#ifdef _MACINTOSH_
		m_gadget_document_window->Close(YES);
		WidgetShutdownLibrary();
#else	// !_MACINTOSH_
		m_gadget_document_window->Close();
#endif	// _MACINTOSH_
		return TRUE;
    }

	/// This method mimics behaviour of document listener.
	void OnDocumentWindowHover()
	{
		m_hide_stdbutton_timer.Stop();
		if (m_wcb_enabled)
		{
			m_buttons_window->Show(TRUE);
		}
	}


	/// This method mimics behaviour of document listener.
	void OnDocumentWindowLeave()
	{
		// this  doc leave does not come to us dependently :/
		if (m_no_hover_from_menu)
		{
			return;
		}
		if(m_wcb_enabled)
		{
			m_hide_stdbutton_timer.Start(DELAYED_STDBUTTON_HIDE);	
		}
	}



    // == OpTimerListener ======================

	void OnTimeOut(OpTimer *timer)
	{
		if (timer == &m_hide_stdbutton_timer)
		{
			if (!m_no_hover_from_menu && !m_block_wcb_hiding) 
			{
				m_buttons_window->Show(FALSE);	
			}
		}
	}


	// == GadgetButtonMouseListener ======================

	void OnMouseMove() 
	{
		m_hide_stdbutton_timer.Stop();
	}


	void OnMouseLeave()
	{
		if(m_wcb_enabled)
		{
			m_hide_stdbutton_timer.Start(DELAYED_STDBUTTON_HIDE);	
		}
	}

	void OnMinimize()
	{
		if (m_gadget_document_window)
		{
			DesktopGadget::GadgetStyle gadget_style =  m_gadget_document_window->GetGadgetStyle();
			if ( gadget_style == DesktopGadget::GADGET_STYLE_ALWAYS_ON_TOP || gadget_style == DesktopGadget::GADGET_STYLE_ALWAYS_BELOW)
			{
				return;
			}
			Minimize();
		}
	}		   


	void OnClose()
	{
		m_gadget_document_window->Close();
		Close();
	}


	void OnShowMenu() 
	{
		// Control buttons must be visible while blocking PopupMenu window
		// is active.
		BlockControlButtons();
		m_gadget_document_window->OnPopupMenu(NULL);
		ReleaseControlButtons();
	}


	void OnDragMove(INT32 x, INT32 y)
	{
		HandleDragMove(x,y);
	}
	void OnPanelSizeChanged()
	{
		AdjustGadgetPosition();
	}

	BOOL HasControlButtons() 
	{
		return m_wcb_enabled;
	}

	void BlockPanelHide(BOOL block)
	{
		m_block_wcb_hiding = block;
		if(block)
		{
			m_buttons_window->Show(TRUE);
		}
	}

#ifdef WIDGETS_UPDATE_SUPPORT
	void SetUpdateController(WidgetUpdater& ctrl) 
	{
		m_updater = &ctrl;
		if (m_wcb_enabled)
		{
			m_buttons_widget->SetUpdateController(ctrl);
		}	

		if(m_decorated)
		{
			m_updater->GetController()->AddUpdateListener(this);

			m_update_bar = OP_NEW(OpUpdateToolbar,(ctrl));
			if (m_update_bar)
				GetWidgetContainer()->GetRoot()->AddChild(m_update_bar,FALSE,TRUE);
		}
	}
#endif //WIDGETS_UPDATE_SUPPORT

#endif // WIDGET_CONTROL_BUTTONS_SUPPORT



	void BlockControlButtons()
	{
		m_no_hover_from_menu = TRUE; 
	}


	void ReleaseControlButtons()
	{
		m_no_hover_from_menu = FALSE;
		if(m_wcb_enabled)
		{
			m_hide_stdbutton_timer.Start(DELAYED_STDBUTTON_HIDE);	
		}
	}
	

#ifdef WIDGETS_UPDATE_SUPPORT
	//=================GadgetUpdateListener==================
	void OnGadgetUpdateAvailable(GadgetUpdateInfo* data,BOOL visible)
	{
		if (!m_update_bar)
			return;
		
		if(visible)
		{
			if (!m_updater->NotificationAllowed())
				return;
			m_update_bar->Show();
		}
		else
		{
			m_update_bar->Hide();
		}
	}
	//=================GadgetUpdateListener==================
	void OnGadgetUpdateStarted(GadgetUpdateInfo* data)
	{
		m_updater->GetController()->RemoveUpdateListener(this);
	}
	

	void OnLayout()
	{
		OpRect rect;
		GetBounds(rect);
		if (m_update_bar)
		{
			rect = m_update_bar->LayoutToAvailableRect(rect);
			m_gadget_document_window->SetInnerPos(0,rect.y);
			m_gadget_document_window->SetOuterSize(rect.width,rect.height);
		}
	}

#endif //WIDGETS_UPDATE_SUPPORT 
	
	private:

	/**
	 * Helper function that modifies given x,y values depending on
	 * state of decoration/control buttons.
	 */
	void AdjustXY(UINT32 &x, UINT32 &y)
	{
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		if(m_wcb_enabled)
			y += WCB_BUTTON_SIZE + WCB_SEPARATOR_WIDTH;
#endif
		return;
	}

	DesktopGadget*	m_gadget_document_window;
	BOOL			m_decorated;

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
	BOOL			m_wcb_enabled;
	DesktopWidgetWindow*	m_buttons_window;
	OpTimer			m_hide_stdbutton_timer; // timer used to hide std buttons after some time when on no hover event occured. 
	BOOL			m_no_hover_from_menu;
	GadgetControlButtons* m_buttons_widget;
	BOOL			m_block_wcb_hiding;
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT

#ifdef WIDGETS_UPDATE_SUPPORT
	WidgetUpdater*			m_updater;
	OpUpdateToolbar*				m_update_bar;
#endif //WIDGETS_UPDATE_SUPPORT

};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
GadgetVersion::GadgetVersion()
	: m_version(0)
	, m_additional_info()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

GadgetVersion::GadgetVersion(const GadgetVersion & version)
	: m_version(version.m_version)
{
	m_additional_info.Set(version.m_additional_info.CStr());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
GadgetVersion::~GadgetVersion()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO: make this function less redundant
OP_STATUS
GadgetVersion::Set(const OpStringC & version_string)
{
	Reset(); // set version to 0 first

	if (version_string.IsEmpty())
	{
		// version == 0
		return OpStatus::OK;
	}
	
	INT32 pos = 0;

	UINT32 part_len;
	UINT32 version_part;

	if (OpStatus::IsSuccess(GetVersionPart(version_string, pos, -1, &version_part, &part_len)) && part_len > 0)
	{
		SetMajor(version_part);
		pos += part_len; // goto pos of next potential '.'

		// to have a minor version, the string must be at least two chars longer (dot and number)
		if (pos + 1 < version_string.Length())
		{
			if (version_string[pos] == '.')
			{
				if (OpStatus::IsSuccess(GetVersionPart(version_string, pos + 1, -1, &version_part, &part_len)) && part_len > 0)
				{
					SetMinor(version_part);
					pos += part_len + 1; // goto pos of next potential '.'

					// to have a revision number, the string must be at least two chars longer (dot and number)
					if (pos + 1 < version_string.Length())
					{
						if (version_string[pos] == '.')
						{
							if (OpStatus::IsSuccess(GetVersionPart(version_string, pos + 1, -1, &version_part, &part_len)) && part_len > 0)
							{
								SetRevision(version_part);
								pos += part_len + 1;
							}
						}
					}
				}
			}
		}
	}

	return m_additional_info.Set(version_string.SubString(pos));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
GadgetVersion::GetVersionPart(const OpStringC & version_string, UINT32 offset, INT32 supported_len, UINT32 * version_part, UINT32 * version_part_len)
{
	if (version_part == NULL || version_part_len == NULL)
	{
		return OpStatus::ERR;
	}

	const uni_char* current_pos = version_string.DataPtr();
	current_pos += offset;

	INT32 len = 0;
	while (current_pos && uni_isdigit(*current_pos))
	{
		len++;
		current_pos++;
	}
	if (len > 0)
	{
		OpString ver_str;
		RETURN_IF_ERROR(ver_str.Set(version_string.SubString(offset), len));
		
		// append zeros, if neccessary
		if (supported_len > 0)
		{
			if (supported_len < len)
			{
				return OpStatus::ERR;
			}
			for (INT32 i = len; i < supported_len; i++)
			{
				RETURN_IF_ERROR(ver_str.Append(UNI_L("0")));
			}
		}
		
		// set return values
		*version_part = uni_atoi(ver_str.CStr());
		*version_part_len = len;

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
GadgetVersion::Reset()
{
	m_version = 0;
	m_additional_info.Empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
GadgetVersion::SetMajor(UINT32 major)
{
	UINT64 major_64 = major;

	if ((major_64 & s_major_ver_mask) != major) // too big of an integer
	{
		return OpStatus::ERR;
	}
	m_version |= ((major_64 & s_major_ver_mask) << s_major_ver_shift); 

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
GadgetVersion::SetMinor(UINT32 minor)
{
	UINT64 minor_64 = minor;

	if ((minor_64 & s_ver_mask) != minor) // too big of an integer
	{
		return OpStatus::ERR;
	}
	m_version |= ((minor_64 & s_ver_mask) << s_minor_ver_shift); 

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS
GadgetVersion::SetRevision(UINT32 revision)
{
	UINT64 revision_64 = revision;

	if ((revision_64 & s_ver_mask) != revision) // too big of an integer
	{
		return OpStatus::ERR;
	}
	m_version |= (revision_64 & s_ver_mask);

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINT32
GadgetVersion::GetMajor() const
{
	return (UINT32) ((m_version >> s_major_ver_shift) & s_major_ver_mask);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINT32
GadgetVersion::GetMinor() const
{
	return (UINT32) ((m_version >> s_minor_ver_shift) & s_ver_mask);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

UINT32
GadgetVersion::GetRevision() const
{
	return (UINT32) (m_version & s_ver_mask);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL
GadgetVersion::operator ==(const GadgetVersion & version) const
{
	return (m_version == version.m_version && m_additional_info.CompareI(version.m_additional_info) == 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL
GadgetVersion::operator <(const GadgetVersion & version) const
{
	if (m_version < version.m_version)
	{
		return TRUE;
	}
	if (m_version == version.m_version)
	{
		return (m_additional_info.CompareI(version.m_additional_info) < 0);
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL
GadgetVersion::operator >(const GadgetVersion & version) const
{
	if (m_version > version.m_version)
	{
		return TRUE;
	}
	if (m_version == version.m_version)
	{
		return (m_additional_info.CompareI(version.m_additional_info) > 0);
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
GadgetVersion
GadgetVersion::operator =(const GadgetVersion & version)
{
	m_version = version.m_version;
	m_additional_info.Set(version.m_additional_info.CStr());

	return *this;
}




/***********************************************************************************
 **
 **	DesktopGadget
 **
 ***********************************************************************************/

DesktopGadget::DesktopGadget(OpWindowCommander* commander)
	: m_container_window(NULL)
	, m_decorated(FALSE)
	, m_drag_requested(FALSE)
#ifdef DOM_GADGET_FILE_API_SUPPORT
	, m_file_choosing_callback(0)
#endif // DOM_GADGET_FILE_API_SUPPORT
	, m_initialized(FALSE)
	, m_has_forced_focus(FALSE)
	, m_gadget_style(commander && commander->GetGadget()->IsSubserver()
			? GADGET_STYLE_HIDDEN : GADGET_STYLE_NORMAL)
	, m_scale(100)
	, m_last_mouse_position(-1000,-1000)
	, m_window_commander(commander)
	, m_last_notification_valid(FALSE)
	, m_chooser(0)
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	, m_previous_widgetprefs_quota(0)
	, m_previous_localstorage_quota(0)
	, m_previous_webdatabases_quota(0)
#endif // DATABASE_MODULE_MANAGER_SUPPORT
	, m_tooltip_handler(NULL)
#ifdef WEBSERVER_SUPPORT
	, m_current_request_queue_num(0)
	, m_act_state(DESKTOP_GADGET_NO_ACTIVITY)
	, m_slow_timer(FALSE)
	, m_websubserver_listener_set(FALSE)
#endif // WEBSERVER_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT
	, m_permission_listener(NULL)
#endif //DOM_GEOLOCATION_SUPPORT
{
#ifdef WEBSERVER_SUPPORT
	// Set the request queue to -1 or not used
	for (INT32 i = 0; i < DESKTOP_GADGET_TRAFFIC_QUEUE_LENGTH; i++)
		m_request_count[i] = -1;

	m_traffic_timer.SetTimerListener(this);
#endif // WEBSERVER_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT
	g_main_message_handler->SetCallBack(this, MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this);
#endif //DOM_GEOLOCATION_SUPPORT
}



DesktopGadget::~DesktopGadget()
{
#ifdef PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_REMOVE_WINDOW, NULL, this));
#endif // PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
	OP_DELETE(m_tooltip_handler);
	m_tooltip_handler = NULL;

#ifdef WEBSERVER_SUPPORT
	RemoveWebsubServerListener();
#endif // WEBSERVER_SUPPORT

	if (m_chooser)
	{
		m_chooser->Cancel();
		OP_DELETE(m_chooser);
		m_chooser = NULL;
	}

	if (m_window_commander)
	{
		m_window_commander->SetMenuListener(NULL);
		m_window_commander->SetDocumentListener(NULL);
		m_window_commander->SetLoadingListener(NULL);
		m_window_commander->OnUiWindowClosing();
	}

#ifdef DOM_GEOLOCATION_SUPPORT
	g_main_message_handler->UnsetCallBacks(this);
	if (NULL != m_permission_listener)
	{
		m_permission_listener->SetParentInputContext(NULL);
		OP_DELETE(m_permission_listener);
		m_permission_listener = NULL;
	}
#endif //DOM_GEOLOCATION_SUPPORT

	OP_DELETE(m_container_window);
	m_container_window = NULL;
}

OP_STATUS DesktopGadget::Init()
{
	OP_ASSERT(!m_initialized);
	OP_ASSERT(GetOpGadget());

#ifdef WEBSERVER_SUPPORT
	if (GetOpGadget() && GetOpGadget()->IsSubserver())
	{
		RETURN_IF_ERROR(InitializeWebserver());
	}
	else
#endif // WEBSERVER_SUPPORT
	{
		RETURN_IF_ERROR(InitializeGadget());
	}

	OP_ASSERT(m_initialized);
    return OpStatus::OK;
}

OP_STATUS DesktopGadget::InitializeContainerWindow()
{
	m_container_window = OP_NEW(GadgetContainerWindow, (this));
	m_decorated = FALSE;
#ifdef DECORATED_GADGET_SUPPORT

	OP_ASSERT(GetOpGadget());
	if (GetOpGadget())
	{
		switch( GetOpGadget()->GetDefaultMode() )
		{
			// (w3c) when gadget declares (all) as default mode, it means that
			// author doesn't care which mode it should be displayed in
			// and we get to choose; we choose floating:
						
		case WINDOW_VIEW_MODE_APPLICATION:
			m_decorated = TRUE;
			break;

		// If unspecified, default to floating.
		case WINDOW_VIEW_MODE_UNKNOWN:

		// (w3c) when gadget declares (all) as default mode, it means that
		// author doesn't care which mode it should be displayed in
		// and we get to choose; we choose floating:

		case WINDOW_VIEW_MODE_FLOATING:	// (w3c) same as widget
		case WINDOW_VIEW_MODE_WIDGET:

		case WINDOW_VIEW_MODE_DOCKED:
			m_decorated = FALSE;
			break;

		case WINDOW_VIEW_MODE_FULLSCREEN:	// (w3c) unsupported
		default: 
			OP_ASSERT(!"Unsupported gadget mode.");
			m_decorated = FALSE;
		}
	}

#endif // DECORATED_GADGET_SUPPORT
	OP_STATUS status = m_container_window->Init(m_decorated);
	return status;
	
}


OP_STATUS DesktopGadget::ConfigureWindowCommander()
{
	OP_ASSERT(m_window_commander != NULL);

	RETURN_IF_ERROR(m_window_commander->OnUiWindowCreated(GetOpWindow()));
#ifdef DOM_GEOLOCATION_SUPPORT	
	m_permission_listener = OP_NEW(GadgetPermissionListener, (this));
	m_permission_listener->SetParentInputContext(this);
	WindowCommanderProxy::SetParentInputContext(m_window_commander, m_permission_listener);
#else
	WindowCommanderProxy::SetParentInputContext(m_window_commander, this);
#endif //DOM_GEOLOCATION_SUPPORT

	m_window_commander->SetDocumentListener(this);
	m_window_commander->SetMenuListener(this);
	m_window_commander->SetLoadingListener(this);
#ifdef DOM_GADGET_FILE_API_SUPPORT
	m_window_commander->SetFileSelectionListener(this);
#endif

	// Hack.  The OpWindow's OpWindowListener is always set upon DesktopWindow
	// initialization.  However, the window commander resets it in
	// OnUiWindowCreated().
	ResetWindowListener();

	return OpStatus::OK;
}


OP_STATUS DesktopGadget::InitializeGadget()
{
	OP_ASSERT(m_window_commander != NULL);

	RETURN_IF_ERROR( InitializeContainerWindow() );		

	RETURN_IF_ERROR(DesktopWindow::Init(OpWindow::STYLE_TRANSPARENT, m_container_window->GetOpWindow()));

	// No window skin on gadgets
	SetSkin(NULL, NULL);
	
	RETURN_IF_ERROR(ConfigureWindowCommander());

	if (!m_decorated)
	{
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		m_container_window->InitializeControlButtons();
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT
	}
	else 
	{
#ifdef _MACINTOSH_
		//this will block resizing of gadget window below its usable level
		SetMinimumInnerSize(70, 16);
#endif //_MACINTOSH_
	}

	m_container_window->AdjustGadgetPosition();
	m_initialized = TRUE; // it is ok to resize from now on

	OP_ASSERT(GetOpGadget());
	if (!GetOpGadget())
		return OpStatus::ERR; // Should never happen
	
	UINT32 initial_width = GetOpGadget()->InitialWidth();
	UINT32 initial_height = GetOpGadget()->InitialHeight();

	m_container_window->Resize(initial_width, initial_height);


	//
	// HACKS from old implementation follow:
	// 

	VisualDevice* vis_dev = WindowCommanderProxy::GetVisualDevice(m_window_commander);
	if (vis_dev)
	{
		vis_dev->SetScrollType(VisualDevice::VD_SCROLLING_NO);
	}
	
	WidgetContainer *wgt_container = this->GetWidgetContainer();
	if (wgt_container)
	{
		wgt_container->GetOpView()->SetVisibility(FALSE);
	}
	// always override this so that we can move widgets when pan is scroll is on
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ScrollIsPan))
	{
		WindowCommanderProxy::OverrideScrollIsPan(m_window_commander);
	}
	
	if(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToReceiveRightClicks) == TRUE)
	{
		OP_STATUS rc;
		TRAP(rc, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::AllowScriptToReceiveRightClicks, FALSE));
	}

	//
	// End of HACKS from old implementation
	// 
#ifdef WIC_PERMISSION_LISTENER
	GetWindowCommander()->SetPermissionListener(m_permission_listener);
#endif //WIC_PERMISSION_LISTENER

	m_tooltip_handler = OP_NEW(GadgetTooltipHandler, ());
	RETURN_OOM_IF_NULL(m_tooltip_handler);
	RETURN_IF_ERROR(m_tooltip_handler->Init());

	m_container_window->Show(TRUE);

	Show(TRUE);
	// If the gadget is visible, make it accessible
	if (GetGadgetStyle() != GADGET_STYLE_ALWAYS_BELOW)
	{
		Activate(TRUE);
	}

#ifdef WIDGETS_UPDATE_SUPPORT

// temp update sheduler

	OP_ASSERT(GetOpGadget());
	OpGadget* for_update  = GetOpGadget();
	if (for_update)
		for_update->Update();
// temp update sheduler end

#endif //WIDGETS_UPDATE_SUPPORT

	return OpStatus::OK;
}


#ifdef WEBSERVER_SUPPORT
OP_STATUS DesktopGadget::InitializeWebserver()
{
	RETURN_IF_ERROR( DesktopWindow::Init(OpWindow::STYLE_HIDDEN_GADGET) );

	// No window skin on gadgets
	SetSkin(NULL, NULL);
	
	m_initialized = TRUE;

#ifdef MSWIN
	//(julienp)
	//Because otherwise hidden gadgets won't work, because the OpWindow gets in an inconsistent state
	//This is an ugly hack but the alternative is to study in details what happens there and probably
	//do some architectural changes
	Show(TRUE);
#endif // MSWIN

	RETURN_IF_ERROR(ConfigureWindowCommander());

	// Start the timer again for the next collection
	m_traffic_timer.Start(DESKTOP_GADGET_TRAFFIC_TIMEOUT);
	SetWebsubServerListener();

	Show(FALSE);

	return OpStatus::OK;
}
#endif // WEBSERVER_SUPPORT

OpGadget* DesktopGadget::GetOpGadget()
{
	OP_ASSERT(m_window_commander != NULL);
	return m_window_commander->GetGadget();
}

const OpGadget* DesktopGadget::GetOpGadget() const
{
	OP_ASSERT(m_window_commander != NULL);
	return m_window_commander->GetGadget();
}

void DesktopGadget::SetIcon(Image &icon)
{

	OP_ASSERT(GetOpGadget());
	if(GetOpGadget() && !GetOpGadget()->IsSubserver()) // && !m_decorated
	{
		m_container_window->SetIcon(icon);
	}
	else
	{
		DesktopWindow::SetIcon(icon);
	}
}


void DesktopGadget::CenterOnScreen()
{
	OpRect rect;
	GetScreenRect(rect, FALSE);
	UINT32 width, height;
	GetOuterSize(width, height);
	SetOuterPos((rect.width-width)/2, (rect.height-height)/2);
}


void DesktopGadget::OnClose(OpWindowCommander* commander)
{
	OpInputAction action(OpInputAction::ACTION_CLOSE_WINDOW);
	OnInputAction(&action);
}


void DesktopGadget::OnDownloadRequest(OpWindowCommander *commander, DownloadCallback *download_callback)
{
	// see OpPageView::OnPageDownloadRequest for reference implementation
	OP_ASSERT(download_callback);

	if (!m_chooser)
		RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	DownloadItem * di = OP_NEW(DownloadItem, (download_callback, m_chooser, TRUE ));
	if (di)
	{
		if (download_callback->Mode() == OpDocumentListener::SAVE)
		{
			di->Init();
			di->Save(FALSE);
		}
	}
}


void DesktopGadget::OnAskPluginDownload(OpWindowCommander* commander)
{
#ifdef WIDGET_RUNTIME_SUPPORT
	OpAutoPtr<AskPluginDownloadDialog> dialog(
			OP_NEW(AskPluginDownloadDialog, ()));
	if (NULL != dialog.get() && OpStatus::IsSuccess(
				dialog->Init(g_application->GetActiveDesktopWindow(),
						UNI_L("Adobe Flash Player"),
						UNI_L("http://redir.opera.com/plugins/?application/x-shockwave-flash"),
						Str::D_SHORT_PLUGIN_DOWNLOAD_RESOURCE_FROM_WIDGET_MESSAGE,
						TRUE)))
	{
		dialog.release();
	}
#endif // WIDGET_RUNTIME_SUPPORT
}

void DesktopGadget::OnActivate(BOOL activate, BOOL first_time)
{
	//reset the gadget style if it has been temporary raised
	if (!activate && m_has_forced_focus && GetGadgetStyle() == GADGET_STYLE_ALWAYS_BELOW)
	{
		GetOpWindow()->SetAlwaysBelow(TRUE);
		m_has_forced_focus = FALSE;
	}
}


BOOL DesktopGadget::Activate(BOOL activate)
{
	OP_ASSERT(GetOpGadget());
#ifdef WEBSERVER_SUPPORT
	if (activate && GetGadgetStyle() == GADGET_STYLE_HIDDEN)
	{
		if (GetOpGadget() && !GetOpGadget()->IsSubserver())
			SetGadgetStyle(GADGET_STYLE_NORMAL);
	}
#endif
	if (activate && GetGadgetStyle() == GADGET_STYLE_ALWAYS_BELOW)
	{
		//unset the below flag, so the window gets visible
		GetOpWindow()->SetAlwaysBelow(FALSE);
		m_has_forced_focus = TRUE;
	}
	else
		m_has_forced_focus = FALSE;
	
	return DesktopWindow::Activate(activate);
}

void DesktopGadget::OnGadgetDragStart (OpWindowCommander* commander, INT32 x, INT32 y)  //  What's the commander for?
{
	OP_ASSERT(GetOpGadget());
	if( m_container_window->HandleDragStart(x, y) )
	{
	    m_drag_requested = FALSE;
		return;
	}
#if defined(_UNIX_DESKTOP_)
	if (m_gadget_style == GADGET_STYLE_ALWAYS_ON_TOP || m_gadget_style == GADGET_STYLE_NORMAL )
	{
		// Not all window managers will raise the gadget window if the window is not
		// obscured by something else when we click on the gadget window. Since we
		// drag the window internally the window manager will not raise it then either.
		// We have to help the window manager [espen 2006-05-15]
		m_container_window->GetWindow()->Raise();
	}
#endif

    UINT32 width, height;
    GetOuterSize(width, height);
    m_drag_client_x = MIN((UINT32)MAX(0,x), width -1);
    m_drag_client_y = MIN((UINT32)MAX(0,y), height-1);
	
	if (GetOpGadget())
		GetOpGadget()->OnDragStart();
}

void DesktopGadget::OnGadgetDragMove(OpWindowCommander* commander, INT32 x, INT32 y)  //  What's the commander for?
{
	if( m_container_window->HandleDragMove(x-m_drag_client_x, y-m_drag_client_y) )
	{
	    m_drag_requested = FALSE;
		return;
	}
#if 0 // seems like it is no longer used [pobara 2010-02-05]
#if defined(_UNIX_DESKTOP_)
	// Prevents bug #195461
	// It happens because X generates a move event when the window under the mouse has moved
	// and a move is not a synchronous action [espen 2006-04-19].
	OpPoint position = g_input_manager->GetMousePosition();
	if (m_last_mouse_position.x == position.x && m_last_mouse_position.y == position.y )
	{
		return;
	}
	m_last_mouse_position = position;
#endif
#endif // 0
}

void DesktopGadget::OnGadgetDragStop (OpWindowCommander* commander)  //  What's the commander for?
{
	OP_ASSERT(GetOpGadget());
	if( m_container_window->HandleDragStop() )
		return;

	if (GetOpGadget())
		GetOpGadget()->OnDragStop();
}


void DesktopGadget::OnGadgetGetAttention(OpWindowCommander* commander)
{
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationsForWidgets))
	{
		GetOpWindow()->Attention();

#ifdef WEBSERVER_SUPPORT
		BroadcastGetAttention();
#endif // WEBSERVER_SUPPORT
	}
}

class FallbackIconProvider : public GadgetUtils::FallbackIconProvider
{
public:
	FallbackIconProvider(OpGadget* gadget) : m_gadget(gadget) {}
	OP_STATUS GetIcon(Image& icon);
private:
	OpGadget* m_gadget;
};

OP_STATUS FallbackIconProvider::GetIcon(Image& icon)
{
#ifdef WEBSERVER_SUPPORT
	OpSkinElement* element = NULL;
	if (m_gadget && m_gadget->IsSubserver())
		element = g_skin_manager->GetSkinElement("Unite Logo");
	else
#endif // WEBSERVER_SUPPORT
		element = g_skin_manager->GetSkinElement("Widget Icon");
	if (element)
	{
		icon = element->GetImage(0);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}


void DesktopGadget::OnGadgetShowNotification(OpWindowCommander* commander, const uni_char* message, GadgetShowNotificationCallback* callback)
{
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationsForWidgets))
	{
		m_last_notification.Wipe();
		m_last_notification.Set(message);

		OpGadget* op_gadget = GetOpGadget();

		Image notification_image;

		if (op_gadget)
		{
			FallbackIconProvider fallback_provider(op_gadget);
			OpStatus::Ignore(GadgetUtils::GetGadgetIcon(notification_image, op_gadget->GetClass(), 32, 32, 0, 0, FALSE, TRUE, fallback_provider));
		}

		GadgetNotificationCallbackWrapper* wrapper = OP_NEW(GadgetNotificationCallbackWrapper, ());
		wrapper->m_callback = callback;

		OpInputAction* action  = OP_NEW(OpInputAction, (OpInputAction::ACTION_DESKTOP_GADGET_CALLBACK));
		if (!action)
			return;
		action->SetActionData(reinterpret_cast<INTPTR>(wrapper));
		OpInputAction* cancel_action =  OP_NEW(OpInputAction, (OpInputAction::ACTION_DESKTOP_GADGET_CANCEL_CALLBACK));
		if (!cancel_action)
			return;
		cancel_action->SetActionData(reinterpret_cast<INTPTR>(wrapper));

		m_last_notification_valid = TRUE;
		NotificationManager::GetInstance()->ShowNotification(DesktopNotifier::WIDGET_NOTIFICATION, GetTitle(), m_last_notification.CStr(), notification_image,
															 action, cancel_action, FALSE);
	}

}

void DesktopGadget::OnGadgetShowNotificationCancel(OpWindowCommander*)
{
	m_last_notification_valid = FALSE;
}

void DesktopGadget::DoNotificationCallback(OpDocumentListener::GadgetShowNotificationCallback* callback, OpDocumentListener::GadgetShowNotificationCallback::Reply reply)
{
	if (callback && m_last_notification_valid)
		callback->OnShowNotificationFinished(reply);
}


void DesktopGadget::OnTitleChanged(OpWindowCommander* commander, const uni_char* title)
{
	OP_ASSERT(GetOpGadget());
	// now, when DesktopGadget is not toplevel window any more, we want:
	if(GetOpGadget() && !GetOpGadget()->IsSubserver() ) //&& !m_decorated 
	{
		m_container_window->SetTitle(title);
	}
	else
	{
		SetTitle(title);
	}
}


void DesktopGadget::OnPopupMenu(OpDocumentContext* context)
{
	OP_ASSERT(m_window_commander != NULL);

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
	if (m_container_window && m_container_window->HasControlButtons())
	{
		  m_container_window->BlockControlButtons();
	}
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT

	if(context == NULL)
	{
		#ifdef WIDGET_RUNTIME_SUPPORT
			const uni_char* menu_name = UNI_L("Desktop Widget Runtime Menu");
		#else
			const uni_char* menu_name = UNI_L("Desktop Widget Menu");
		#endif

		OpInputAction action(OpInputAction::ACTION_SHOW_POPUP_MENU);
		action.SetActionDataString(menu_name);
		action.SetActionPosition(OpRect()); // This doesn't make much sense, but maybe nobody cares about it
		g_input_manager->SetKeyboardInputContext(this,FOCUS_REASON_OTHER);
	     
		// This is for control buttons, for zoom to work correctly
		VisualDevice *vis_dev = WindowCommanderProxy::GetVisualDevice(m_window_commander);
		if (vis_dev)
		{
			g_input_manager->SetMouseInputContext(vis_dev); 
		}

		g_input_manager->InvokeAction(&action);
	}
	else
	{
		OpAutoPtr<DesktopMenuContext> menu_context(OP_NEW(DesktopMenuContext, ()));
		if (NULL != menu_context.get())
		{
			
			if (OpStatus::IsSuccess(menu_context->Init(m_window_commander, context)))
			{
				const uni_char* menu_name = NULL;
				OpRect rect;
				OpPoint point = context->GetScreenPosition();
#ifdef WIDGET_RUNTIME_SUPPORT
				ResolveGadgetContextMenu(*menu_context, menu_name);
#else // WIDGET_RUNTIME_SUPPORT
				ResolveContextMenu(point, menu_context.get(), menu_name, rect,
						WindowCommanderProxy::GetType(GetWindowCommander()));
#endif // WIDGET_RUNTIME_SUPPORT
				if (InvokeContextMenuAction(point, menu_context.get(), menu_name, rect))
				{
					menu_context.release();
				}
			}
		}
	}
#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
	 if (m_container_window && m_container_window->HasControlButtons())
		{
			  m_container_window->ReleaseControlButtons();
		}
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT

}

void DesktopGadget::OnPopupMenu(OpWindowCommander* commander, OpDocumentContext& context, const OpPopupMenuRequestMessage *)
{
	OnPopupMenu(&context);
}

# ifdef WIC_MIDCLICK_SUPPORT
void DesktopGadget::OnMidClick(OpWindowCommander * commander, OpDocumentContext& context,
								BOOL mousedown,	ShiftKeyState modifiers)
{
	MidClickManager::OnMidClick(commander, context, 0, mousedown, modifiers, TRUE);
}
# endif // WIC_MIDCLICK_SUPPORT


DesktopWindow* DesktopGadget::GetToplevelDesktopWindow()
{
#ifdef WEBSERVER_SUPPORT
	OP_ASSERT(GetOpGadget()); // should never happen
	OP_ASSERT(GetOpGadget() && !GetOpGadget()->IsSubserver()); 	// should never happen
#endif // WEBSERVER_SUPPORT

	return m_container_window;
}


void DesktopGadget::OnShow(BOOL show)
{

	OP_ASSERT(GetOpGadget());

	if (!GetOpGadget()) // should never happen
		return;

	if (show 
#ifdef WEBSERVER_SUPPORT
		&& !GetOpGadget()->IsSubserver()
#endif 
		)
	{
		GetOpGadget()->OnShow();

		// If style is on-top or on-bottom, then keep that. Otherwise use 
		// normal. We have to use SetGadgetStyle to keep behavior in sync
		// with internal setting [espen 2008-03-31]

		if( m_gadget_style == GADGET_STYLE_ALWAYS_ON_TOP || m_gadget_style == GADGET_STYLE_ALWAYS_BELOW )
		{
			GadgetStyle tmp = m_gadget_style;
			m_gadget_style = GADGET_STYLE_NORMAL; // Force transition in SetGadgetStyle(). Needed after OnShow()
			SetGadgetStyle(tmp, FALSE, /*show=*/FALSE); // We cannot allow Show in SetGadgetStyle, it triggers a new OnShow
		}
		else
		{
			SetGadgetStyle(GADGET_STYLE_NORMAL, FALSE, FALSE);
		}
	}
	else
	{
		GetOpGadget()->OnHide();
#ifdef WEBSERVER_SUPPORT		
		// Remember the new style. Non service widgets shall not use the GADGET_STYLE_HIDDEN flag
		if (GetOpGadget()->IsSubserver())
		{
			SetGadgetStyle(GADGET_STYLE_HIDDEN, FALSE, FALSE);
		}
#endif
	}

	//let listeners know we changes something (f.i. the owner)
	BroadcastDesktopWindowChanged();
}


OP_STATUS DesktopGadget::AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info)
{
	// Gadgets and Unite apps are never part of a session.
	return OpStatus::ERR;
}


void DesktopGadget::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{

}


void DesktopGadget::OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	PasswordDialog *dialog = OP_NEW(PasswordDialog, (callback, commander));
	if (dialog)
		dialog->Init(g_application->GetActiveDesktopWindow()/*GetParentDesktopWindow()*/);
}


void DesktopGadget::OnScaleChanged(OpWindowCommander* commander, UINT32 newScale)
{
	OP_ASSERT(m_window_commander != NULL);
	OP_ASSERT(GetOpGadget());

	if (!GetOpGadget()) // Should never happen
		return;
	
	INT32 width  = GetOpGadget()->InitialWidth()  * newScale / 100;
	INT32 height = GetOpGadget()->InitialHeight() * newScale / 100;

	m_scale = newScale;

	SetOuterSize(width, height);
	WindowCommanderProxy::SetVisualDevSize(m_window_commander, width, height);
	m_container_window->Resize(width, height);
}


/***********************************************************************************
**
**	Moving and Resizing
**
***********************************************************************************/

void DesktopGadget::OnInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height)
{
//	OP_ASSERT(m_window_commander != NULL);

	SetInnerSize(width, height);
	m_container_window->Resize(width, height);
	// this is postponed to OnResize, do not uncomment following line
	// WindowCommanderProxy::SetVisualDevSize(m_window_commander, width, height); 
}

void DesktopGadget::OnGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	GetInnerSize(*width, *height);
}

void DesktopGadget::OnOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height)
{
	OP_ASSERT(m_window_commander != NULL);

	SetOuterSize(width, height);
	WindowCommanderProxy::SetVisualDevSize(m_window_commander, width, height);

	m_container_window->Resize(width, height);

}

void DesktopGadget::OnGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	GetOuterSize(*width, *height);
}

void DesktopGadget::OnMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y)
{
	SetOuterPos(x, y);
}

void DesktopGadget::OnGetPosition(OpWindowCommander* commander, INT32* x, INT32* y)
{
	OP_ASSERT(GetOpGadget());
	if(GetOpGadget() && !GetOpGadget()->IsSubserver())
	{
		m_container_window->GetOuterPos(*x,*y);
	}
	else //  this doesn't make difference anyway:
	{
		DesktopWindow::GetOuterPos(*x,*y);
	}
}

void DesktopGadget::GetOuterPos(INT32& x, INT32& y)
{
	OP_ASSERT(GetOpGadget());
	if(GetOpGadget() && !GetOpGadget()->IsSubserver()) 
	{
		m_container_window->GetOuterPos(x,y);
	}
	else
	{
		DesktopWindow::GetOuterPos(x,y);
	}
}

void DesktopGadget::SetOuterPos(INT32 x, INT32 y)
{
	OP_ASSERT(GetOpGadget());
	if(GetOpGadget() && !GetOpGadget()->IsSubserver())
	{
		m_container_window->SetOuterPos(x,y);
	}
	else //  this doesn't make difference anyway:
	{
		DesktopWindow::SetOuterPos(x,y);
	}
}

/***********************************************************************************
**
**	OnAnchorSpecial
**
***********************************************************************************/
/*virtual*/ BOOL
DesktopGadget::OnAnchorSpecial(OpWindowCommander * commander, const AnchorSpecialInfo & info)
{
	return g_application->AnchorSpecial(commander, info);
}

///////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::OnStatusText(OpWindowCommander* commander, const uni_char* text)
{
	if (text)
	{
		SetStatusText(text);
	}
}

#ifdef DOM_GADGET_FILE_API_SUPPORT
// DesktopFileChooserListener
void DesktopGadget::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (m_file_choosing_callback)
	{
		m_file_choosing_callback->OnFilesSelected(&result.files);
	}
	m_file_choosing_callback = 0;
}

// FileSelectionListener
void DesktopGadget::OnDomFilesystemFilesRequest(OpWindowCommander* commander, OpFileSelectionListener::DomFilesystemCallback* callback)
{
	if (!callback || ! commander)
		return;
	else
		m_file_choosing_callback = callback;

	if (!m_chooser)
		RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	switch(callback->GetSelectionType())
	{
	case OpFileSelectionListener::DomFilesystemCallback::OPEN_FILE:
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
		break;
	case OpFileSelectionListener::DomFilesystemCallback::OPEN_MULTIPLE_FILES:
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI;
		break;
	case OpFileSelectionListener::DomFilesystemCallback::SAVE_FILE:
		m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE/*PROMPT_OVERWRITE*/;
		break;
	case OpFileSelectionListener::DomFilesystemCallback::GET_DIRECTORY:
	case OpFileSelectionListener::DomFilesystemCallback::GET_MOUNTPOINT:
		m_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
		break;
	default:
		break;
	}

	m_request.caption.Wipe();

	if (callback->GetCaption())
	{
		m_request.caption.Set(callback->GetCaption());
	}

	m_request.extension_filters.DeleteAll();

	BOOL all_extensions_filter_found = FALSE;

	for (UINT32 idx = 0; callback->GetMediaType(idx); ++idx)
	{
		const OpFileSelectionListener::MediaType* filter = 
				callback->GetMediaType(idx);

		// Omit empty filters.

		if (filter->media_type.IsEmpty() && 
			filter->file_extensions.GetCount() == 0) 
		{
			continue;
		}

		// Check if "all files" filter has been already included.

		if (filter->file_extensions.GetCount() == 1 &&
		 	filter->file_extensions.Get(0)->Compare("*") == 0)
		{
			all_extensions_filter_found = TRUE;
		}

		CopyAddMediaType(filter, &m_request.extension_filters, FALSE);
	}

	// If "all files" filter hasn't been included add it here.

	if (!all_extensions_filter_found)
	{
		OpAutoPtr<OpFileSelectionListener::MediaType> all_files_type(
				OP_NEW(OpFileSelectionListener::MediaType, ()));
		if (NULL == all_files_type.get())
		{
			return;
		}

		OpAutoPtr<OpString> all_files_extension(OP_NEW(OpString, ()));
		if (NULL == all_files_extension.get())
		{
			return;
		}

		RETURN_VOID_IF_ERROR(g_languageManager->GetString(
				Str::SI_IDSTR_ALL_FILES_ASTRIX, all_files_type->media_type));
		RETURN_VOID_IF_ERROR(all_files_extension->Set(UNI_L("*")));

		RETURN_VOID_IF_ERROR(all_files_type->file_extensions.Add(
				all_files_extension.get()));
		all_files_extension.release();

		RETURN_VOID_IF_ERROR(m_request.extension_filters.Add(
				all_files_type.get()));
		all_files_type.release();
	}

	m_request.initial_path.Wipe();

	if (callback->GetInitialPath())
	{
		m_request.initial_path.Set(callback->GetInitialPath());
	}

	OpStatus::Ignore(m_chooser->Execute(GetOpWindow(), this, m_request));
}


void DesktopGadget::OnDomFilesystemFilesCancel(OpWindowCommander* commander)
{
	m_file_choosing_callback = 0;
}
#endif // DOM_GADGET_FILE_API_SUPPORT

void DesktopGadget::OnQueryGoOnline(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback)
{
	//INTEGRATOR NOTE: This has to be implemented asap
	callback->OnDialogReply(DialogCallback::REPLY_YES);
}

void DesktopGadget::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef DOM_GEOLOCATION_SUPPORT
	if(msg == MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK)
	{
		m_permission_listener->ProcessCallback();
	}
#endif 
	DesktopWindow::HandleCallback(msg,par1,par2);
}


/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/
BOOL DesktopGadget::OnInputAction(OpInputAction* action)
{
	DesktopWindow* active_window = g_application->GetActiveDesktopWindow();

	OpWindowCommander* wc = NULL;

	if (active_window)
		wc = active_window->GetWindowCommander();

	if( HandleGadgetActions(action, wc ) )
		return TRUE;

	if( HandleLinkActions(action, wc ) )
		return TRUE;

	if( HandleImageActions(action, wc ) )
		return TRUE;

    return DesktopWindow::OnInputAction(action);
}

BOOL DesktopGadget::HandleGadgetActions(OpInputAction* action, OpWindowCommander* wc )
{
	OP_ASSERT(m_window_commander != NULL);

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SET_WIDGET_STYLE:
				{
					child_action->SetSelected(!SetGadgetStyle((GadgetStyle) child_action->GetActionData(), TRUE));
					return TRUE;
				}
			}

			break;
		}

		case OpInputAction::ACTION_SET_WIDGET_STYLE:
		{
            return SetGadgetStyle((GadgetStyle) action->GetActionData());
		}

		case OpInputAction::ACTION_RELOAD:
		{
			m_window_commander->Reload(FALSE);
			return TRUE;
		}

		case OpInputAction::ACTION_FORCE_RELOAD:
		{
			m_window_commander->Reload(TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_DOWNLOAD_MORE_WIDGETS:
		{
			OpString url_name;
			url_name.Set("http://widgets.opera.com");
			g_op_platform_viewers->OpenInDefaultBrowser(url_name);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL DesktopGadget::HandleLinkActions(OpInputAction* action, OpWindowCommander* wc )
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OPEN_LINK:
		{
			DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext *>(action->GetActionData());
			OpString url_string;
			if (menu_context && menu_context->GetDocumentContext()
				&& OpStatus::IsSuccess(menu_context->GetDocumentContext()->GetLinkData(OpDocumentContext::AddressNotForUI, &url_string)))
			{
				g_op_platform_viewers->OpenInDefaultBrowser(url_string);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_COPY_LINK:
		{
			DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext *>(action->GetActionData());
			OpString url_string;
			if (menu_context && menu_context->GetDocumentContext()
				&& OpStatus::IsSuccess(menu_context->GetDocumentContext()->GetLinkData(OpDocumentContext::AddressNotForUI, &url_string)))
			{
				g_desktop_clipboard_manager->PlaceText(url_string.CStr());
#if defined(_X11_SELECTION_POLICY_)
				// Mouse selection
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), true);
#endif
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_LINK:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				OpDocumentContext* ctx = menu_context->GetDocumentContext();
				if (ctx && ctx->HasLink())
				{
					URLInformation* url_info;
					if (OpStatus::IsSuccess(ctx->CreateLinkURLInformation(&url_info)))
					{
						OP_STATUS status = url_info->DownloadTo(NULL, NULL); // TODO: use second parameter to define download path
						if (OpStatus::IsError(status))
						{
							OP_DELETE(url_info);
						}
						else
						{
							// The URLInformation object owns itself and will take care of everything I think.
						}
					}
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

BOOL DesktopGadget::HandleImageActions(OpInputAction* action, OpWindowCommander* wc )
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_SAVE_IMAGE:
			case OpInputAction::ACTION_COPY_IMAGE:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						enabled = dmctx->GetDocumentContext() ? dmctx->GetDocumentContext()->HasImage() && dmctx->GetDocumentContext()->HasCachedImageData() : FALSE;
					}
					child_action->SetEnabled( enabled );
					return TRUE;
				}
			}
			return FALSE;
		}
		break;

	case OpInputAction::ACTION_SAVE_IMAGE:
		{
			// handled by core
			return TRUE;
		}
	case OpInputAction::ACTION_COPY_IMAGE:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext())
				{
					wc->CopyImageToClipboard(*dmctx->GetDocumentContext());
				}
			}
			return TRUE;
		}
	}

	return FALSE;
}

/***********************************************************************************
**
**	SetGadgetStyle
**
***********************************************************************************/

BOOL DesktopGadget::SetGadgetStyle(GadgetStyle gadget_style, BOOL only_test_if_possible, BOOL show)
{
	OP_ASSERT(GetOpGadget());
#ifdef WEBSERVER_SUPPORT
	if (GetOpGadget() && GetOpGadget()->IsSubserver())
	{
		OP_ASSERT(m_gadget_style == GADGET_STYLE_HIDDEN);
		m_gadget_style = GADGET_STYLE_HIDDEN;

	}
#endif
	//don't run this function if no changes are needed
	if (gadget_style == m_gadget_style)
		return FALSE;

	if (only_test_if_possible)
		return TRUE;

	OpWindow *wnd = m_container_window->GetOpWindow();
	OP_ASSERT(wnd);
	
	//show in the gadget in the taskbar (windows) or windowlist (mac), and not allways on top if it is pinned
	wnd->SetShowInWindowList(gadget_style == GADGET_STYLE_NORMAL);

	//reset (previous) always on top style
	if (m_gadget_style == GADGET_STYLE_ALWAYS_ON_TOP)
		wnd->SetFloating(FALSE);
	//reset (previous) alway below style
	if (m_gadget_style == GADGET_STYLE_ALWAYS_BELOW)
		wnd->SetAlwaysBelow(FALSE);
#ifdef WEBSERVER_SUPPORT
	//reset (previous) hidden style
	if (show && m_gadget_style == GADGET_STYLE_HIDDEN)
		wnd->Show(TRUE);
#endif

	if (gadget_style == GADGET_STYLE_ALWAYS_BELOW)
		wnd->SetAlwaysBelow(TRUE);
	if (gadget_style == GADGET_STYLE_ALWAYS_ON_TOP)
		wnd->SetFloating(TRUE);

#ifdef WEBSERVER_SUPPORT
	if (show && gadget_style == GADGET_STYLE_HIDDEN)
		wnd->Show(FALSE);
#endif

	//remember the new style
	m_gadget_style = gadget_style;

	//let listeners know we changes something (f.i. the owner)
	BroadcastDesktopWindowChanged();

	return TRUE;
}

void DesktopGadget::SetScale(const UINT32 new_scale)
{
	OP_ASSERT(m_window_commander != NULL);
	m_window_commander->SetScale(new_scale);
}

//#define _DEBUG_WEBSERVER_TRANSFER

////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WEBSERVER_SUPPORT
void DesktopGadget::OnNewRequest(WebserverRequest *request)
{
	// If the slow timer is running and a request comes in go directly to the fast timer
	if (m_slow_timer)
	{
		m_slow_timer = FALSE;

		m_traffic_timer.Stop();
		m_traffic_timer.Start(DESKTOP_GADGET_TRAFFIC_TIMEOUT);
	}

	// Increment the fact that a request happened
	if (m_request_count[m_current_request_queue_num] < 0)
		m_request_count[m_current_request_queue_num] = 0;
	m_request_count[m_current_request_queue_num]++;

#if defined(_DEBUG) && defined(_DEBUG_WEBSERVER_TRANSFER)
//	printf("OnNewRequest: %s\n", request->GetOriginalRequest());
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::OnTransferProgressIn(double transfer_rate, UINT bytes_transfered,  WebserverRequest *request, UINT tcp_connections)
{
#if defined(_DEBUG) && defined(_DEBUG_WEBSERVER_TRANSFER)
	printf("Transfer In, rate: %lf, bytes: %u, connections: %u\n", transfer_rate, bytes_transfered, tcp_connections);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::OnTransferProgressOut(double transfer_rate, UINT bytes_transfered, WebserverRequest *request, UINT tcp_connections, BOOL request_finished)
{
#if defined(_DEBUG) && defined(_DEBUG_WEBSERVER_TRANSFER)
	printf("Transfer Out, rate: %lf, bytes: %u, connections: %u, finished: %d\n", transfer_rate, bytes_transfered, tcp_connections, request_finished);
#endif
}

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

static const OpFileLength MAX_QUOTA_INCREASE = 100u * 1024 * 1024;	// 100MB max

void DesktopGadget::OnIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, QuotaCallback *callback)
{
	if(callback)
	{
		OpFileLength *old_quota_size = NULL;
		if(uni_strcmp(db_type, "localstorage") == 0)
		{
			old_quota_size = &m_previous_localstorage_quota;
		}
		else if(uni_strcmp(db_type, "webdatabase") == 0)
		{
			old_quota_size = &m_previous_webdatabases_quota;
		}
		else if(uni_strcmp(db_type, "widgetpreferences") == 0)
		{
			old_quota_size = &m_previous_widgetprefs_quota;
		}
		else
		{
			OP_ASSERT(!"Unknown storage type - add it!");
			callback->OnQuotaReply(FALSE, 0);
			return;
		}
		// if you get asked for the same quota size, it means we've hit the limit.  Due to rounding differences, this is the only way
		// to ensure we've hit it. 
		if(*old_quota_size == current_quota_size)
		{
			callback->OnQuotaReply(FALSE, 0);
		}
		else
		{
			if(current_quota_size > MAX_QUOTA_INCREASE)
			{
				// too large already, reject it (user can still configure higher in prefs)
				callback->OnQuotaReply(FALSE, 0);
			}
			else
			{
				// allow this much
				callback->OnQuotaReply(TRUE, MAX_QUOTA_INCREASE);
			}
			*old_quota_size = current_quota_size;
		}
	}
}
#endif // DATABASE_MODULE_MANAGER_SUPPORT

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::OnTimeOut(OpTimer *timer)
{
#ifdef WEBSERVER_SUPPORT
	// Set the listener if it hasn't been set yet
	SetWebsubServerListener();
#endif // WEBSERVER_SUPPORT

	// Start the timer again for the next collection

	// Check for the slow timer
	if (m_slow_timer)
	{
		// Get the current time
		time_t seconds_since_last_request = g_timecache->CurrentTime() - m_last_request_time;

		// Once the difference between the current time and last requested time exceeds
		// the time at which we write "No Activity" and "No Activity for 15 minutes"
		if (seconds_since_last_request > DESKTOP_GADGET_NOTIFY_TIME_DELAY)
			BroadcastDesktopGadgetActivityChange(m_act_state, seconds_since_last_request);

		// Restart the timer
		m_traffic_timer.Start(DESKTOP_GADGET_TRAFFIC_SLOW_TIMEOUT);
	}
	else
	{
		INT32 total_requests = 0, num_in_queue = 0;

		// Move to the next one in the queue, and don't exceed the queue length
		m_current_request_queue_num = (m_current_request_queue_num + 1) % DESKTOP_GADGET_TRAFFIC_QUEUE_LENGTH;

		// Collect the total number of requests
		for (INT32 i = 0; i < DESKTOP_GADGET_TRAFFIC_QUEUE_LENGTH; i++)
		{
			if (m_request_count[i] > -1)
			{
				total_requests += m_request_count[i];
				num_in_queue++;
			}
		}

		// Process this average
		double requests_per_second = 0.0;
		if (num_in_queue > 0)
			requests_per_second = (double)total_requests / ((double)DESKTOP_GADGET_TRAFFIC_TIMEOUT / 1000.0) / (double)num_in_queue;

#if defined(_DEBUG) && defined(_DEBUG_WEBSERVER_TRANSFER)
		printf("Requests per second: %lf, Num in queue: %d, Curr num: %d, Array: %d,%d,%d,%d,%d, Num requests: %d\n", requests_per_second, num_in_queue,
				m_current_request_queue_num, m_request_count[0], m_request_count[1], m_request_count[2], m_request_count[3], m_request_count[4], total_requests);
#endif

		// Set the count in the next item to no requests
		m_request_count[m_current_request_queue_num] = 0;

		DesktopGadgetActivityState act_state = m_act_state;

		// Magically change requests per second into English
		if (requests_per_second < DESKTOP_GADGET_RPS_LOW)
			m_act_state = DESKTOP_GADGET_LOW_ACTIVITY;
		else if (requests_per_second < DESKTOP_GADGET_RPS_MEDIUM)
			m_act_state = DESKTOP_GADGET_MEDIUM_ACTIVITY;
		else
			m_act_state = DESKTOP_GADGET_HIGH_ACTIVITY;

		// Broadcast the state change to the listners
		if (act_state != m_act_state)
			BroadcastDesktopGadgetActivityChange(m_act_state);

		// If the total requests is 0 then kill the timer, and it will auto restart on the next request
		if (num_in_queue == DESKTOP_GADGET_TRAFFIC_QUEUE_LENGTH && !total_requests)
		{
			// Save the time that this died for the first time
			m_last_request_time = g_timecache->CurrentTime();

			// Set the activity state
			m_act_state = DESKTOP_GADGET_NO_ACTIVITY;
			BroadcastDesktopGadgetActivityChange(m_act_state);

			// Change the speed of the timer
			m_slow_timer = TRUE;
			m_traffic_timer.Start(DESKTOP_GADGET_TRAFFIC_SLOW_TIMEOUT);
		}
		else
		{
			// Start the timer again for the next collection
			m_traffic_timer.Start(DESKTOP_GADGET_TRAFFIC_TIMEOUT);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::BroadcastDesktopGadgetActivityChange(DesktopGadgetActivityState act_state, time_t seconds_since_last_request)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDesktopGadgetActivityStateChanged(act_state, seconds_since_last_request);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::BroadcastGetAttention()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnGetAttention();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadget::SetWebsubServerListener()
{
	if(!m_websubserver_listener_set)
	{
		OpGadget* op_gadget = GetOpGadget();
		OP_ASSERT(op_gadget);
		if (op_gadget)
		{
			unsigned long windows_id = op_gadget->GetWindow()->Id();
			WebSubServer *subserver = g_webserver ? g_webserver->WindowHasWebserverAssociation(windows_id) : NULL;
			// might be NULL if the service hasn't been initialized yet
			if(subserver)
			{
				subserver->AddListener(this);
				m_websubserver_listener_set = TRUE;
			}
		}
	}
}

void DesktopGadget::RemoveWebsubServerListener()
{
	if(m_websubserver_listener_set)
	{
		OpGadget* op_gadget = GetOpGadget();
		OP_ASSERT(op_gadget);
		if (op_gadget && op_gadget->GetWindow())
		{
			unsigned long windows_id = op_gadget->GetWindow()->Id();
			WebSubServer *subserver = g_webserver ? g_webserver->WindowHasWebserverAssociation(windows_id) : NULL;
			// might be NULL if the service hasn't been initialized yet
			if(subserver)
			{
				subserver->RemoveListener(this);
				m_websubserver_listener_set = FALSE;
			}
		}
	}
}

#endif // WEBSERVER_SUPPORT

////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WIC_CAP_UPLOAD_FILE_CALLBACK
BOOL DesktopGadget::OnUploadConfirmationNeeded(OpWindowCommander* commander)
{
# if defined(_KIOSK_MANAGER_)
	if( g_application && KioskManager::GetInstance()->GetEnabled() )
		return FALSE;
	else
		return TRUE;
# endif // _KIOSK_MANAGER_
}
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK


void DesktopGadget::ShowGadget()
{
	Show(TRUE);
	Activate(TRUE);
}


void DesktopGadget::OnResize(INT32 width, INT32 height)
{
	OP_ASSERT(m_window_commander != NULL);
	if( IsInitialized() ) // OnResize may be called before full initialization
	{
		// we shouldn't really do this, but core cannot handle resizing 
		// gadget's visual and layout viewports; see DSK-271207 [pobara 23-01-2010]
		WindowCommanderProxy::SetVisualDevSize(m_window_commander, width, height);
	}
}

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT

void DesktopGadget::OnMouseGadgetEnter(OpWindowCommander* commander)
{
	// this method works as on hover, not on enter, so we will name
	m_container_window->OnDocumentWindowHover();
}


void DesktopGadget::OnMouseGadgetLeave(OpWindowCommander* commander)
{
	m_container_window->OnDocumentWindowLeave();
}


BOOL DesktopGadget::HasControlButtonsEnabled()
{
	// this is REALLY bad way for testing if feature is enabled
	// however, until gadget module offers us better way of detecting
	// features on the fly, we have to use it :(
	// [pobara, bazyl 2010-02-04]
	OP_ASSERT(GetOpGadget());
	if (GetOpGadget() && GetOpGadget()->GetClass()->HasFeature(WCB_FEATURE))
	{
		OpGadgetFeature *f = NULL;
		unsigned int f_count = GetOpGadget()->GetClass()->FeatureCount();
		for (unsigned int i = 0; i < f_count; i++)
		{
			f = GetOpGadget()->GetClass()->GetFeature(i);
			if ((uni_stricmp(WCB_FEATURE, f->URI()) == 0) &&
				(uni_stricmp(f->GetParamValue(WCB_FEATURE_PARAM), UNI_L("false")) == 0))
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}
#endif //WIDGET_CONTROL_BUTTONS_SUPPORT

void DesktopGadget::OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback)
{
	callback->OnAlertDismissed();
}

void DesktopGadget::OnJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, OpDocumentListener::JSDialogCallback *callback)
{
	callback->OnConfirmDismissed(FALSE);
}

void DesktopGadget::OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, OpDocumentListener::JSDialogCallback *callback)
{
	callback->OnPromptDismissed(FALSE, UNI_L(""));
}

#ifdef WIDGETS_UPDATE_SUPPORT
void DesktopGadget::SetUpdateController(WidgetUpdater& ctrl)
{ 
	if(m_container_window)
		m_container_window->SetUpdateController(ctrl);
}

#endif //WIDGETS_UPDATE_SUPPORT

void DesktopGadget::OnHover(OpWindowCommander* commander, const uni_char* url,
		const uni_char* link_title, BOOL is_image)
{
	// One would think it's not possible to be here if we're a Unite app (a
	// hidden window), in which case we don't have a tooltip handler.  But it
	// is possible.
	if (m_tooltip_handler != NULL)
	{
		m_tooltip_handler->ShowTooltip(url, link_title);
	}
}

void DesktopGadget::OnNoHover(OpWindowCommander* commander)
{
	// One would think it's not possible to be here if we're a Unite app (a
	// hidden window), in which case we don't have a tooltip handler.  But it
	// is possible.
	if (m_tooltip_handler != NULL)
	{
		m_tooltip_handler->HideTooltip();
	}
}

#ifdef PLUGIN_AUTO_INSTALL
void DesktopGadget::NotifyPluginDetected(OpWindowCommander* commander, const OpStringC& mime_type)
{
	// If the plug-in auto-installation feature is disabled, we don't need to
	// notify of a detected plug-in.
#	ifdef PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_DETECTED, mime_type, this));
#	endif // PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
}

void DesktopGadget::NotifyPluginMissing(OpWindowCommander* commander, const OpStringC& a_mime_type)
{
	// In case that the plugin auto installation mechanism is disabled for thr Widget Runtime, we
	// don't want to resolve any plugins, so we don't pass the missing plugin notifications from
	// core to platform.
#	ifdef PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_MISSING, a_mime_type, this));
#	endif // PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
}

void DesktopGadget::RequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available)
{
	// No plugins resolved in case there is no auto installation.
#	ifdef PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
	OpStatus::Ignore(g_plugin_install_manager->GetPluginInfo(mime_type, plugin_name, available));
#	endif // PLUGIN_AUTO_INSTALL_FOR_WIDGET_RUNTIME
}

void DesktopGadget::RequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information)
{
	// Nevertheless the auto installation availability, we want to show the Plugin Install Dialog when a placeholder is clicked.
	// Since we don't resolve any plugins if the auto installation is disabled, the dialog will always say "Opera cannot install
	// the plugin automatically", which is exactly what we want.
	OpString tmp_mime_type;
	OpStatus::Ignore(information.GetMimeType(tmp_mime_type));
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_PLACEHOLDER_CLICKED, tmp_mime_type, NULL, NULL, &information));
}
#endif // PLUGIN_AUTO_INSTALL

#endif // GADGET_SUPPORT
