
#ifndef CONTACTSPANEL_H
#define CONTACTSPANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/util/adt/opvector.h"


#include "adjunct/quick/panels/HotlistPanel.h"

#include "modules/widgets/OpWidget.h"

#ifdef MSWIN
#include "platforms/windows/windows_ui/oui.h"
#endif

class OpToolbar;
class OpLabel;
class OpHotlistView;

/***********************************************************************************
**
**	ContactsPanel
**
***********************************************************************************/

class ContactsPanel : public HotlistPanel
{
	public:
			
								ContactsPanel();
		virtual					~ContactsPanel();

		OpHotlistView*			GetHotlistView() const {return m_hotlist_view;}

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Contacts";}

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);

		virtual Type			GetType() {return PANEL_TYPE_CONTACTS;}

		// == OpInputContext ======================

		INT32 		GetSelectedFolderID();
		void SetSelectedItemByID(INT32 id,
					 BOOL changed_by_mouse = FALSE,
					 BOOL send_onchange = TRUE,
					 BOOL allow_parent_fallback = FALSE)
		  {
		    m_hotlist_view->SetSelectedItemByID(id, changed_by_mouse, send_onchange, allow_parent_fallback);
		  }


		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Contacts Panel";}

		virtual void			OnAdded();
		virtual void			OnRemoving();
		virtual void			OnShow(BOOL show);

	private:

		OpHotlistView*			m_hotlist_view;
};

#endif // CONTACTSPANEL_H
