#ifndef MAC_SVGFONT_H
#define MAC_SVGFONT_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGFont.h"
#include "modules/svg/src/SVGTransform.h"
#include "platforms/mac/pi/MacOpFont.h"

class MacOpSVGFont : public SVGSystemFont
{
public:
	MacOpSVGFont(OpFont* font);
	virtual ~MacOpSVGFont();

#ifdef SVG_DEBUG_SHOW_FONTNAMES
	virtual const uni_char*	GetFontName();
#endif

	virtual OpBpath*		GetGlyphOutline(uni_char uc);
	virtual SVGnumber 		GetGlyphAdvance(uni_char uc);
	virtual void			SetSize(SVGnumber size);
	virtual SVGTransform*	GetTextTransform() { return &mTransform; }

private:
	ATSCubicMoveToUPP 			mMoveToUPP;
	ATSCubicLineToUPP 			mLineToUPP;
	ATSCubicCurveToUPP 			mCurveToUPP;
	ATSCubicClosePathUPP 		mClosePathUPP;

	ATSQuadraticNewPathUPP 		mQuadNewPathUPP;
	ATSQuadraticLineUPP 		mQuadLineUPP;
	ATSQuadraticCurveUPP 		mQuadCurveUPP;
	ATSQuadraticClosePathUPP 	mQuadClosePathUPP;

	ATSCurveType				mCurveType;

#ifndef USE_CG_SHOW_GLYPHS
	ATSUGlyphInfoArray*			mGlyphInfoArray;
#endif
	SVGTransform				mTransform;
};

#endif // SVG_SUPPORT
#endif // MAC_SVGFONT_H

