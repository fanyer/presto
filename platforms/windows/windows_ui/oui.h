
#ifndef OUI_H
#define OUI_H

# include "modules/util/tree.h"

#define OUI_IMPLEMENT_WM_MESSAGE(wmMessage, onMessage)		case wmMessage: {return onMessage(in_WParam, in_LParam);}
#define OUI_DEFAULT_ON_MESSAGE(wmMessage, onMessage)		virtual LRESULT onMessage(WPARAM in_WParam, LPARAM in_LParam) {return DefaultWindowProc(wmMessage, in_WParam, in_LParam);}
#define OUI_DECLARE_ON_MESSAGE(onMessage)					virtual LRESULT onMessage(WPARAM in_WParam, LPARAM in_LParam)

enum DragDropCommand
{
	DRAGDROP_COMMAND_QUERY,
	DRAGDROP_COMMAND_LEAVE,
	DRAGDROP_COMMAND_DROP
};

enum DragDropType
{
	DRAGDROP_TYPE_BOOKMARKS,
	DRAGDROP_TYPE_PAGEBAR_ITEM,
	DRAGDROP_TYPE_WINDOWBAR_ITEM
};

enum OPTB
{
	OPTB_MAIN,
	OPTB_STATUS,
	OPTB_ADDRESS,
	OPTB_WINDOW,
	OPTB_HOTLIST_BOOKMARKS_COLUMNS,
	OPTB_PERSONAL,
	OPTB_PAGE,
	OPTB_MENU,
};

/***********************************************************************************
**
**	Helpers
**
***********************************************************************************/

void EnableDisableDlgItem(HWND hwnd, int id, BOOL enable);
void EnableDlgItem(HWND hwnd, int id);
void DisableDlgItem(HWND hwnd, int id);

void ShowHideDlgItem(HWND hwnd, int id, BOOL show);
void ShowDlgItem(HWND hwnd, int id);
void HideDlgItem(HWND hwnd, int id);

void MoveWindowRect(HWND hwnd, RECT* rect);

/***********************************************************************************
**
**	OUIWindow
**
***********************************************************************************/

class OUIWindow : public Tree
{
	public:

										OUIWindow();
		virtual							~OUIWindow();
								
		static void						StaticRegister
										(
											HINSTANCE		in_Instance
										);


		static OUIWindow*				StaticGetWindowObject
										(
											HWND			in_HWnd
										) {return (OUIWindow*) ::SendMessage(in_HWnd, s_WM_OUI_GETWINDOWOBJECT, 0, 0);};

		virtual HWND					Create
										(
											uni_char*		in_Name, 
											int				in_Style,
											int				in_ExStyle,
											int				in_X,
											int				in_Y,
											int				in_Width,
											int				in_Height,
											HWND			in_Parent,
											HMENU			in_Menu
										);

		virtual void					Destroy();

		inline	void					ShowWindow()			{::ShowWindow(m_HWnd, SW_SHOWNA);};
		inline	void					HideWindow()			{::ShowWindow(m_HWnd, SW_HIDE);};
		inline	void					InvalidateWindow()		{::InvalidateRect(m_HWnd, NULL, FALSE);};
		inline	void					UpdateWindow()			{::UpdateWindow(m_HWnd);};
		inline	void					RedrawWindow()			{InvalidateWindow(); UpdateWindow();};
		inline	void					MoveWindow(RECT* rect)	{MoveWindowRect(m_HWnd, rect);};

		inline	BOOL					SetWindowPos
										(
											HWND			in_InsertAfter,
											int				in_X,
											int				in_Y,
											int				in_Width,
											int				in_Height,
											UINT			in_Flags
										) {return ::SetWindowPos(m_HWnd, in_InsertAfter, in_X, in_Y, in_Width, in_Height, in_Flags);};

		inline LRESULT					SendMessage
										(
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										) {return ::SendMessage(m_HWnd, in_Message, in_WParam, in_LParam);};

