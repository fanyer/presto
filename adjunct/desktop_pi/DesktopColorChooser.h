#ifndef OP_COLORCHOOSER_H
#define OP_COLORCHOOSER_H

class DesktopWindow;

class OpColorChooserListener
{
public:
	virtual ~OpColorChooserListener() {};

	/**
	 * Returns the chosen font and color
	 */
	virtual void OnColorSelected(COLORREF color) = 0;
};

class OpColorChooser
{
public:
	/** Create an OpColorChooser object, see OpColorChooser.h */
	static OP_STATUS Create(OpColorChooser** new_object);

	virtual ~OpColorChooser() {};

	/**
	 * Shows the color selector and runs OnColorSelected in the listener if a color is selected
	 */
	virtual OP_STATUS Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent=NULL) = 0;
};

#endif // OP_COLORCHOOSER_H
