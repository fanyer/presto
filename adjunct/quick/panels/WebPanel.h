
#ifndef WEB_PANEL_H
#define WEB_PANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "adjunct/quick/panels/HotlistPanel.h"

#include "modules/util/adt/opvector.h"

/***********************************************************************************
**
**	WebPanel
**
***********************************************************************************/

class WebPanel : public HotlistPanel, public OpPageListener
{
	public:

								WebPanel(const uni_char* url = NULL, INT32 id = 0, const uni_char* guid = NULL);	
		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage();
		virtual Image			GetPanelIcon();
		virtual void			GetURL(OpString& url);

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void			OnShow(BOOL show);

		virtual Type			GetType() {return PANEL_TYPE_WEB;}
		virtual INT32			GetID() {return m_id;}
		virtual OpStringC		GetGuid() {return m_guid;}
	
		OpBrowserView*			GetBrowserView() {return m_browser_view;}

		void  					UpdateSettings();

		// == OpInputContext ======================

		virtual const char*		GetInputContextName() {return "Web Panel";}
		BOOL					OnInputAction(OpInputAction* action);

		// == OpPageListener ======================

		virtual void			OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user);

	private:

		OpBrowserView*			m_browser_view;
		OpString				m_url;
		OpString				m_guid; // guid of the bookmark
		int						m_id;
		BOOL					m_started;
};

#endif // WEB_PANEL_H
