
#ifndef WINDOWS_PANEL_H
#define WINDOWS_PANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/inputmanager/inputaction.h"

#include "adjunct/quick/panels/HotlistPanel.h"

#include "modules/util/adt/opvector.h"
#include "modules/widgets/OpWidget.h"

class OpToolbar;
class OpWindowList;

/***********************************************************************************
**
**	WindowsPanel
**
***********************************************************************************/

class WindowsPanel : public HotlistPanel
{
	public:
			
								WindowsPanel();
		virtual					~WindowsPanel() {};

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Windows";}

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		virtual Type			GetType() {return PANEL_TYPE_WINDOWS;}

		// == OpInputContext ======================

		virtual const char*		GetInputContextName() {return "Windows Panel";}
		virtual BOOL			OnInputAction(OpInputAction* action);

	private:

		OpWindowList*			m_windows_view;
};

#endif // WINDOWS_PANEL_H
