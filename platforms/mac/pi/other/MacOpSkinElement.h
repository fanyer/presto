#ifndef MAC_OP_SKINELEMENT_H_
#define MAC_OP_SKINELEMENT_H_

#include "modules/skin/OpSkinElement.h"
#include "platforms/mac/util/MachOCompatibility.h"

class MacOpPainter;

/***********************************************************************************
**
**	MacOpSkinElement
**
***********************************************************************************/

class MacOpSkinElement : public OpSkinElement
{
	public:

							MacOpSkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size);
		virtual				~MacOpSkinElement();

		virtual OP_STATUS	Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args);
		virtual OP_STATUS	GetTextColor(UINT32* color, INT32 state);
		virtual OP_STATUS	GetBorderWidth(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetSize(INT32* width, INT32* height, INT32 state);
		virtual OP_STATUS	GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);
		virtual OP_STATUS	GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state);

		virtual BOOL		IsLoaded() {return m_native_type == NATIVE_NOT_SUPPORTED ? OpSkinElement::IsLoaded() : TRUE; }

		static void			Init();
		static void			Free();

		virtual void		OverrideDefaults(OpSkinElement::StateElement* se);

	private:

		// OBS OBS OBS: This enum must be kept in sync with the skin names in s_mac_native_types[]
		enum NativeType
		{
			NATIVE_NOT_SUPPORTED,					// for everyone else
			NATIVE_PUSH_BUTTON,
			NATIVE_PUSH_DEFAULT_BUTTON,
			NATIVE_TOOLBAR_BUTTON,
			NATIVE_SELECTOR_BUTTON,
			NATIVE_MENU_BUTTON,
			NATIVE_HEADER_BUTTON,
			NATIVE_HEADER_SORT_ASC_BUTTON,
			NATIVE_HEADER_SORT_DESC_BUTTON,
			NATIVE_LINK_BUTTON,
			NATIVE_TAB_BUTTON,
			NATIVE_PAGEBAR_BUTTON,
			NATIVE_CAPTION_MINIMIZE_BUTTON,

			NATIVE_CAPTION_CLOSE_BUTTON,
			NATIVE_CAPTION_RESTORE_BUTTON,
			NATIVE_CHECKBOX,
			NATIVE_RADIO_BUTTON,
			NATIVE_DROPDOWN,
			NATIVE_DROPDOWN_BUTTON,
			NATIVE_DROPDOWN_LEFT_BUTTON,
			NATIVE_DROPDOWN_SEARCH,
			NATIVE_DROPDOWN_ZOOM,
			NATIVE_EDIT,
			NATIVE_MULTILINE_EDIT,
			NATIVE_BROWSER,
			NATIVE_PROGRESSBAR,

			NATIVE_TREEVIEW,
			NATIVE_PANEL_TREEVIEW,
			NATIVE_LISTBOX,
			NATIVE_LIST_ITEM,
			NATIVE_CHECKMARK,
			NATIVE_BULLET,
			NATIVE_WINDOW,
			NATIVE_BROWSER_WINDOW,
			NATIVE_DIALOG,
			NATIVE_DIALOG_TAB_PAGE,
			NATIVE_TABS,
			NATIVE_HOTLIST,
			NATIVE_HOTLIST_SELECTOR,
			NATIVE_HOTLIST_SPLITTER,
			NATIVE_HOTLIST_PANEL_HEADER,
			NATIVE_STATUS,
			NATIVE_MAINBAR,

			NATIVE_PERSONALBAR,
			NATIVE_PAGEBAR,
			NATIVE_ADDRESSBAR,
			NATIVE_NAVIGATIONBAR,
			NATIVE_MAILBAR,
			NATIVE_VIEWBAR,
			NATIVE_STATUSBAR,
			NATIVE_CYCLER,
			NATIVE_DIALOG_BUTTON,

// Icons
			NATIVE_BROWSER_ICON,
			NATIVE_DOCUMENT_ICON,
			NATIVE_FOLDER_ICON,
			NATIVE_OPEN_FOLDER_ICON,
			NATIVE_TRASH_ICON,
			//NATIVE_LARGE_TRASH_ICON,
			NATIVE_MAIL_TRASH_ICON,
			NATIVE_BOOKMARK_VISITED_ICON,
			NATIVE_BOOKMARK_UNVISITED_ICON,
			//NATIVE_NEW_PAGE_ICON,
			//NATIVE_OPEN_DOCUMENT_ICON,
			NATIVE_NEW_FOLDER_ICON,

			//NATIVE_GO_TO_PAGE_ICON,
			NATIVE_DIALOG_WARNING,
			NATIVE_DIALOG_ERROR,
			NATIVE_DIALOG_QUESTION,
			NATIVE_DIALOG_INFO,
			NATIVE_EDITABLE_DROPDOWN,
			NATIVE_CHATBAR,
			NATIVE_START,
//			NATIVE_DIALOG_PAGE,
//			NATIVE_DIALOG_HEADER,
//			NATIVE_PAGEBAR_DOCUMENT_LOADING_ICON,

			NATIVE_PROGRESS_DOCUMENT,
			NATIVE_PROGRESS_IMAGES,
			NATIVE_PROGRESS_TOTAL,
			NATIVE_PROGRESS_SPEED,
			NATIVE_PROGRESS_ELAPSED,
			NATIVE_PROGRESS_STATUS,
			NATIVE_PROGRESS_GENERAL,
			NATIVE_PROGRESS_CLOCK,
			NATIVE_PROGRESS_EMPTY,
			NATIVE_PROGRESS_FULL,
			NATIVE_IDENTIFY_AS,
/*
			NATIVE_SCROLLBAR_HORIZONTAL,
			NATIVE_SCROLLBAR_HORIZONTAL_KNOB,
			NATIVE_SCROLLBAR_HORIZONTAL_LEFT,
			NATIVE_SCROLLBAR_HORIZONTAL_RIGHT,
			NATIVE_SCROLLBAR_VERTICAL,
			NATIVE_SCROLLBAR_VERTICAL_KNOB,
			NATIVE_SCROLLBAR_VERTICAL_UP,
			NATIVE_SCROLLBAR_VERTICAL_DOWN,
*/
			NATIVE_DISCLOSURE_TRIANGLE,
			NATIVE_TOOLTIP
		};

		NativeType			m_native_type;
		OpBitmap*			m_icon;
		ThemeButtonDrawInfo	m_prev_drawinfo;

		static OpBitmap*	sBrowserIcon;
		static OpBitmap*	sDocumentIcon;
		static OpBitmap*	sFolderIcon;
		static OpBitmap*	sTrashIcon;
		static OpBitmap*	sLargeTrashIcon;
		static OpBitmap*	sLargeFullTrashIcon;
		static OpBitmap*	sBookmarkIcon;
		//static OpBitmap*	sNewPageIcon;
		//static OpBitmap*	sOpenDocumentIcon;
		//static OpBitmap*	sNewFolderIcon;
		static OpBitmap*	sWarningIcon;
		static OpBitmap*	sErrorIcon;
		static OpBitmap*	sQuestionIcon;
		static OpBitmap*	sInfoIcon;
		
		OSStatus PainterHIThemeDrawTabPane(CGContextRef context, const HIRect * inRect, const HIThemeTabPaneDrawInfo * inDrawInfo, HIThemeOrientation inOrientation);
		OSStatus PainterHIThemeDrawBackground(CGContextRef context, const HIRect * inBounds, const HIThemeBackgroundDrawInfo * inDrawInfo, HIThemeOrientation inOrientation);
		OSStatus PainterHIThemeDrawTab(CGContextRef context, const HIRect * inRect, const HIThemeTabDrawInfo * inDrawInfo, HIThemeOrientation inOrientation);
	
		OSStatus PainterDrawThemePopupArrow(CGContextRef context, const HIRect& inRect, ThemeArrowOrientation orientation, ThemePopupArrowSize size, ThemeDrawState state);
		OSStatus PainterDrawThemeEditTextFrame(CGContextRef context, const HIRect& inRect, ThemeDrawState inState);
		OSStatus PainterDrawThemeListBoxFrame(CGContextRef context, const HIRect& inRect, ThemeDrawState inState);
		OSStatus PainterDrawThemeTabPane(CGContextRef context, const HIRect& inRect, ThemeDrawState inState);
		OSStatus PainterDrawThemeTab(CGContextRef context, const HIRect& inRect, ThemeTabStyle inStyle, ThemeTabDirection inDirection, int index, int ntabs, INT32 skin_state);
		OSStatus PainterDrawThemeWindowHeader(CGContextRef context, const HIRect& inRect, ThemeDrawState inState);
		OSStatus PainterDrawThemePlacard(CGContextRef context, const HIRect& inRect, ThemeDrawState inState);
		OSStatus PainterDrawThemePrimaryGroup(CGContextRef context, const HIRect& inRect, ThemeDrawState inState);
		OSStatus PainterDrawThemeButton(CGContextRef context, const HIRect& inRect, ThemeButtonKind inKind, const ThemeButtonDrawInfo * inNewDrawInfo, const ThemeButtonDrawInfo * inPrevInfo, HIThemeOrientation orientation = kHIThemeOrientationNormal);
		OSStatus PainterDrawThemeTrack(CGContextRef context, const ThemeTrackDrawInfo * drawInfo);
		OSStatus PainterDrawThemeFocusRect(CGContextRef context, const HIRect& inRect, Boolean inHasFocus);
		OSStatus PainterDrawThemeGenericWell(CGContextRef context, const HIRect& inRect, const ThemeButtonDrawInfo * inNewDrawInfo);
		ThemeButtonKind DetermineDropdownButtonKind(OpWidget* w, CGRect& r);
};

#endif // !MAC_OP_SKINELEMENT_H_
