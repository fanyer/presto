/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef TRUE_TYPE_CONVERTER_H
#define TRUE_TYPE_CONVERTER_H

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/postscriptdictionary.h"
#include "modules/libvega/src/postscript/sfntdatatypes.h"
#include "modules/display/sfnt_base.h"

/** @brief Parser for TrueType cmap table data
  */


class PostScriptCommandBuffer;


//-------------------------------------------------
// SFNT offset table
//-------------------------------------------------
class TrueTypeOffsetTable
{
public:
	TrueTypeOffsetTable() : sfnt_version(0), number_of_tables(0), search_range(0), entry_selector(0), range_shift(0) {}

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

	AutoDeleteHead* getTableRecordList() { return &table_record_list; }
	bool checkSFNTTableWanted(UINT32 tag);
	bool checkSFNTTableCopied(UINT32 tag);

	UINT16 getNumberOfTables() { return number_of_tables; }
	FontType getFontType() { return font_type; }

private:
	TrueTypeOffsetTable(const TrueTypeOffsetTable &g);
	TrueTypeOffsetTable& operator=(const TrueTypeOffsetTable&);

	OP_STATUS readTableRecords(const UINT8* data, const UINT32 table_length);

	// Offset table values
	UINT32 sfnt_version;
	UINT16 number_of_tables;
	UINT16 search_range;
	UINT16 entry_selector;
	UINT16 range_shift;
	FontType font_type;

	AutoDeleteHead table_record_list;
};



//-------------------------------------------------
// SFNT head
//-------------------------------------------------
class TrueTypeHeadTable
{
public:
	TrueTypeHeadTable();

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

	OP_STATUS getFontBBox(OpString8& font_bbox);
	long convertUnit(INT16 value) const { return value * 1000L / units_per_em; }
	bool isBold();
	bool isItalic();

	bool indexToLocFormatIsLong() { return index_to_loc_format_is_long; }
	SFNTDataTypes::Fixed getTableVersion() { return table_version; }
	SFNTDataTypes::Fixed getFontRevision() { return font_revision; }
	void setMaxBoundingBox(INT16 _x_min, INT16 _y_min, INT16 _x_max, INT16 _y_max) { x_min = _x_min; y_min = _y_min; x_max = _x_max; y_max = _y_max; }

private:
	TrueTypeHeadTable(const TrueTypeHeadTable &g);
	TrueTypeHeadTable& operator=(const TrueTypeHeadTable&);

	SFNTDataTypes::Fixed table_version;
	SFNTDataTypes::Fixed font_revision;
	UINT32 checksum_adjustment;
	UINT32 magic_number;
	UINT16 flags;
	UINT16 units_per_em;

	UINT32 created_hi32;
	UINT32 created_lo32;
	UINT32 modified_hi32;
	UINT32 modified_lo32;

	INT16 x_min;
	INT16 y_min;
	INT16 x_max;
	INT16 y_max;

	UINT16 mac_format;
	UINT16 lowest_rec_ppem;
	INT16 font_direction_hint;

	bool index_to_loc_format_is_long;
	INT16 glyph_data_format;
};


//-------------------------------------------------
// SFNT hhea
//-------------------------------------------------
class TrueTypeHheaTable
{
public:
	TrueTypeHheaTable() {}

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

	void setNumberOfHMetrics(UINT16 number_of_h_metrics) { this->number_of_h_metrics = number_of_h_metrics; }
	UINT16 getNumberOfHMetrics() { return number_of_h_metrics; }
	void setMaxAdvanceWidth(UINT16 value) { advance_width_max = value; }
	void setMinLeftSideBearing(INT16 value) { min_left_side_bearing = value; }

private:
	TrueTypeHheaTable(const TrueTypeHheaTable &g);
	TrueTypeHheaTable& operator=(const TrueTypeHheaTable&);

	INT16 ascender;
	INT16 descender;
	INT16 line_gap;
	UINT16 advance_width_max;
	INT16 min_left_side_bearing;
	INT16 min_right_side_bearing;
	INT16 x_max_extent;
	INT16 caret_slope_rise;
	INT16 caret_slope_run;
	INT16 caret_offset;
	INT16 metric_data_format;
	UINT16 number_of_h_metrics;
};


//-------------------------------------------------
// SFNT maxp
//-------------------------------------------------
class TrueTypeMaxpTable
{
public:
	TrueTypeMaxpTable() : table_version(0), num_glyphs(0) {}

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

	UINT16 getNumberOfGlyphs() { return num_glyphs; }
	void setNumberOfGlyphs(UINT16 value) { num_glyphs = value; }
	void setMaxContours(UINT16 value) { max_contours = value; }

private:
	TrueTypeMaxpTable(const TrueTypeMaxpTable &g);
	TrueTypeMaxpTable& operator=(const TrueTypeMaxpTable&);

