#ifndef OP_FONTCHOOSER_H
#define OP_FONTCHOOSER_H

class DesktopWindow;
class FontAtt;

class OpFontChooserListener
{
public:
	virtual ~OpFontChooserListener() {};

	/**
	 * Returns the chosen font and color
	 */
	virtual void OnFontSelected(FontAtt& font, COLORREF color) = 0;
};

class OpFontChooser
{
public:
	/** Create an OpFontChooser object */
	static OP_STATUS Create(OpFontChooser** new_object);

	virtual ~OpFontChooser() {};

	/**
	 * Shows the font selector and runs OnFontSelected in the listener if a font is selected.
	 * @param initial_font The font we start with when selecting.
	 * @param initial_color The initial color.
	 * @param fontname_only If we don't need size or color, can probably be ignored for some platforms.
	 * @param listener The one who listens for changes shall hear them.
	 * @param parent The DesktopWindow parent, if any.
	 * @return OP_STATUS error code as appropriate.
	 */
	virtual OP_STATUS Show(FontAtt& initial_font, COLORREF initial_color, BOOL fontname_only, OpFontChooserListener* listener, DesktopWindow* parent=NULL) = 0;
};

#endif // OP_FONTCHOOSER_H