		LRESULT							Notify
										(
											int				in_NotifyCode
										);

		inline BOOL						PostMessage
										(
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										) {return ::PostMessage(m_HWnd, in_Message, in_WParam, in_LParam);};

		inline	BOOL					GetClientRect
										(
											LPRECT			in_Rect
										) {return ::GetClientRect(m_HWnd, in_Rect);};

		int								GetWindowWidth();
		int								GetWindowHeight();
		inline int						GetWindowText
										(
											char*			in_Text,
											int				in_MaxCount
										) {return ::GetWindowTextA(m_HWnd, in_Text, in_MaxCount);};

		inline int						GetWindowTextLength()	{return ::GetWindowTextLength(m_HWnd);};
		inline	HWND					GetHWnd()				{return m_HWnd;};
		inline	HWND					GetParentHWnd()			{return ::GetParent(m_HWnd);};
		OUIWindow*						GetParent();

		HWND							SetParent(HWND new_parent) {return ::SetParent(m_HWnd, new_parent);};

		inline	BOOL					IsCreated()				{return m_HWnd ? TRUE : FALSE;};

		BOOL							IsPointInWindow
										(
											int				in_X,
											int				in_Y
										);

		inline void						SetNotifyTarget
										(
											HWND			in_NotifyTarget
										) {m_NotifyTarget = in_NotifyTarget;};

		inline HWND						SetCapture() {return ::SetCapture(m_HWnd);};

	protected:

		static LRESULT CALLBACK			StaticWindowProc
										(
											HWND			in_HWnd,
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										);
										
		virtual LRESULT					DefaultWindowProc
										(
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										) {return ::DefWindowProc(m_HWnd, in_Message, in_WParam, in_LParam);};