	UINT32 table_version;
	UINT16 num_glyphs;
	UINT16 max_points;
	UINT16 max_contours;
	UINT16 max_composite_points;
	UINT16 max_composite_contours;
	UINT16 max_zones;
	UINT16 max_twilight_points;
	UINT16 max_storage;
	UINT16 max_function_defs;
	UINT16 max_instrustion_defs;
	UINT16 max_stack_elements;
	UINT16 max_size_of_instructions;
	UINT16 max_component_elements;
	UINT16 max_component_depth;
};




//-------------------------------------------------
// SFNT hmtx
//-------------------------------------------------
class TrueTypeHmtxTable
{
public:
	TrueTypeHmtxTable() : num_glyphs(0), max_advance_width(0), min_left_side_bearing(0x7fff) {}

	UINT32 getLength();
	UINT32 getChecksum();

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length, UINT16 number_of_glyphs, UINT16 number_of_hmetrics);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);

	OP_STATUS addGlyphHMetrics(UINT32 glyph_index, UINT16 advance_width, INT16 left_side_bearing);
	OP_STATUS getGlyphHMetrics(UINT32 glyph_index, UINT16& advance_width, INT16& left_side_bearing);
	UINT32 getNumberOfHMetrics() { return glyph_hmetrics_vector.GetCount(); }
	
	UINT16 getMaxAdvanceWidth() { return max_advance_width; }
	INT16 getMinLeftSideBearing() { return min_left_side_bearing; }

private:
	TrueTypeHmtxTable(const TrueTypeHmtxTable &g);
	TrueTypeHmtxTable& operator=(const TrueTypeHmtxTable&);

	class GlyphHMetrics
	{
	public:
		GlyphHMetrics(UINT16 glyph_index, UINT16 advance_width, INT16 left_side_bearing) : 
				glyph_index(glyph_index), advance_width(advance_width), left_side_bearing(left_side_bearing) {}
		~GlyphHMetrics() {}
	
		UINT16 glyph_index;
		UINT16 advance_width;
		INT16 left_side_bearing;
	};

	UINT16 num_glyphs;
	OpAutoVector<GlyphHMetrics> glyph_hmetrics_vector;

	UINT16 max_advance_width;
	INT16 min_left_side_bearing;
};


//-------------------------------------------------
// SFNT loca
//-------------------------------------------------
class TrueTypeLocaTable
{
public:
	class Glyph
	{
	public:
		Glyph(UINT32 offset, UINT32 length) : offset(offset), length(length) {}
		~Glyph() {}
		UINT32 offset;
		UINT32 length;
	};

	TrueTypeLocaTable() : num_glyphs(0), next_offset(0), format_is_long(true) {}
	~TrueTypeLocaTable();

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length, UINT16 number_of_glyphs, bool index_to_loc_format_is_long);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

	OP_STATUS addGlyph(UINT32 length);
	OP_STATUS addGlyph(UINT32 offset, UINT32 length);
	OP_STATUS getOffsetLength(UINT32 glyph_index, UINT32& offset, UINT32& length);

	void setFormatIsLong(bool format_long) { format_is_long = format_long; }

private:
	TrueTypeLocaTable(const TrueTypeLocaTable &g);
	TrueTypeLocaTable& operator=(const TrueTypeLocaTable&);

	UINT16 num_glyphs;
	UINT32 next_offset;
	bool format_is_long;
	OpAutoVector<Glyph> glyph_vector;	// Loca table representation
};


//-------------------------------------------------
// SFNT post
//-------------------------------------------------
class TrueTypePostTable
{
public:
	TrueTypePostTable();
	
	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length);

	UINT32 getVersion() { return version; }
	UINT32 getIsFixedPitch() { return is_fixed_pitch; }	
	INT16 getUnderlinePosition() { return underline_position; }	
	INT16 getUnderlineThickness() { return underline_thickness; }	

	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

private:
	TrueTypePostTable(const TrueTypePostTable &g);
	TrueTypePostTable& operator=(const TrueTypePostTable&);

	UINT32 version;
	UINT32 italic_angle;
	INT16 underline_position;
	INT16 underline_thickness;
	UINT32 is_fixed_pitch;
	UINT32 min_mem_type_42;
	UINT32 max_mem_type_42;
	UINT32 min_mem_type_1;
	UINT32 max_mem_type_1;
};


//-------------------------------------------------
// TrueTypeGlyph
//-------------------------------------------------
class TrueTypeGlyph
{
public:
	TrueTypeGlyph(UINT32 opera_glyph_index, UINT32 ps_glyph_index, UINT8 ps_charcode);
	TrueTypeGlyph(UINT32 opera_glyph_index, UINT32 ps_glyph_index);
	~TrueTypeGlyph() { if (glyph_data) OP_DELETEA(glyph_data); }
	
