
#ifndef LINKS_PANEL_H
#define LINKS_PANEL_H

#include "adjunct/quick/panels/HotlistPanel.h"

class OpToolbar;
class OpLinksView;

/***********************************************************************************
**
**	LinksPanel
**
***********************************************************************************/

class LinksPanel : public HotlistPanel
{
	public:
			
								LinksPanel() {}
		virtual					~LinksPanel() {}

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Links";}

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		virtual Type			GetType() {return PANEL_TYPE_LINKS;}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Links Panel";}

	private:

		OpLinksView*			m_links_view;
};

#endif // LINKS_PANEL_H
