
#ifndef SEARCH_PANEL_H
#define SEARCH_PANEL_H

#include "adjunct/quick/panels/HotlistPanel.h"

class OpToolbar;

/***********************************************************************************
**
**	SearchPanel
**
***********************************************************************************/

class SearchPanel : public HotlistPanel
{
	public:
			
								SearchPanel() {}
		virtual					~SearchPanel() {}

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Search";}

		virtual void			OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect);
		virtual void			OnLayoutPanel(OpRect& rect) {}
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		virtual Type			GetType() {return PANEL_TYPE_SEARCH;}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Search Panel";}

	private:
};

#endif // SEARCH_PANEL_H
