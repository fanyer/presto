#ifndef _PROCESSED_STRING_H_
#define _PROCESSED_STRING_H_

/*
  this is part of the "public api" to the extent that VEGAOpPainter
  might use it, even when not using mdefont to load or rasterize
  glyphs of fonts.
 */

/**
   a helper struct for ProcessedString, stores glyph id (_not_ unicode
   codepoint) and position (relative to baseline and start of string).
 */
struct ProcessedGlyph
{
	/**
	   placement of glyph, relative to baseline, start of
	   string. should not be adjusted with top, left (since this
	   may require the glyph to be rasterized) - this should
	   instead be done when drawing.
	*/
	OpPoint m_pos;
	UINT32 m_id;
};
/**
   a struct for processed text strings - used for mesuring/drawing
   text. provides string advance, as well as individual glyph
   placement.
 */
struct ProcessedString
{
	size_t m_length; ///< (output) string length, in glyphs
	INT32 m_advance; ///< sum of advance of individual glyphs
	ProcessedGlyph* m_processed_glyphs; ///< handle to individual glyphs

    /**
       if TRUE, m_id in m_proceesed_glyphs is glyph indices, if FALSE
       unicode codepoints. translation from unicode codepoints to
       glyph indices is not free, so we try to keep the characters as
       unicode codepoints. this is not possible for operations that
       work on glyph indices - eg kerning and opentype processing.
     */
    BOOL m_is_glyph_indices;

    /**
       if TRUE, the glyphs in the processed string has had their
       position adjusted with top and left.
       if FALSE, positions represent the accumulated advance but does
       NOT include top and left adjustment of individual glyphs.
     */
    BOOL m_top_left_positioned;
};

/**
   adjusts width of processed string to word_width
 */
BOOL AdjustToWidth(ProcessedString& processed_string, const short word_width);


#endif // _PROCESSED_STRING_H_