		virtual LRESULT					WindowProc
										(
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										)
										{
											if (in_Message == s_WM_OUI_GETWINDOWOBJECT)
											{
												return (LRESULT) this;
											}

											switch (in_Message)
											{
												OUI_IMPLEMENT_WM_MESSAGE(WM_CREATE,				OnCreate)
												OUI_IMPLEMENT_WM_MESSAGE(WM_CLOSE,				OnClose)
												OUI_IMPLEMENT_WM_MESSAGE(WM_SHOWWINDOW,			OnShowWindow)
												OUI_IMPLEMENT_WM_MESSAGE(WM_DESTROY,			OnDestroy)
												OUI_IMPLEMENT_WM_MESSAGE(WM_NCDESTROY,			OnNCDestroy)
												OUI_IMPLEMENT_WM_MESSAGE(WM_COMMAND,			OnCommand)
												OUI_IMPLEMENT_WM_MESSAGE(WM_SIZE,				OnSize)
												OUI_IMPLEMENT_WM_MESSAGE(WM_PAINT,				OnPaint)
												OUI_IMPLEMENT_WM_MESSAGE(WM_GETDLGCODE,			OnGetDlgCode)
												OUI_IMPLEMENT_WM_MESSAGE(WM_GETMINMAXINFO,		OnGetMinMaxInfo)
												OUI_IMPLEMENT_WM_MESSAGE(WM_CHAR,				OnChar)
												OUI_IMPLEMENT_WM_MESSAGE(WM_KEYDOWN,			OnKeyDown)
												OUI_IMPLEMENT_WM_MESSAGE(WM_KILLFOCUS,			OnKillFocus)
												OUI_IMPLEMENT_WM_MESSAGE(WM_SETFOCUS,			OnSetFocus)
												OUI_IMPLEMENT_WM_MESSAGE(WM_SETCURSOR,			OnSetCursor)
												OUI_IMPLEMENT_WM_MESSAGE(WM_ACTIVATE,			OnActivate)
												OUI_IMPLEMENT_WM_MESSAGE(WM_CAPTURECHANGED,		OnCaptureChanged)
												OUI_IMPLEMENT_WM_MESSAGE(WM_CONTEXTMENU,		OnContextMenu)
												OUI_IMPLEMENT_WM_MESSAGE(WM_MEASUREITEM,		OnMeasureItem)
												OUI_IMPLEMENT_WM_MESSAGE(WM_DRAWITEM,			OnDrawItem)
												OUI_IMPLEMENT_WM_MESSAGE(WM_CTLCOLOREDIT,		OnCtlColorEdit)
												OUI_IMPLEMENT_WM_MESSAGE(WM_CTLCOLORBTN,		OnCtlColorBtn)
												OUI_IMPLEMENT_WM_MESSAGE(WM_WINDOWPOSCHANGED,	OnWindowPosChanged)
												OUI_IMPLEMENT_WM_MESSAGE(WM_NCCALCSIZE,			OnNCCalcSize)
												OUI_IMPLEMENT_WM_MESSAGE(WM_ERASEBKGND,			OnEraseBKGnd)										
												OUI_IMPLEMENT_WM_MESSAGE(WM_LBUTTONDOWN,		OnLButtonDown)
												OUI_IMPLEMENT_WM_MESSAGE(WM_MBUTTONDOWN,		OnMButtonDown)
												OUI_IMPLEMENT_WM_MESSAGE(WM_RBUTTONDOWN,		OnRButtonDown)
												OUI_IMPLEMENT_WM_MESSAGE(WM_LBUTTONUP,			OnLButtonUp)
												OUI_IMPLEMENT_WM_MESSAGE(WM_MBUTTONUP,			OnMButtonUp)
												OUI_IMPLEMENT_WM_MESSAGE(WM_RBUTTONUP,			OnRButtonUp)
												OUI_IMPLEMENT_WM_MESSAGE(WM_MOUSEMOVE,			OnMouseMove)
												OUI_IMPLEMENT_WM_MESSAGE(WM_MOUSEWHEEL,			OnMouseWheel)										
												OUI_IMPLEMENT_WM_MESSAGE(WM_TIMER,				OnTimer)
												OUI_IMPLEMENT_WM_MESSAGE(WM_LBUTTONDBLCLK,		OnLButtonDblClk)
												OUI_IMPLEMENT_WM_MESSAGE(WM_PALETTECHANGED,		OnPaletteChanged)
												OUI_IMPLEMENT_WM_MESSAGE(WM_QUERYNEWPALETTE,	OnQueryNewPalette)
											}		

											return DefaultWindowProc(in_Message, in_WParam, in_LParam);
										}
		