	UINT8 getPSCharcode() { return ps_charcode; }
	UINT32 getOperaGlyphIndex() { return opera_glyph_index; }
	UINT32 getPSGlyphIndex() { return ps_glyph_index; }

	bool hasCharcode() { return has_charcode; }
	void getGlyphName(OpString8& name) { name.Set(glyph_name); }
	void setGlyphName(const uni_char* name) { glyph_name.Set(name); }

	UINT8* getGlyphData() { return glyph_data; }
	UINT32 getGlyphDataSize() { return glyph_data_size; }

	bool isComposite() { return num_contours < 0; }
	INT16 getNumContours() { return num_contours; }
	INT16 getXMin() { return x_min; };
	INT16 getYMin() { return y_min; };
	INT16 getXMax() { return x_max; };
	INT16 getYMax() { return y_max; };

	OP_STATUS copyGlyphData(const UINT8* data, UINT32 length);
	OP_STATUS writeGlyphData(PostScriptCommandBuffer& buffer);

private:
	TrueTypeGlyph(const TrueTypeGlyph &g);
	TrueTypeGlyph& operator=(const TrueTypeGlyph&);

	UINT32 opera_glyph_index;
	UINT32 ps_glyph_index;
	UINT8 ps_charcode;
	bool has_charcode;
	OpString8 glyph_name;
	UINT8* glyph_data;
	UINT32 glyph_data_size;

	INT16 num_contours;
	INT16 x_min;
	INT16 y_min;
	INT16 x_max;
	INT16 y_max;

	UINT32 composite_glyph_index;
};


//-------------------------------------------------
// SFNT glyf
//-------------------------------------------------
class TrueTypeGlyfTable
{
public:
	TrueTypeGlyfTable();
	~TrueTypeGlyfTable();

	OP_STATUS parseTable(const UINT8* table, const UINT32 table_length, UINT32 table_offset, TrueTypeLocaTable* loca_table);
	OP_STATUS generateSFNTData(PostScriptCommandBuffer& buffer);
	UINT32 getLength();
	UINT32 getChecksum();

	UINT32 getNumberOfGlyphs() { return handled_glyphs.GetCount(); }

	TrueTypeLocaTable* getLocaTable() { return &glyphs_loca_table; }
	TrueTypeHmtxTable* getHmtxTable() { return &glyphs_hmtx_table; }

	OP_STATUS getGlyph(UINT32 glyph_index, TrueTypeGlyph*& glyph, UINT8* font_data, UINT32 font_data_size, TrueTypeHmtxTable& hmtx_table);
	OP_STATUS addTrueTypeGlyph(UINT32 opera_glyph_index, UINT8* font_data, UINT32 font_data_size, TrueTypeGlyph*& glyph, TrueTypeHmtxTable& hmtx_table);
	OP_STATUS writeGlyfTableData(PostScriptCommandBuffer& buffer);
	OP_STATUS getPostScriptGlyphMappingDefs(OpString8& encoding_vector, OpString8& char_string_defs);

	void getMaxBoundingBox(INT16& x_min, INT16& y_min, INT16& x_max, INT16& y_max) { x_min = max_x_min; y_min = max_y_min; x_max = max_x_max; y_max = max_y_max; }
	INT16 getMaxContours() { return max_contours; }

private:
	TrueTypeGlyfTable(const TrueTypeGlyfTable &g);
	TrueTypeGlyfTable& operator=(const TrueTypeGlyfTable&);

	enum {
		ARG_1_AND_2_ARE_WORDS = 1 << 0,
		ARGS_ARE_XY_VALUES = 1 << 1,
		ROUND_XY_TO_GRID = 1 << 2,
		WE_HAVE_A_SCALE = 1 << 3,
		MORE_COMPONENTS = 1 << 5,
		WE_HAVE_AN_X_AND_Y_SCALE = 1 << 6,
		WE_HAVE_A_TWO_BY_TWO = 1 << 7,
		WE_HAVE_INSTRUCTIONS = 1 << 8,
		USE_MY_METRICS = 1 << 9,
		OVERLAP_COMPOUND = 1 << 10
	};

	OP_STATUS updateCompositeIndices(TrueTypeGlyph* glyph, UINT8* font_data, UINT32 font_data_size, TrueTypeHmtxTable& hmtx_table);
	void updateMaxValues(TrueTypeGlyph* glyph);

