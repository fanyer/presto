#include "core/pch.h"

#include "adjunct/quick/widgets/OpTrashBinButton.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/sessions/opsession.h"

OpPointerHashTable<DesktopWindow , OpVector<OpTrashBinButton> > OpTrashBinButton::m_trash_bin_collection;

OP_STATUS
OpTrashBinButton::Construct(OpTrashBinButton** obj) 
{
	*obj = NULL;

	OpAutoPtr<OpTrashBinButton> tmp_obj(OP_NEW(OpTrashBinButton,())); 
	if (tmp_obj.get() == NULL) 
	{
		return OpStatus::ERR_NO_MEMORY; 
	}

	if (OpStatus::IsError(tmp_obj->init_status) || OpStatus::IsError(tmp_obj->Init()))
	{
		return tmp_obj->init_status; 
	}
	
	*obj = tmp_obj.release();
	return OpStatus::OK;
}

void
OpTrashBinButton::Cleanup()
{
	if (g_main_message_handler->HasCallBack(this, MSG_QUICK_ANIMATE_TRASH_BIN,  reinterpret_cast<MH_PARAM_1>(m_bdw)))
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_QUICK_ANIMATE_TRASH_BIN, reinterpret_cast<MH_PARAM_1>(m_bdw), 0);
		g_main_message_handler->UnsetCallBack(this, MSG_QUICK_ANIMATE_TRASH_BIN, reinterpret_cast<MH_PARAM_1>(m_bdw));
	}
	if (g_application)
	{
		g_application->GetDesktopWindowCollection().RemoveListener(this);
	}
	OpVector<OpTrashBinButton> *vec=NULL;
	if (OpStatus::IsSuccess(m_trash_bin_collection.GetData(m_bdw, &vec)))
	{
		OpStatus::Ignore(vec->RemoveByItem(this));

		if (vec->GetCount() == 0)
		{
			OpStatus::Ignore(m_trash_bin_collection.Remove(m_bdw, &vec));
			OP_DELETE(vec);
		}
	}
}

OP_STATUS
OpTrashBinButton::Init()
{
	init_status = g_application->GetDesktopWindowCollection().AddListener(this);
	if (OpStatus::IsError(init_status))
	{
		return init_status; 
	}

	BrowserDesktopWindow *bdw_last_incollection =  m_bdw = g_application->GetActiveBrowserDesktopWindow();
	
	OpVector<DesktopWindow> browser_windows;
	if (OpStatus::IsSuccess(g_application->GetDesktopWindowCollection().GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, browser_windows)))
	{
		bdw_last_incollection = static_cast<BrowserDesktopWindow*>(browser_windows.Get(browser_windows.GetCount()-1));
	}
	
	if ( m_bdw != bdw_last_incollection)
	{
		 m_bdw = bdw_last_incollection;
	}

	OP_ASSERT(m_bdw);

	init_status = g_main_message_handler->SetCallBack(this, MSG_QUICK_ANIMATE_TRASH_BIN, reinterpret_cast<MH_PARAM_1>(m_bdw));
	if (OpStatus::IsError(init_status))
	{
		g_application->GetDesktopWindowCollection().RemoveListener(this);
		return init_status; 
	}

	OpVector<OpTrashBinButton> *vec = NULL;
	init_status = m_trash_bin_collection.GetData(m_bdw, &vec);
	if (!vec && OpStatus::IsError(init_status))
	{
		OpAutoPtr< OpVector<OpTrashBinButton> >tmp_vec(OP_NEW(OpVector<OpTrashBinButton>, ()));
		if (tmp_vec.get() == NULL)
		{
			g_application->GetDesktopWindowCollection().RemoveListener(this);
			return OpStatus::ERR_NO_MEMORY;
		}

		init_status = m_trash_bin_collection.Add(m_bdw, tmp_vec.get());
		if (OpStatus::IsError(init_status))
		{
			g_application->GetDesktopWindowCollection().RemoveListener(this);
			return init_status; 
		}
		 
		vec = tmp_vec.release();
	}

	OpStatus::Ignore(init_status = vec->Add(this)); //No return check is needed here. 
		
	return OpStatus::OK;
}

void
OpTrashBinButton::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{ 
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_BUTTON, w, h, cols, rows); 
}

void
OpTrashBinButton::OnDesktopWindowRemoved(DesktopWindow* window)
{
	if (window == m_bdw)
		return;

	BOOL is_session_wnd = FALSE;
	OpVector<OpTrashBinButton> *vec;
	if (window->GetType() == WINDOW_TYPE_BROWSER && OpStatus::IsSuccess(m_trash_bin_collection.Remove(window, &vec)))
	{
		is_session_wnd = TRUE;
		OP_DELETE(vec);
	}

	is_session_wnd = is_session_wnd == FALSE ? m_bdw->IsSessionUndoWindow(window) && window->GetParentDesktopWindow() : is_session_wnd;
	if (is_session_wnd)
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_QUICK_ANIMATE_TRASH_BIN, reinterpret_cast<MH_PARAM_1>(m_bdw), static_cast<MH_PARAM_2>(0));
		g_main_message_handler->PostDelayedMessage(MSG_QUICK_ANIMATE_TRASH_BIN, reinterpret_cast<MH_PARAM_1>(m_bdw), static_cast<MH_PARAM_2>(0), 0);
	}
}

void
OpTrashBinButton::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (IsVisible() && msg == MSG_QUICK_ANIMATE_TRASH_BIN && m_bdw == reinterpret_cast<BrowserDesktopWindow*>(par1))
	{
		if (g_application->GetActiveBrowserDesktopWindow() == m_bdw)
		{
			GetForegroundSkin()->StartAnimation(FALSE); //Stop ongoing animation
			GetForegroundSkin()->StartAnimation(TRUE, TRUE);
			InvalidateAll();
		}
	}
	else
	{
		//We didn't recognize the message, pass it on to default handler
		OpButton::HandleCallback(msg, par1, par2);
	}
}
