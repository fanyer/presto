
#ifndef BOOKMARKSPANEL_H
#define BOOKMARKSPANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "modules/util/adt/opvector.h"

#include "adjunct/quick/panels/HotlistPanel.h"
#include "adjunct/quick/widgets/OpBookmarkView.h"

class OpToolbar;
class OpLabel;

/***********************************************************************************
 **
 **	BookmarksPanel
 **
 ***********************************************************************************/

class BookmarksPanel : public HotlistPanel
{
	public:
  
  BookmarksPanel();
  virtual	        ~BookmarksPanel();
  
  OpBookmarkView*       GetBookmarkView() const { return m_bookmark_view; }
  
  virtual OP_STATUS	Init();
  virtual void		GetPanelText(OpString& text, 
				     Hotlist::PanelTextType text_type);
  virtual const char*	GetPanelImage() {return "Panel Bookmarks";}
    
  virtual void		OnLayoutPanel(OpRect& rect);
  virtual void		OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect);
  virtual void		OnFullModeChanged(BOOL full_mode);
  virtual void		OnFocus(BOOL focus,
				FOCUS_REASON reason);
  
  // Implementing the OpTreeModelItem interface
  virtual Type		GetType() {return PANEL_TYPE_BOOKMARKS;}
  
  /**
   * Set the search text in the quick find field of the associated treeview
   * @param search text to be set
   */
  virtual void 		SetSearchText(const OpStringC& search_text);

  // OpWidgetListener
  
  // == OpInputContext ======================
  
  virtual const char*	GetInputContextName() {return "Bookmarks Panel";}
  virtual BOOL		OnInputAction(OpInputAction* action);

	// OpWidget
  virtual void OnAdded();
  virtual void OnRemoving();
  virtual void OnShow(BOOL show);

  void SetSelectedItemByID(INT32 id,
			   BOOL changed_by_mouse = FALSE,
			   BOOL send_onchange = TRUE,
			   BOOL allow_parent_fallback = FALSE)
    {
      m_bookmark_view->SetSelectedItemByID(id, changed_by_mouse, send_onchange, allow_parent_fallback);
    }


  HotlistModelItem*	GetSelectedItem() { return m_bookmark_view->GetSelectedItem(); }

  INT32 		GetSelectedFolderID();

  BOOL                  OpenUrls( const OpINT32Vector& index_list, 
				  BOOL3 new_window, 
				  BOOL3 new_page, 
				  BOOL3 in_background, 
				  DesktopWindow* target_window = NULL, 
				  INT32 position = -1, 
				  BOOL ignore_modifier_keys = FALSE);

  
 private:

  OpBookmarkView*       m_bookmark_view;
};

#endif // BOOKMARKSPANEL_H
