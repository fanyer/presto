#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_USE_PLATFORM_FACTORY

// Make sure the destructor is called when deleting
#include "modules/pi/OpBitmap.h"

#include "modules/svg/src/SVGFactory.h"
#include "modules/svg/src/SVGCanvasVega.h"
#include "WindowsSVGFont.h"

class WindowsSVGFactory : public SVGFactory
{
	OP_STATUS CreateSVGCanvas(SVGCanvas** new_canvas)
	{
		*new_canvas = new SVGCanvasVega();
		return *new_canvas ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	
	OP_STATUS CreateSVGFont(SVGFont** new_font, OpFont* font)
	{
		*new_font = new WindowsSVGFont(font);
		return *new_font ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
};

/** Static function to create the OpFactory with. */
SVGFactory *SVGFactory::Create()
{
	return new WindowsSVGFactory;
}
#endif // SVG_USE_PLATFORM_FACTORY
#endif // SVG_SUPPORT