										OUI_DEFAULT_ON_MESSAGE(WM_CREATE,			OnCreate);
										OUI_DEFAULT_ON_MESSAGE(WM_CLOSE,			OnClose);
										OUI_DEFAULT_ON_MESSAGE(WM_SHOWWINDOW,		OnShowWindow)
										OUI_DEFAULT_ON_MESSAGE(WM_DESTROY,			OnDestroy);
										OUI_DEFAULT_ON_MESSAGE(WM_PAINT,			OnPaint);
										OUI_DEFAULT_ON_MESSAGE(WM_GETDLGCODE,		OnGetDlgCode);
										OUI_DEFAULT_ON_MESSAGE(WM_GETMINMAXINFO,	OnGetMinMaxInfo);
										OUI_DEFAULT_ON_MESSAGE(WM_SIZE,				OnSize);
										OUI_DEFAULT_ON_MESSAGE(WM_CHAR,				OnChar);
										OUI_DEFAULT_ON_MESSAGE(WM_KEYDOWN,			OnKeyDown);
										OUI_DEFAULT_ON_MESSAGE(WM_KILLFOCUS,		OnKillFocus);
										OUI_DEFAULT_ON_MESSAGE(WM_SETFOCUS,			OnSetFocus);
										OUI_DEFAULT_ON_MESSAGE(WM_SETCURSOR,		OnSetCursor);
										OUI_DEFAULT_ON_MESSAGE(WM_ACTIVATE,			OnActivate);
										OUI_DEFAULT_ON_MESSAGE(WM_COMMAND,			OnCommand);
										OUI_DEFAULT_ON_MESSAGE(WM_CAPTURECHANGED,	OnCaptureChanged);
										OUI_DEFAULT_ON_MESSAGE(WM_CONTEXTMENU,		OnContextMenu);
										OUI_DEFAULT_ON_MESSAGE(WM_MEASUREITEM,		OnMeasureItem);
										OUI_DEFAULT_ON_MESSAGE(WM_DRAWITEM,			OnDrawItem);
										OUI_DEFAULT_ON_MESSAGE(WM_CTLCOLOREDIT,		OnCtlColorEdit);
										OUI_DEFAULT_ON_MESSAGE(WM_CTLCOLORBTN,		OnCtlColorBtn);
										OUI_DEFAULT_ON_MESSAGE(WM_WINDOWPOSCHANGED,	OnWindowPosChanged);
										OUI_DEFAULT_ON_MESSAGE(WM_NCCALCSIZE,		OnNCCalcSize);								
										OUI_DEFAULT_ON_MESSAGE(WM_LBUTTONDOWN,		OnLButtonDown);
										OUI_DEFAULT_ON_MESSAGE(WM_MBUTTONDOWN,		OnMButtonDown);
										OUI_DEFAULT_ON_MESSAGE(WM_RBUTTONDOWN,		OnRButtonDown);
										OUI_DEFAULT_ON_MESSAGE(WM_LBUTTONUP,		OnLButtonUp);
										OUI_DEFAULT_ON_MESSAGE(WM_MBUTTONUP,		OnMButtonUp);
										OUI_DEFAULT_ON_MESSAGE(WM_RBUTTONUP,		OnRButtonUp);
										OUI_DEFAULT_ON_MESSAGE(WM_MOUSEMOVE,		OnMouseMove);
										OUI_DEFAULT_ON_MESSAGE(WM_MOUSEWHEEL,		OnMouseWheel);
										OUI_DEFAULT_ON_MESSAGE(WM_TIMER,			OnTimer);
										OUI_DEFAULT_ON_MESSAGE(WM_LBUTTONDBLCLK,	OnLButtonDblClk);
										OUI_DEFAULT_ON_MESSAGE(WM_PALETTECHANGED,	OnPaletteChanged);
										OUI_DEFAULT_ON_MESSAGE(WM_QUERYNEWPALETTE,	OnQueryNewPalette);

										OUI_DECLARE_ON_MESSAGE(OnNCDestroy);
										OUI_DECLARE_ON_MESSAGE(OnEraseBKGnd);


		static HINSTANCE				s_Instance;
		static UINT						s_WM_OUI_GETWINDOWOBJECT;

		HWND							m_HWnd;
		HWND							m_NotifyTarget;

	private:	
							
};

/***********************************************************************************
**
**	OUISubclassedWindow
**
***********************************************************************************/

class OUISubclassedWindow : public OUIWindow
{
	public:

										OUISubclassedWindow
										(
											HWND			in_HWnd
										);

		virtual							~OUISubclassedWindow();
										
	protected:

		static LRESULT CALLBACK			StaticWindowProc
										(
											HWND			in_HWnd,
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										);
										
		virtual LRESULT					DefaultWindowProc
										(
											UINT			in_Message,
											WPARAM			in_WParam,
											LPARAM			in_LParam
										) {return CallWindowProc(m_OldWindowProc, m_HWnd, in_Message, in_WParam, in_LParam);};

										OUI_DECLARE_ON_MESSAGE(OnNCDestroy);

	private:

		WNDPROC							m_OldWindowProc;
					
};

/***********************************************************************************
**
**	OUIPopupFilterListListener
**
***********************************************************************************/

class OUIPopupFilterList;

class OUIPopupFilterListListener
{
	public:
		
		virtual OP_STATUS				GetColumns
										(
											OUIPopupFilterList*	in_PopupFilterList,
//											SFTTREE_COLUMN_EX**	out_Columns,
											int*				out_ColumnCount,
											int*				out_ResultColumn		// -1 means combine all
										) = 0;

