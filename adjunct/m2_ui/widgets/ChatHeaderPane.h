#ifndef CHAT_HEADER_PANE_H
#define CHAT_HEADER_PANE_H

#ifdef M2_SUPPORT

#  include "adjunct/quick_toolkit/widgets/OpGroup.h"
#  include "adjunct/quick_toolkit/widgets/OpToolbar.h"

class OpWebImageWidget;
/***********************************************************************************
**
**	ChatHeaderPane
**
***********************************************************************************/

class ChatHeaderPane : public OpGroup
{
	public:
					ChatHeaderPane(const char* skin);

        INT32		GetHeightFromWidth(INT32 width);

		void		SetHeaderToolbar(const char* header_toolbar_name);
		void		SetToolbar(const char* standard_toolbar_name);
		void		SetImage(const char* skin_image, const uni_char* url_image);

		void		SetHeaderToolbarVisibility(BOOL visibility) { m_header_toolbar->SetAlignment(visibility ? OpBar::ALIGNMENT_TOP : OpBar::ALIGNMENT_OFF); }

		// OpWidget hooks

		void		OnLayout();
		void		OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

	private:

		void		GetImageRect(OpRect& rect);

		OpWebImageWidget*		m_header_webimage;
		BOOL					m_has_header_webimage;
		OpToolbar*				m_header_toolbar;
		OpToolbar*				m_toolbar;
};

#endif // M2_SUPPORT
#endif // CHAT_HEADER_PANE_H
