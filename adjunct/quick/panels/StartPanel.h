
#ifndef START_PANEL_H
#define START_PANEL_H

#include "adjunct/quick/panels/HotlistPanel.h"

class OpToolbar;

/***********************************************************************************
**
**	StartPanel
**
***********************************************************************************/

class StartPanel : public HotlistPanel
{
	public:
			
								StartPanel() {}
		virtual					~StartPanel() {}

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Start";}

		virtual void			OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect);
		virtual void			OnLayoutPanel(OpRect& rect) {}
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual	void			OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

		virtual Type			GetType() {return PANEL_TYPE_START;}

		virtual void			OnClick(OpWidget *object, UINT32 id);

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Start Panel";}

	private:
};

#endif // START_PANEL_H