	/**
	   helper struct for glyph counters

	   if a glyph is successfully added the glyph counters should be
	   incremented. this lock increases the counters on creation, and
	   will restore them on destruction unless release is called.
	 */
	struct GlyfCounterLock
	{
		GlyfCounterLock(TrueTypeGlyfTable* glyf) : m_glyf(glyf) { m_glyf->incrementGlyphCounters(); }
		~GlyfCounterLock() { if (m_glyf) m_glyf->decrementGlyphCounters(); }
		void release() { m_glyf = NULL; }
		TrueTypeGlyfTable* m_glyf;
	};
	friend struct GlyfCounterLock;
	void incrementGlyphCounters();
	void decrementGlyphCounters();

	TrueTypeLocaTable* loca_table;

	OpAutoINT32HashTable<TrueTypeGlyph> handled_glyphs;

	UINT32 ps_charcode_counter;
	UINT32 ps_glyphindex_counter;

	TrueTypeLocaTable glyphs_loca_table;
	TrueTypeHmtxTable glyphs_hmtx_table;

	INT16 max_points;
	INT16 max_contours;
	INT16 max_x_min;
	INT16 max_y_min;
	INT16 max_x_max;
	INT16 max_y_max;

	UINT32 glyf_table_offset;
	UINT32 glyf_table_length;
};


//-------------------------------------------------
// TrueTypeConverter
//-------------------------------------------------

class TrueTypeConverter
{
public:
	TrueTypeConverter();
	~TrueTypeConverter();

	/** Init the converter with already available font data.
	 * @param font OpFont font object data to work on */
	OP_STATUS Init(UINT8* font_data, UINT32 data_size);

	/** Do necessary parsing of SFNT tables from given font data. */
	OP_STATUS parseFont();

	/** Get font information dictionary.
	 * @returns dictionary with PostScript font information */
	const class PostScriptDictionary& getFontInfo() { return fontinfo; }

	/** Generate PostScript Type42 output for the loaded font.
	 * @param buffer command buffer to output the generated data to */
	OP_STATUS generatePostScript(PostScriptCommandBuffer& buffer, OpString8& ps_fontname);

	/** Return font name as specified in TrueType file
	  * @return name of TrueType font. */
	OpString8 getFontName();

	/** Return font bounding box as specified in TrueType fule
	  * @return font bounding box. */
	void getFontBBox(OpString8& font_bbox) { head_table.getFontBBox(font_bbox); }

	/** Return checksum for glyf table, as specified in table record.
	  * Safe to call without call to parseFont. */
	UINT32 getGLYFChecksum() { return glyf_checksum; }

	/** Return the number of SFNT tables the converted font will have.
	  * @return number of tables in resulting font. */
	UINT16 getNumberOfTables() { return offset_table.getNumberOfTables(); }

	/** Return checksum of glyphs in font.
	  * @return number of glyphs in font. */
	UINT16 getNumberOfGlyphs() { return maxp_table.getNumberOfGlyphs(); }

	/** Return font type
	  * @return font type. */
	FontType getFontType() { return offset_table.getFontType(); }

	/** Get font Type42 paint type, currently only type 0 (filled glyphs) is supported
	  * @return Type42 paint type, 0=filled glyphs, 1=stroked glyphs. */
	int getPaintType() { return paint_type; }

	/** Check if font is bold
	  * @return true of font is bold.
	  * Safe to call without call to parseFont. */
	bool isBold() { return head_table.isBold(); }

	/** Check if font is italic
	  * @return true of font is italic.
	  * Safe to call without call to parseFont. */
	bool isItalic() { return head_table.isItalic(); }


	OP_STATUS getGlyphIdentifiers(UINT32 glyph_index, UINT8& ps_charcode, OpString8& glyph_name, bool& has_charcode, 
								  UINT8* font_data, UINT32 font_data_size);

private:
	OP_STATUS readSFNTTable(UINT32 table_tag);
	OP_STATUS readGlyfChecksum();
	OP_STATUS readOffsetTable();
	OP_STATUS readLocaTable();
	OP_STATUS readFontName();

	OP_STATUS generatePostScriptSFNTSData(PostScriptCommandBuffer& buffer);

	void updateOffsetTables();
	void updateSFNTTables();

	class DataHolder;
	OpAutoPtr<DataHolder> data_pointer;

	PostScriptDictionary fontinfo;
	UINT32 glyf_checksum;

	OpString8 font_name;
	int paint_type;

	TrueTypeOffsetTable offset_table;
	TrueTypeHeadTable head_table;
	TrueTypeHheaTable hhea_table;
	TrueTypeMaxpTable maxp_table;
	TrueTypeHmtxTable hmtx_table;
	TrueTypePostTable post_table;
	TrueTypeLocaTable loca_table;
	TrueTypeGlyfTable glyf_table;
};

#endif // VEGA_POSTSCRIPT_PRINTING

#endif // TRUE_TYPE_CONVERTER_H