		virtual OP_STATUS				GetItems
										(
											OUIPopupFilterList*	in_PopupFilterList,
											const uni_char*		in_MatchString,
											uni_char***			out_Items,
											int*				out_ItemCount
										) = 0;

};

/***********************************************************************************
**
**	OUIPopupFilterList
**
***********************************************************************************/

class OUIPopupFilterList : public OUIWindow
{
	public:

		enum
		{
			POPUPFILTERLIST_SELCHANGE,
			POPUPFILTERLIST_SELDONE,
			POPUPFILTERLIST_CANCEL,
			POPUPFILTERLIST_TAB,
		};

										OUIPopupFilterList
										(
											OUIPopupFilterListListener*		in_Listener
										);
										
		virtual							~OUIPopupFilterList();

		void							ForwardUpDown(BOOL up);
		BOOL							SetMatchString(const uni_char* matchString);
		uni_char*						GetCurrentItemText();
//		inline int						GetCurrentItem() {return SftTree_GetCurSel(m_Tree->GetHWnd());};
										
	protected:
		
										OUI_DECLARE_ON_MESSAGE(OnCreate);
										OUI_DECLARE_ON_MESSAGE(OnSize);
										OUI_DECLARE_ON_MESSAGE(OnCommand);
										OUI_DECLARE_ON_MESSAGE(OnSetFocus);

	private:							

		class OUIPopupFilterTree: public OUISubclassedWindow
		{
			public:

												OUIPopupFilterTree
												(
													HWND					in_HWnd,
													OUIPopupFilterList*		in_PopupFilterList
												);

			protected:

												OUI_DECLARE_ON_MESSAGE(OnKeyDown);

			private:
				
				OUIPopupFilterList*				m_PopupFilterList;
		};

		OUIPopupFilterListListener*		m_Listener;
		OUIPopupFilterTree*				m_Tree;
		int								m_ColumnCount;
		int								m_ResultColumn;
};


/***********************************************************************************
**
**	OUIDocumentProgressPopup
**
***********************************************************************************/

class OUIDocumentProgressPopup : public OUIWindow
{
	public:

		enum
		{
			FIELD_DOCUMENT	= 0x01,
			FIELD_IMAGES	= 0x02,
			FIELD_TOTAL		= 0x04,
			FIELD_SPEED		= 0x08,
			FIELD_TIME		= 0x10,
			FIELD_STATUS	= 0x20,
			FIELD_ALL		= 0xff
		};
										OUIDocumentProgressPopup
										(
											Window*		i_document_window
										);
										
		virtual							~OUIDocumentProgressPopup();

		void							PlacePopup(RECT* rect, BOOL inside_addressbar);
		void							UpdateProgress(int fields);
								
	protected:
		
										OUI_DECLARE_ON_MESSAGE(OnPaint);
	private:							

		Window*							m_document_window;
		int								m_field_width;
		int								m_field_height;
		BOOL							m_inside_addressbar;
};

/***********************************************************************************
**
**	OUITabCycleWindow
**
***********************************************************************************/

class OUITabCycleWindow : public OUIWindow
{
	public:

										OUITabCycleWindow(HWND mdi_client, BOOL forward);
		virtual							~OUITabCycleWindow();

		void							Cycle(BOOL forward);
		HWND							GetSelected();
								
	protected:
		
										OUI_DECLARE_ON_MESSAGE(OnPaint);
	private:							

		BOOL							GetItemRect(int item, RECT* rect);	

		struct OUITabCycleWindowItem
		{
			OpString	m_title;
			HWND		m_hwnd;
		};

		HWND							m_mdi_client;
		OUITabCycleWindowItem*			m_items;
		int								m_number_of_items;
		int								m_selected_item;
		int								m_item_height;
		int								m_item_width;
		int								m_columns;
		int								m_rows;
};


#endif
