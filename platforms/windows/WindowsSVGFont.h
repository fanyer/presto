#ifndef WINDOWSSVGFONT_H
#define WINDOWSSVGFONT_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGFont.h"
#include "modules/svg/src/SVGTransform.h"

class OpFont;

class WindowsSVGFont : public SVGSystemFont
{
private:
	static const int	buffer_size;
	SVGTransform 		m_text_transform;
	SVGnumber			m_size;

public:
	WindowsSVGFont(OpFont* font);
	virtual ~WindowsSVGFont() { }
	
	virtual OpBpath*		GetGlyphOutline(uni_char uc);
	virtual SVGnumber 		GetGlyphAdvance(uni_char uc);
	
	virtual void			SetSize(SVGnumber size);
	virtual void			SetWeight(SVGFontWeight weight);
	
	virtual BOOL			HasGlyph(uni_char uc);

	virtual SVGnumber		GetStringWidth(const uni_char* text, INT32 len = -1);
	virtual SVGTransform*	GetTextTransform();
};

#endif // SVG_SUPPORT
#endif // WINDOWSSVGFONT_H
