#ifndef SVG_DEBUG_SETTINGS
#define SVG_DEBUG_SETTINGS

#if 0 // Output SVG errors to an available console
#define SVG_LOG_ERRORS_RENDER
#endif

#if 0 // Turn on traces in bison parser 
#define SVG_LOG_YYERRORS
#endif

#if 0 // Send errors to stderr in addition to opera console or ecmascript console
#define SVG_ERRORS_TO_STDERR
#endif

#if 0 // Write bitmaps after each filter step / Print filter chain
#define SVG_FILTERS_DEBUG
#endif

#if 0 // Warn about non-parsed objects in svg namespace. Needs "svg" keyword in debug.txt
#define SVG_DEBUG_UNIFIED_PARSER
#endif

#ifdef _DEBUG
extern void DumpRenderTarget(class VEGARenderTarget* rt, const uni_char* filestr);
#endif // _DEBUG

#ifdef _DEBUG
class TmpSingleByteString
{
public:
	TmpSingleByteString() : m_str(NULL) {}
	~TmpSingleByteString() { OP_DELETEA(m_str); }

	void Set(const uni_char* src, unsigned int srclen);

	const char* GetString() const { return m_str ? m_str : ""; }

private:
	char* m_str;
};
#endif // _DEBUG

// Strings to use in debug.txt to get debug information
// svg - general debug keyword that can be used by conditionals in this file.
// svg_text - prints textposition and fontsizes and other misc information
// svg_text_draw - prints each word and its position (SVGRenderer::TxtOut)
// svg_invalid (more info if you uncomment SVG_DEBUG_DIRTYRECTS in svgpch.h)
// svg_bbox - prints information about calculated bounding boxes
// svg_gradient - - prints gradient data and gradientstops
// svg_filter - dumps filter chain (see also define above)
// svg_attributes - prints information about attribute parsing and retreiving from element. currently not used.
// svg_parser - print warnings about failed parsings.
// svg_context - prints warnings about missing (or mismatching type) svg contexts in AttrValueStore::GetSVG*Context()
// svg_animation - print (a lot) of debug information relating to animation, intervals
// svg_lexer - print information about when the lexer reallocates its buffer
// svg_systemfontcache - print some information about the systemfontcache (SVGEmbeddedSystemFont thing)
// svg_intersection - print information about failed rendering in intersection modes
// svg_intersection_miss - print information about subtrees that are cut by dirtyrect optimization
// svg_intersection_hit - print information about intersected subtrees (dirtyrect optimization)
// svg_canvas - print some information about what's done in SetupCanvas
// svg_transform - print information about SVGTransformTraversalObject traverse
// svg_enum - print debug information about the internal enum handling
// svg_opacity - print information regarding the creation/deletion of transparency layers
// svg_probe - print information from svg probes
// svg_strokepath - print how many lines are generated in SVGCanvasVega::CreateStrokePath
// svg_shadowtrees - print information about shadowtree (re)generation
// svg_background - print information about background image handling

#endif // SVG_DEBUG_SETTINGS
