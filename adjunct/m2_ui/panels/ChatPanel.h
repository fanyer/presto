
#ifndef CHAT_PANEL_H
#define CHAT_PANEL_H

#if defined(M2_SUPPORT) && defined(IRC_SUPPORT)

#include "adjunct/quick/panels/HotlistPanel.h"

#include "modules/widgets/OpWidgetFactory.h"

/***********************************************************************************
**
**	ChatPanel
**
***********************************************************************************/

class ChatPanel : public HotlistPanel
{
	public:

								ChatPanel();
		virtual					~ChatPanel() {};

		void					JoinChatRoom(UINT32 account_id, OpString& room);

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Chat";}
		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		virtual Type			GetType() {return PANEL_TYPE_CHAT;}
		BOOL					IsSingleClick() {return !IsFullMode() && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);}

		// OpWidgetListener

		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Chat Panel";}

	private:

		BOOL					ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);

		OpTreeView*				m_rooms_view;
};

#endif //M2_SUPPORT && IRC_SUPPORT

#endif // CHAT_PANEL_H
