/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "core/pch.h"

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/libvega/src/postscript/truetypeconverter.h"
#include "modules/libvega/src/postscript/postscriptcommandbuffer.h"

#include "modules/util/opfile/opfile.h"


#define BOUNDS_CHECK(offset) if ((offset) > table_length) return OpStatus::ERR


//-------------------------------------------------
// SFNT offset table
//-------------------------------------------------

OP_STATUS TrueTypeOffsetTable::parseTable(const UINT8* table, const UINT32 table_length)
{
	OP_NEW_DBG("TrueTypeOffsetTable::parseTable()", "psprint");
	if (!table)
		return OpStatus::ERR;

	BOUNDS_CHECK(12);

	sfnt_version = SFNTDataTypes::getULong(table);
	number_of_tables = SFNTDataTypes::getUShort(table + 4);
	search_range = SFNTDataTypes::getUShort(table + 6);
	entry_selector = SFNTDataTypes::getUShort(table + 8);
	range_shift = SFNTDataTypes::getUShort(table + 10);
	font_type = GetFontType(sfnt_version);

	OP_DBG(("Number of SFNT tables: %d", number_of_tables));
	return(readTableRecords(table, table_length));
}


/** Read the individual table records that follows offset table */
OP_STATUS TrueTypeOffsetTable::readTableRecords(const UINT8* data, const UINT32 table_length)
{
	OP_NEW_DBG("TrueTypeOffsetTable::readTableRecords()", "psprint");

	BOUNDS_CHECK(12u + 16u*number_of_tables);

	const UINT8* table_record_ptr = data + 12;
	for (UINT16 table=0; table < number_of_tables; table++)
	{
		UINT32 tag = SFNTDataTypes::getULong(table_record_ptr);
		UINT32 checksum = SFNTDataTypes::getULong(table_record_ptr + 4);
		UINT32 offset = SFNTDataTypes::getULong(table_record_ptr + 8);
		UINT32 length = SFNTDataTypes::getULong(table_record_ptr + 12);
/*		printf("Table tag: %c%c%c%c\n", (tag & 0xff000000) >> 24, (tag & 0x00ff0000) >> 16, (tag & 0x0000ff00) >> 8, (tag & 0x000000ff));*/
		OP_DBG(("Table tag: %c%c%c%c", (tag & 0xff000000) >> 24, (tag & 0x00ff0000) >> 16, (tag & 0x0000ff00) >> 8, (tag & 0x000000ff)));
		OP_DBG((" - checksum: %x, offset: %x, length: %x", checksum, offset, length));

		if (checkSFNTTableWanted(tag))
		{
			BOUNDS_CHECK(offset + length);

			TrueTypeTableRecord* table_record = OP_NEW(TrueTypeTableRecord, (tag, checksum, offset, length));
			if (!table_record)
				return OpStatus::ERR_NO_MEMORY;
			OpAutoPtr<TrueTypeTableRecord> table_record_ap (table_record);

			if (checkSFNTTableCopied(tag))
			{
				table_record->copied_data = OP_NEWA(UINT8, length);
				if(!table_record->copied_data)
					return OpStatus::ERR_NO_MEMORY;

				op_memcpy(table_record->copied_data, data + offset, length);
			}

			table_record_ap.release();
			table_record->Into(&table_record_list);
		}

		table_record_ptr += 16;
	}

	// Recalculate offset table values for new number of tables
	number_of_tables = table_record_list.Cardinal();
	entry_selector = static_cast<UINT16>(op_floor(op_log((double)number_of_tables)/op_log(2.0)));   // Log2(max power of 2 <= number_of_tables)
	search_range = static_cast<UINT16>(op_pow(2, entry_selector) * 16);                     // (max power of 2 <= number_of_tables) * 16
	range_shift = number_of_tables * 16 - search_range;                                     // number of tables * 16 - search_range

	return OpStatus::OK;
}


/** Check if a table tag corresponds to one of the tables actually used by
  * PostScript Type42 renderers (see The Type 42 Font Format Specification,
  * Adobe Developer Support Technical Note #5012, page 12). */
bool TrueTypeOffsetTable::checkSFNTTableWanted(UINT32 tag)
{
	switch (tag)
	{
	case MAKE_TAG('h','e','a','d'):
	case MAKE_TAG('h','h','e','a'):
	case MAKE_TAG('m','a','x','p'):
	case MAKE_TAG('l','o','c','a'):
	case MAKE_TAG('h','m','t','x'):
	case MAKE_TAG('p','r','e','p'):
	case MAKE_TAG('f','p','g','m'):
	case MAKE_TAG('v','h','e','a'):
	case MAKE_TAG('g','l','y','f'):
	case MAKE_TAG('c','v','t',' '):
	case MAKE_TAG('v','m','t','x'):
		return true;
	}
	return false;
}


/** Return true if data should be copied for later use for this table */
bool TrueTypeOffsetTable::checkSFNTTableCopied(UINT32 tag)
{
	switch (tag)
	{
	case MAKE_TAG('h','e','a','d'):
	case MAKE_TAG('h','h','e','a'):
	case MAKE_TAG('m','a','x','p'):
	case MAKE_TAG('l','o','c','a'):
	case MAKE_TAG('h','m','t','x'):
		return false;
	}
	return true;
}


OP_STATUS TrueTypeOffsetTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	// Write offset table
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(sfnt_version));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(number_of_tables));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(search_range));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(entry_selector));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(range_shift));

	// Write table records
	TrueTypeTableRecord* tr = static_cast<TrueTypeTableRecord*>(table_record_list.First());
	while (tr)
	{
		RETURN_IF_ERROR(buffer.writeHexCodeUINT32(tr->tag));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT32(tr->checksum));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT32(tr->offset));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT32(tr->length));
	    tr = static_cast<TrueTypeTableRecord*>(tr->Suc());
	}

	return OpStatus::OK;
}

UINT32 TrueTypeOffsetTable::getLength()
{
	return 12 + table_record_list.Cardinal() * 16;
}

UINT32 TrueTypeOffsetTable::getChecksum()
{
	return 0; // FIXME
}



//-------------------------------------------------
// SFNT head
//-------------------------------------------------
TrueTypeHeadTable::TrueTypeHeadTable()
  : units_per_em(0), index_to_loc_format_is_long(false)
{
}


OP_STATUS TrueTypeHeadTable::parseTable(const UINT8* table, const UINT32 table_length)
{
	if (!table)
		return OpStatus::ERR;

	BOUNDS_CHECK(54);

// 	char file[64];
// 	snprintf(file, 64, "/tmp/sfnt_head_table_XXXXXX");
// 	int fd = mkstemp(file);
// 	write(fd, table, table_length);
// 	close(fd);

	table_version.Set(SFNTDataTypes::getULong(table));
	font_revision.Set(SFNTDataTypes::getULong(table + 4));

	checksum_adjustment = SFNTDataTypes::getULong(table + 8);
	magic_number = SFNTDataTypes::getULong(table + 12);
	flags = SFNTDataTypes::getUShort(table + 16);
	units_per_em = SFNTDataTypes::getUShort(table + 18);

	created_hi32 = SFNTDataTypes::getULong(table + 20);
	created_lo32 = SFNTDataTypes::getULong(table + 24);
	modified_hi32 = SFNTDataTypes::getULong(table + 28);
	modified_lo32 = SFNTDataTypes::getULong(table + 32);;

	x_min = SFNTDataTypes::getShort(table + 36);
	y_min = SFNTDataTypes::getShort(table + 38);
	x_max = SFNTDataTypes::getShort(table + 40);
	y_max = SFNTDataTypes::getShort(table + 42);

	mac_format = SFNTDataTypes::getUShort(table + 44);
	lowest_rec_ppem = SFNTDataTypes::getUShort(table + 46);
	font_direction_hint = SFNTDataTypes::getShort(table + 48);

	// The indexToLocFormat field tells wether loca table stores short or long values
	UINT16 index_to_loc_format = SFNTDataTypes::getUShort(table + 50);
	index_to_loc_format_is_long = (index_to_loc_format == 1) ? true : false;

	glyph_data_format = SFNTDataTypes::getShort(table + 52);

	return OpStatus::OK;
}


OP_STATUS TrueTypeHeadTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(table_version.integer));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(table_version.fraction));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(font_revision.integer));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(font_revision.fraction));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(checksum_adjustment));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(magic_number));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(flags));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(units_per_em));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(created_hi32));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(created_lo32));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(modified_hi32));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(modified_lo32));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(x_min));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(y_min));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(x_max));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(y_max));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(mac_format));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(lowest_rec_ppem));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(font_direction_hint));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(index_to_loc_format_is_long ? 1 : 0));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(0));

	return OpStatus::OK;
}


UINT32 TrueTypeHeadTable::getLength()
{
	return 54;
}


UINT32 TrueTypeHeadTable::getChecksum()
{
	SFNTChecksumAdder checksum;
	checksum.addUINT16(table_version.integer);
	checksum.addUINT16(table_version.fraction);
	checksum.addUINT16(font_revision.integer);
	checksum.addUINT16(font_revision.fraction);
	checksum.addUINT32(checksum_adjustment);
	checksum.addUINT32(magic_number);
	checksum.addUINT16(flags);
	checksum.addUINT16(units_per_em);
	checksum.addUINT32(created_hi32);
	checksum.addUINT32(created_lo32);
	checksum.addUINT32(modified_hi32);
	checksum.addUINT32(modified_lo32);
	checksum.addUINT16(x_min);
	checksum.addUINT16(y_min);
	checksum.addUINT16(x_max);
	checksum.addUINT16(y_max);
	checksum.addUINT16(mac_format);
	checksum.addUINT16(lowest_rec_ppem);
	checksum.addUINT16(font_direction_hint);
	checksum.addUINT16(index_to_loc_format_is_long ? 1 : 0);
	checksum.addUINT16(0);

	return checksum.getChecksum();
}


OP_STATUS TrueTypeHeadTable::getFontBBox(OpString8& font_bbox)
{
	long _x_min = convertUnit(x_min);
	long _y_min = convertUnit(y_min);
	long _x_max = convertUnit(x_max);
	long _y_max = convertUnit(y_max);
	RETURN_IF_ERROR(font_bbox.Set(""));
	return font_bbox.AppendFormat("[%ld %ld %ld %ld]", _x_min, _y_min, _x_max, _y_max);
}


bool TrueTypeHeadTable::isBold()
{
	if (mac_format & 1 << 0)
		return true;
	return false;
}


bool TrueTypeHeadTable::isItalic()
{
	if (mac_format & 1 << 1)
		return true;
	return false;
}


//-------------------------------------------------
// SFNT hhea
//-------------------------------------------------

OP_STATUS TrueTypeHheaTable::parseTable(const UINT8* table, const UINT32 table_length)
{
	if (!table)
		return OpStatus::ERR;

	BOUNDS_CHECK(36);

	ascender = SFNTDataTypes::getShort(table + 4);
	descender = SFNTDataTypes::getShort(table + 6);
	line_gap = SFNTDataTypes::getShort(table + 8);
	advance_width_max = SFNTDataTypes::getUShort(table + 10);
	min_left_side_bearing = SFNTDataTypes::getShort(table + 12);
	min_right_side_bearing = SFNTDataTypes::getShort(table + 14);
	x_max_extent = SFNTDataTypes::getShort(table + 16);
	caret_slope_rise = SFNTDataTypes::getShort(table + 18);
	caret_slope_run = SFNTDataTypes::getShort(table + 20);
	caret_offset = SFNTDataTypes::getShort(table + 22);
	metric_data_format = SFNTDataTypes::getShort(table + 32);
	number_of_h_metrics = SFNTDataTypes::getUShort(table + 34);

	return OpStatus::OK;
}


OP_STATUS TrueTypeHheaTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(0x00010000));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(ascender));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(descender));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(line_gap));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(advance_width_max));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(min_left_side_bearing));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(min_right_side_bearing));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(x_max_extent));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(caret_slope_rise));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(caret_slope_run));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(caret_offset));

	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(0));	// reserved word
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(0));	// reserved word
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(0));	// reserved word
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(0));	// reserved word

	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(metric_data_format));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(number_of_h_metrics));

	return OpStatus::OK;
}


UINT32 TrueTypeHheaTable::getLength()
{
	return 36;
}


UINT32 TrueTypeHheaTable::getChecksum()
{
	SFNTChecksumAdder checksum;
	checksum.addUINT32(0x00010000);
	checksum.addUINT16(ascender);
	checksum.addUINT16(descender);
	checksum.addUINT16(line_gap);
	checksum.addUINT16(advance_width_max);
	checksum.addUINT16(min_left_side_bearing);
	checksum.addUINT16(min_right_side_bearing);
	checksum.addUINT16(x_max_extent);
	checksum.addUINT16(caret_slope_rise);
	checksum.addUINT16(caret_slope_run);
	checksum.addUINT16(caret_offset);
	checksum.addUINT16(0);
	checksum.addUINT16(0);
	checksum.addUINT16(0);
	checksum.addUINT16(0);
	checksum.addUINT16(metric_data_format);
	checksum.addUINT16(number_of_h_metrics);
	return checksum.getChecksum();
}


//-------------------------------------------------
// SFNT maxp
//-------------------------------------------------

OP_STATUS TrueTypeMaxpTable::parseTable(const UINT8* table, const UINT32 table_length)
{
	if (!table)
		return OpStatus::ERR;

	BOUNDS_CHECK(32);

	table_version = SFNTDataTypes::getULong(table);
	num_glyphs = SFNTDataTypes::getUShort(table + 4);

	if (table_version == 0x00010000)
	{
		max_points = SFNTDataTypes::getUShort(table + 6);
		max_contours = SFNTDataTypes::getUShort(table + 8);
		max_composite_points = SFNTDataTypes::getUShort(table + 10);
		max_composite_contours = SFNTDataTypes::getUShort(table + 12);
		max_zones = SFNTDataTypes::getUShort(table + 14);
		max_twilight_points = SFNTDataTypes::getUShort(table + 16);
		max_storage = SFNTDataTypes::getUShort(table + 18);
		max_function_defs = SFNTDataTypes::getUShort(table + 20);
		max_instrustion_defs = SFNTDataTypes::getUShort(table + 22);
		max_stack_elements = SFNTDataTypes::getUShort(table + 24);
		max_size_of_instructions = SFNTDataTypes::getUShort(table + 26);
		max_component_elements = SFNTDataTypes::getUShort(table + 28);
		max_component_depth = SFNTDataTypes::getUShort(table + 30);
	}

	return OpStatus::OK;
}


OP_STATUS TrueTypeMaxpTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	RETURN_IF_ERROR(buffer.writeHexCodeUINT32(table_version));
	RETURN_IF_ERROR(buffer.writeHexCodeUINT16(num_glyphs));

	if (table_version == 0x00010000)
	{
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_points));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_contours));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_composite_points));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_composite_contours));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_zones));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_twilight_points));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_storage));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_function_defs));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_instrustion_defs));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_stack_elements));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_size_of_instructions));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_component_elements));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(max_component_depth));
	}

	return OpStatus::OK;
}


UINT32 TrueTypeMaxpTable::getLength()
{
	if (table_version == 0x00010000)
		return 32;
	return 6;
}


UINT32 TrueTypeMaxpTable::getChecksum()
{
	SFNTChecksumAdder checksum;

	checksum.addUINT32(table_version);
	checksum.addUINT16(num_glyphs);

	if (table_version == 0x00010000)
	{
		checksum.addUINT16(max_points);
		checksum.addUINT16(max_contours);
		checksum.addUINT16(max_composite_points);
		checksum.addUINT16(max_composite_contours);
		checksum.addUINT16(max_zones);
		checksum.addUINT16(max_twilight_points);
		checksum.addUINT16(max_storage);
		checksum.addUINT16(max_function_defs);
		checksum.addUINT16(max_instrustion_defs);
		checksum.addUINT16(max_stack_elements);
		checksum.addUINT16(max_size_of_instructions);
		checksum.addUINT16(max_component_elements);
		checksum.addUINT16(max_component_depth);
	}

	return checksum.getChecksum();
}


//-------------------------------------------------
// SFNT hmtx
//-------------------------------------------------


OP_STATUS TrueTypeHmtxTable::parseTable(const UINT8* table, const UINT32 table_length, UINT16 number_of_glyphs, UINT16 number_of_hmetrics)
{
	if (!table)
		return OpStatus::ERR;

	OP_ASSERT(number_of_glyphs >= number_of_hmetrics);
	BOUNDS_CHECK(4u*number_of_hmetrics + 2u*(number_of_glyphs-number_of_hmetrics));

	num_glyphs = number_of_glyphs;

	const UINT8* pos = table;
	UINT16 advance_width = 0;
	INT16 left_side_bearing;
	UINT32 glyph_index = 0;
	for (UINT16 m = 0; m < number_of_hmetrics; m++)
	{
		advance_width = SFNTDataTypes::getUShort(pos);
		left_side_bearing = SFNTDataTypes::getShort(pos + 2);
		RETURN_IF_ERROR(addGlyphHMetrics(glyph_index, advance_width, left_side_bearing));
		++glyph_index;
		pos += 4;
	}

	// After above list with both values follows an array of extra lbearings using the width from last element in above list.
	for (UINT16 lb = 0; lb < num_glyphs - number_of_hmetrics; lb++)
	{
		left_side_bearing = SFNTDataTypes::getShort(pos);
		RETURN_IF_ERROR(addGlyphHMetrics(glyph_index, advance_width, left_side_bearing));
		++glyph_index;
		pos += 2;
	}

	return OpStatus::OK;
}


OP_STATUS TrueTypeHmtxTable::addGlyphHMetrics(UINT32 glyph_index, UINT16 advance_width, INT16 left_side_bearing)
{
	GlyphHMetrics* hmetrics = OP_NEW(GlyphHMetrics, (glyph_index, advance_width, left_side_bearing));
	if (!hmetrics)
		return OpStatus::ERR_NO_MEMORY;

	if (advance_width > max_advance_width)
		max_advance_width = advance_width;
	if (left_side_bearing < min_left_side_bearing)
		min_left_side_bearing = left_side_bearing;

	glyph_hmetrics_vector.Add(hmetrics);
	return OpStatus::OK;
}


OP_STATUS TrueTypeHmtxTable::getGlyphHMetrics(UINT32 glyph_index, UINT16& advance_width, INT16& left_side_bearing)
{
	if (glyph_index >= glyph_hmetrics_vector.GetCount())
		return OpStatus::ERR;

	GlyphHMetrics * ghm = glyph_hmetrics_vector.Get(glyph_index);
	if (!ghm)
		return OpStatus::ERR;

	advance_width = ghm->advance_width;
	left_side_bearing = ghm->left_side_bearing;

	return OpStatus::OK;
}



OP_STATUS TrueTypeHmtxTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	UINT32 count = glyph_hmetrics_vector.GetCount();
	for (UINT32 g=0; g < count; g++)
	{
		GlyphHMetrics * ghm = glyph_hmetrics_vector.Get(g);
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(ghm->advance_width));
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(ghm->left_side_bearing));
	}

	return OpStatus::OK;
}


UINT32 TrueTypeHmtxTable::getLength()
{
	return glyph_hmetrics_vector.GetCount() * 4;
}


UINT32 TrueTypeHmtxTable::getChecksum()
{
	SFNTChecksumAdder checksum;

	UINT32 count = glyph_hmetrics_vector.GetCount();
	for (UINT32 g=0; g < count; g++)
	{
		GlyphHMetrics * ghm = glyph_hmetrics_vector.Get(g);
		checksum.addUINT16(ghm->advance_width);
		checksum.addUINT16(ghm->left_side_bearing);
	}

	return checksum.getChecksum();
}



//-------------------------------------------------
// SFNT loca
//-------------------------------------------------

TrueTypeLocaTable::~TrueTypeLocaTable()
{
}


OP_STATUS TrueTypeLocaTable::parseTable(const UINT8* table, const UINT32 table_length, UINT16 number_of_glyphs, bool index_to_loc_format_is_long)
{
	if (!table)
		return OpStatus::ERR;

	BOUNDS_CHECK((number_of_glyphs + 1) * (index_to_loc_format_is_long ? 4u : 2u));

	num_glyphs = number_of_glyphs;
	format_is_long = index_to_loc_format_is_long;

	const UINT8* pos = table;
	for (UINT16 g=0; g < num_glyphs; g++)
	{
		UINT32 offset;
		UINT32 length;
		if (format_is_long)
		{
			offset = SFNTDataTypes::getULong(pos);
			length = SFNTDataTypes::getULong(pos+4) - offset;  // Safe since Loca table has 1 extra element for size caulation
			pos += 4;
		}
		else
		{
			// Short table version stores actual offset divided by 2
			offset = (UINT32)SFNTDataTypes::getUShort(pos) * 2;
			length = (UINT32)SFNTDataTypes::getUShort(pos+2) * 2 - offset;  // Safe since Loca table has 1 extra element for size caulation
			pos += 2;
		}

		addGlyph(offset, length);
	}

	return OpStatus::OK;
}


OP_STATUS TrueTypeLocaTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	UINT32 count = glyph_vector.GetCount();
	Glyph* glyph = NULL;

	for (UINT32 g=0; g < count; g++)
	{
		glyph = glyph_vector.Get(g);

		if (format_is_long)
			RETURN_IF_ERROR(buffer.writeHexCodeUINT32(glyph->offset));
		else
			RETURN_IF_ERROR(buffer.writeHexCodeUINT16(static_cast<UINT16>(glyph->offset / 2)));
	}

	// Write final extra element which equals total glyph table size.
	UINT32 length = 0;
	if (glyph)
		length = glyph->offset + glyph->length;

	if (format_is_long)
		RETURN_IF_ERROR(buffer.writeHexCodeUINT32(length));
	else
		RETURN_IF_ERROR(buffer.writeHexCodeUINT16(static_cast<UINT16>(length / 2)));

	return OpStatus::OK;
}


UINT32 TrueTypeLocaTable::getLength()
{
	if (format_is_long)
		return (glyph_vector.GetCount() + 1) * 4;
	return (glyph_vector.GetCount() + 1) * 2;
}


UINT32 TrueTypeLocaTable::getChecksum()
{
	SFNTChecksumAdder checksum;

	UINT32 count = glyph_vector.GetCount();
	Glyph* glyph = NULL;

	for (UINT32 g=0; g < count; g++)
	{
		glyph = glyph_vector.Get(g);
		if (format_is_long)
			checksum.addUINT32(glyph->offset);
		else
			checksum.addUINT16(static_cast<UINT16>(glyph->offset / 2));
	}

	// Final extra element which equals total glyph table size.
	UINT32 length = 0;
	if (glyph)
		length = glyph->offset + glyph->length;

	if (format_is_long)
		checksum.addUINT32(length);
	else
		checksum.addUINT16(static_cast<UINT16>(length / 2));

	return checksum.getChecksum();
}


OP_STATUS TrueTypeLocaTable::addGlyph(UINT32 length)
{
	Glyph* glyph = OP_NEW(Glyph, (next_offset, length));
	if (!glyph)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = glyph_vector.Add(glyph);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(glyph);
		return status;
	}

	next_offset += length;
	if (next_offset % 2)
		++next_offset;

	return OpStatus::OK;
}


OP_STATUS TrueTypeLocaTable::addGlyph(UINT32 offset, UINT32 length)
{
	Glyph* glyph = OP_NEW(Glyph, (offset, length));
	if (!glyph)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = glyph_vector.Add(glyph);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(glyph);
		return status;
	}

	next_offset = offset + length;
	if (next_offset % 2)
		++next_offset;

	return OpStatus::OK;
}


/** Return offset and length of a glyph in glyf table for a glyph index */
OP_STATUS TrueTypeLocaTable::getOffsetLength(UINT32 glyph_index, UINT32& offset, UINT32& length)
{
	if (glyph_index >= num_glyphs || glyph_index >= glyph_vector.GetCount())
		return OpStatus::ERR;

	Glyph* glyph = glyph_vector.Get(glyph_index);
	offset = glyph->offset;
	length = glyph->length;
	return OpStatus::OK;
}



//-------------------------------------------------
// SFNT post
//-------------------------------------------------

TrueTypePostTable::TrueTypePostTable()
	: version(0)
	, italic_angle(0)
	, underline_position(0)
	, underline_thickness(0)
	, is_fixed_pitch(0)
	, min_mem_type_42(0)
	, max_mem_type_42(0)
	, min_mem_type_1(0)
	, max_mem_type_1(0)
{
}


OP_STATUS TrueTypePostTable::parseTable(const UINT8* table, const UINT32 table_length)
{
	if (!table)
		return OpStatus::ERR;

	BOUNDS_CHECK(32);

	const UINT8* pos = table;
	version = SFNTDataTypes::getULong(pos);
	pos += 4; // skip to italicAngle
	italic_angle = SFNTDataTypes::getULong(pos);
	pos += 4; // skip to underlinePosition
	underline_position = SFNTDataTypes::getShort(pos);
	pos += 2; // skip to underlineThickness
	underline_thickness = SFNTDataTypes::getShort(pos);
	pos += 2; // skip to isFixedPitch
	is_fixed_pitch = SFNTDataTypes::getULong(pos);
	pos += 4; // skip to mimMemType42
	min_mem_type_42 = SFNTDataTypes::getULong(pos);
	pos += 4; // skip to maxMemType42
	max_mem_type_42 = SFNTDataTypes::getULong(pos);
	pos += 4; // skip to mimMemType1
	min_mem_type_1 = SFNTDataTypes::getULong(pos);
	pos += 4; // skip to maxMemType1
	max_mem_type_1 = SFNTDataTypes::getULong(pos);

	return OpStatus::OK;
}


OP_STATUS TrueTypePostTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	return OpStatus::OK;
}


UINT32 TrueTypePostTable::getLength()
{
	return 0;
}


UINT32 TrueTypePostTable::getChecksum()
{
	return 0;
}


//-------------------------------------------------
// SFNT glyf
//-------------------------------------------------

TrueTypeGlyfTable::TrueTypeGlyfTable()
	: ps_charcode_counter(0)
	, ps_glyphindex_counter(0)
	, max_points(0)
	, max_contours(0)
	, max_x_min(0x7fff)
	, max_y_min(0x7fff)
	, max_x_max(0)
	, max_y_max(0)
{
}

TrueTypeGlyfTable::~TrueTypeGlyfTable()
{
}


OP_STATUS TrueTypeGlyfTable::parseTable(const UINT8* table, const UINT32 table_length, UINT32 table_offset, TrueTypeLocaTable* loca_table)
{
	this->loca_table = loca_table;

	glyf_table_offset = table_offset;
	glyf_table_length = table_length;

	return OpStatus::OK;
}



OP_STATUS TrueTypeGlyfTable::generateSFNTData(PostScriptCommandBuffer& buffer)
{
	return OpStatus::OK;
}



UINT32 TrueTypeGlyfTable::getLength()
{
	OpHashIterator* glyph_iter = handled_glyphs.GetIterator();
	OpAutoPtr<OpHashIterator> glyph_iter_ap(glyph_iter);

	UINT32 length = 0;
	if (glyph_iter)
	{
		OP_STATUS ret = glyph_iter->First();
        while (ret == OpStatus::OK)
        {
			TrueTypeGlyph* glyph = static_cast<TrueTypeGlyph*>(glyph_iter->GetData());
			length += glyph->getGlyphDataSize();
			if (length % 2) // 2 byte align glyph data
				++length;

			ret = glyph_iter->Next();
		}
	}

	return length;
}


UINT32 TrueTypeGlyfTable::getChecksum()
{
	SFNTChecksumAdder checksum;

	OpHashIterator* glyph_iter = handled_glyphs.GetIterator();
	OpAutoPtr<OpHashIterator> glyph_iter_ap(glyph_iter);

	UINT32 length = 0;
	if (glyph_iter)
	{
		OP_STATUS ret = glyph_iter->First();
        while (ret == OpStatus::OK)
        {
			TrueTypeGlyph* glyph = static_cast<TrueTypeGlyph*>(glyph_iter->GetData());
			UINT32 data_size = glyph->getGlyphDataSize();
			checksum.addData(glyph->getGlyphData(), data_size);
			length += data_size;
			if (length % 2) // 2 byte align glyph data
			{
				++length;
				checksum.addUINT8(0);
			}

			ret = glyph_iter->Next();
		}
	}

	return checksum.getChecksum();
}



OP_STATUS TrueTypeGlyfTable::getGlyph(UINT32 glyph_index, TrueTypeGlyph*& glyph, UINT8* font_data, UINT32 font_data_size, TrueTypeHmtxTable& hmtx_table)
{
	TrueTypeGlyph* _glyph;
	if (handled_glyphs.Contains(glyph_index))
		RETURN_IF_ERROR(handled_glyphs.GetData(glyph_index, &_glyph));
	else
		RETURN_IF_ERROR(addTrueTypeGlyph(glyph_index, font_data, font_data_size, _glyph, hmtx_table));

	glyph = _glyph;
// 	printf("getGlyphIdentifiers() - glyph_index: %d, - ps_charcode: %d\n", glyph_index, ps_charcode);

	return OpStatus::OK;
}


/** Add a TrueType glyph to list of handled glyphs.
  *
  * @param opera_glyph_index glyph index to add as given by Opera
  * @param font_data pointer to font data
  * @param font_data_size size of font data
  * @param glyph (out) set to resulting Glyph object on success
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeGlyfTable::addTrueTypeGlyph(UINT32 opera_glyph_index, UINT8* font_data, UINT32 font_data_size, 
											  TrueTypeGlyph*& glyph, TrueTypeHmtxTable& hmtx_table)
{
	if (ps_charcode_counter < 256)
		glyph = OP_NEW(TrueTypeGlyph, (opera_glyph_index, ps_glyphindex_counter, ps_charcode_counter));
	else
		glyph = OP_NEW(TrueTypeGlyph, (opera_glyph_index, ps_glyphindex_counter));

	if (!glyph)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoPtr<TrueTypeGlyph> glyph_ap(glyph);
	GlyfCounterLock lock(this);

	// Copy glyph data from font
	UINT32 offset; UINT32 length;
	RETURN_IF_ERROR(loca_table->getOffsetLength(opera_glyph_index, offset, length));

	const UINT8* glyf_table = font_data + glyf_table_offset;

	//printf("copying glyph data for glyph %x (%x), offset: %d (%%4 = %d), length: %d (%%4 = %d)\n", opera_glyph_index, glyph->getPSGlyphIndex(), offset, offset % 4, length, length % 4);
	const UINT8* glyph_data = glyf_table + offset;
	RETURN_IF_ERROR(glyph->copyGlyphData(glyph_data, length));

	// Add the glyph to internal hmtx table
	UINT16 advance_width; INT16 left_side_bearing;
	RETURN_IF_ERROR(hmtx_table.getGlyphHMetrics(opera_glyph_index, advance_width, left_side_bearing));
	RETURN_IF_ERROR(glyphs_hmtx_table.addGlyphHMetrics(glyph->getPSGlyphIndex(), advance_width, left_side_bearing));

	updateMaxValues(glyph);

	RETURN_IF_ERROR(handled_glyphs.Add(opera_glyph_index, glyph));

	// Add the glyph to internal loca table
	OP_STATUS status = glyphs_loca_table.addGlyph(length);
	if (OpStatus::IsError(status))
	{
		handled_glyphs.Remove(opera_glyph_index, &glyph);
		return status;
	}
	glyph_ap.release();
	lock.release();

	// makes calls to getGlyph, which might end up here again
	if (glyph->isComposite())
	{
		status = updateCompositeIndices(glyph, font_data, font_data_size, hmtx_table);
	}

	return status;
}


/** Update all composite glyph indices in glyph if it is a composite. */
OP_STATUS TrueTypeGlyfTable::updateCompositeIndices(TrueTypeGlyph* glyph, UINT8* font_data, UINT32 font_data_size, TrueTypeHmtxTable& hmtx_table)
{
	if (!glyph->isComposite())
		return OpStatus::ERR;

	const UINT8* const glyph_data_end = glyph->getGlyphData() + glyph->getGlyphDataSize();
	UINT8* pos = glyph->getGlyphData() + 10;
	bool more_components = true;
	while(more_components)
	{
		// bounds-check
		if (pos + 4u > glyph_data_end)
			return OpStatus::ERR;

		UINT16 flags = SFNTDataTypes::getUShort(pos);
		UINT16 composite_glyph_index = SFNTDataTypes::getUShort(pos + 2);

		//printf(" - index: %x\n", composite_glyph_index);

		TrueTypeGlyph* composite_glyph;
		RETURN_IF_ERROR(getGlyph(composite_glyph_index, composite_glyph, font_data, font_data_size, hmtx_table));
		UINT16 new_index = static_cast<UINT16>(composite_glyph->getPSGlyphIndex());
		SFNTDataTypes::setUShort(pos + 2, new_index);

		//UINT16 changed = SFNTDataTypes::getUShort(pos + 2);
		//printf("After change composite index is: %x\n", changed);

		pos += 4 + 2;
		if (flags & ARG_1_AND_2_ARE_WORDS)
			pos += 2;

		if (flags & WE_HAVE_A_SCALE)
			pos += 2;
		else if (flags & WE_HAVE_AN_X_AND_Y_SCALE)
			pos += 4;
		else if (flags & WE_HAVE_A_TWO_BY_TWO)
			pos += 8;

		more_components = (flags & MORE_COMPONENTS) != 0;
	}

	return OpStatus::OK;
}


void TrueTypeGlyfTable::updateMaxValues(TrueTypeGlyph* glyph)
{
	if (glyph->getNumContours() > max_contours)
		max_contours = glyph->getNumContours();

	if (glyph->getXMin() < max_x_min)
		max_x_min = glyph->getXMin();

	if (glyph->getYMin() < max_y_min)
		max_y_min = glyph->getYMin();

	if (glyph->getXMax() > max_x_max)
		max_x_max = glyph->getXMax();

	if (glyph->getYMax() > max_y_max)
		max_y_max = glyph->getYMax();
}


void TrueTypeGlyfTable::incrementGlyphCounters()
{
	++ps_charcode_counter;
	++ps_glyphindex_counter;
}


void TrueTypeGlyfTable::decrementGlyphCounters()
{
	--ps_charcode_counter;
	--ps_glyphindex_counter;
}




/** Write SFNT glyf table with data from used glyphs
  *
  * @param buffer output buffer to write data to
  * @return OpStatus::OK if successful.
  */
OP_STATUS TrueTypeGlyfTable::writeGlyfTableData(PostScriptCommandBuffer& buffer)
{
	OpHashIterator* glyph_iter = handled_glyphs.GetIterator();
	OpAutoPtr<OpHashIterator> glyph_iter_ap(glyph_iter);

	UINT32 max_string_size = buffer.getMaxPSStringSize();
	UINT32 size_count = 0;

	if (glyph_iter)
	{
		OP_STATUS ret = glyph_iter->First();
        while (ret == OpStatus::OK)
        {
			TrueTypeGlyph* glyph = static_cast<TrueTypeGlyph*>(glyph_iter->GetData());

			// Break hexcode block if it becomes too large due to PostScript limitations
			size_count += glyph->getGlyphDataSize() * 2;  // 2 hex code chars per byte
			if (size_count >= max_string_size)
			{
				RETURN_IF_ERROR(buffer.endHexCodeBlock());
				RETURN_IF_ERROR(buffer.beginHexCodeBlock());
				size_count = glyph->getGlyphDataSize() * 2;
			}

// 			buffer.addCommand("<glyph>");
			RETURN_IF_ERROR(glyph->writeGlyphData(buffer));
// 			buffer.addCommand("</glyph>");

			ret = glyph_iter->Next();
		}
	}

	return OpStatus::OK;
}


/** Generate definitions for the PostScript encoding vector and CharStrings dictionary that are needed for a Type42 font.
  *
  * @param encoding_vector (out) will be set to resulting encoding vector
  * @param char_string_defs (out) will be set to resulting CharStrings dictionary
  * @return OpStatus::OK if successful. 
  */
OP_STATUS TrueTypeGlyfTable::getPostScriptGlyphMappingDefs(OpString8& encoding_vector, OpString8& char_string_defs)
{
	int encoding_width = 0;
	int char_string_width = 0;
	int glyph_count = 0;

	OpHashIterator* glyph_iter = handled_glyphs.GetIterator();
	OpAutoPtr<OpHashIterator> glyph_iter_ap(glyph_iter);

	if (glyph_iter)
	{
		OP_STATUS ret = glyph_iter->First();
        while (ret == OpStatus::OK)
        {
			TrueTypeGlyph* glyph = static_cast<TrueTypeGlyph*>(glyph_iter->GetData());

			OpString8 glyph_name;
			glyph->getGlyphName(glyph_name);
			encoding_vector.AppendFormat("/%s ", glyph_name.CStr());
			if (++encoding_width == 8 )  {
				encoding_width = 0;
				encoding_vector.Append("\n");
			}

			char_string_defs.AppendFormat("/%s %d ", glyph_name.CStr(), glyph->getPSGlyphIndex());
			if (++char_string_width == 6 )  {
				char_string_width = 0;
				char_string_defs.Append("\n");
			}

			glyph_count++;
			ret = glyph_iter->Next();
		}
	}

	// encoding tables must have exactly 256 elements, fill up with .notdefs
	for (int c=0; c < 256 - glyph_count; c++)
	{
		encoding_vector.Append("/.notdef ");
		if (++encoding_width == 8 )  {
			encoding_width = 0;
			encoding_vector.Append("\n");
		}
	}

	return OpStatus::OK;
}




//=================================================
// TrueTypeGlyph
//=================================================
TrueTypeGlyph::TrueTypeGlyph(UINT32 opera_glyph_index, UINT32 ps_glyph_index, UINT8 ps_charcode) 
	: opera_glyph_index(opera_glyph_index)
	, ps_glyph_index(ps_glyph_index)
	, ps_charcode(ps_charcode)
	, has_charcode(true)
	, glyph_data(0)
	, num_contours(0)
	, composite_glyph_index(0)
{ 
	glyph_name.AppendFormat("g%x", ps_glyph_index);
	if (opera_glyph_index == 0)
		this->ps_glyph_index = 0;
}


TrueTypeGlyph::TrueTypeGlyph(UINT32 opera_glyph_index, UINT32 ps_glyph_index) 
	: opera_glyph_index(opera_glyph_index)
	, ps_glyph_index(ps_glyph_index)
	, ps_charcode(0)
	, has_charcode(false)
	, glyph_data(0)
	, num_contours(0)
	, composite_glyph_index(0)
{ 
	glyph_name.AppendFormat("g%x", ps_glyph_index); 
	if (opera_glyph_index == 0)
		this->ps_glyph_index = 0;
}


/** Copy glyph data from the font to the internal glyph representation
  *
  * @param data start of glyph data to copy
  * @param length size of glyph data to copy
  * @return OpStatus::OK if successful.
  */
OP_STATUS TrueTypeGlyph::copyGlyphData(const UINT8* data, UINT32 length)
{
	if (!data)
		return OpStatus::ERR;

	glyph_data = OP_NEWA(UINT8, length);
	if(!glyph_data)
		return OpStatus::ERR_NO_MEMORY;

	num_contours = SFNTDataTypes::getShort(data);
	x_min = SFNTDataTypes::getShort(data + 2);
	y_min = SFNTDataTypes::getShort(data + 4);
	x_max = SFNTDataTypes::getShort(data + 6);
	y_max = SFNTDataTypes::getShort(data + 8);

	if (isComposite())
	{
		composite_glyph_index = SFNTDataTypes::getUShort(data+12);
	}

	op_memcpy(glyph_data, data, length);
	glyph_data_size = length;

	return OpStatus::OK;
}


/** Write glyph data to an output buffer as hexcode
  *
  * @param buffer output buffer to write glyph data to
  * @return OpStatus::OK if successful.
  */
OP_STATUS TrueTypeGlyph::writeGlyphData(PostScriptCommandBuffer& buffer)
{
	RETURN_IF_ERROR(buffer.writeHexCode(glyph_data, glyph_data_size, false));

	if (glyph_data_size % 2) // padding to 2 byte alignment
		RETURN_IF_ERROR(buffer.writeHexCodeUINT8(0));

	return OpStatus::OK;
}



//-------------------------------------------------
// TrueTypeConverter::DataHolder
//-------------------------------------------------
class TrueTypeConverter::DataHolder
{
public:
	DataHolder(UINT8* data, UINT32 size) : data(data), length(size) {}
	~DataHolder() {}
	UINT8* getData() const { return data; }
	OpFileLength getLength() const { return length; }
private:
	UINT8* data;
	OpFileLength length;
};



//=================================================
// TrueTypeConverter
//=================================================
TrueTypeConverter::TrueTypeConverter()
  : glyf_checksum(0)
  , paint_type(0)
{
}


TrueTypeConverter::~TrueTypeConverter()
{
}


/** Initialize the converter with a font.
  * 
  * @param font_data pointer to font data to use with the converter
  * @param font_data_size size of font data
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeConverter::Init(UINT8* font_data, UINT32 data_size)
{
	data_pointer.reset(OP_NEW(DataHolder, (font_data, data_size)));
	if (!data_pointer.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(readGlyfChecksum());
	return readSFNTTable(MAKE_TAG('h','e','a','d'));
}


/** Parse the initialized font. This will read and parse several important SFNT tables and store
  * internal representations of the data. This data will be used when generating a PostScript Type42
  * representation of the font.
  * 
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeConverter::parseFont()
{
	DataHolder* data_holder = data_pointer.get();
	if (!data_holder)
		return OpStatus::ERR;

	RETURN_IF_ERROR(readOffsetTable());
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('h','h','e','a')));
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('m','a','x','p')));
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('h','m','t','x')));
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('l','o','c','a')));
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('p','o','s','t')));
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('c','m','a','p')));
	RETURN_IF_ERROR(readSFNTTable(MAKE_TAG('g','l','y','f')));

	if (font_name.IsEmpty())
		RETURN_IF_ERROR(readFontName());

	// Add glyph entry for the empty .notdef glyph
	TrueTypeGlyph* glyph;
	RETURN_IF_ERROR(glyf_table.addTrueTypeGlyph(0, data_holder->getData(), (UINT32)data_holder->getLength(), glyph, hmtx_table));
	glyph->setGlyphName(UNI_L(".notdef"));

	// We can't rely on font data availability after this point
	data_pointer.reset();
	return OpStatus::OK;
}


/** Read the offset table that all fonts that are not collections begins with 
  * 
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeConverter::readOffsetTable()
{
	DataHolder* data_holder = data_pointer.get();
	if (!data_holder)
		return OpStatus::ERR;

	UINT8* table = data_holder->getData();
	offset_table.parseTable(table, (UINT32)data_holder->getLength());

	return OpStatus::OK;
}


/** Read and set the checksum for the glyf table.
  * 
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeConverter::readGlyfChecksum()
{
	const UINT8* table;
	UINT16 table_length;
	DataHolder* data_holder = data_pointer.get();
	if (!data_holder)
		return OpStatus::ERR;

	if(OpStatus::IsError(GetSFNTTable(data_holder->getData(), (size_t)data_holder->getLength(), MakeSFNTTag("glyf"), table, table_length, &glyf_checksum)))
	{
		glyf_checksum = 0;
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}


/** Read and parse the SFNT table given by a certain SFNT table tag.
  *
  * @param table_tag 4 byte tag for table to read and parse
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeConverter::readSFNTTable(UINT32 table_tag)
{
	const UINT8* table;
	UINT16 table_length;

	DataHolder* data_holder = data_pointer.get();
	if (!data_holder)
		return OpStatus::ERR;

	if (!GetSFNTTable(data_holder->getData(), (size_t)data_holder->getLength(), table_tag, table, table_length))
		return OpStatus::ERR;

	switch( table_tag )
	{
		// Table 'head' is the general font header
		case MAKE_TAG('h','e','a','d'):
			return head_table.parseTable(table, table_length);

		// Table 'hhea' is the horizontal metrics header
		case MAKE_TAG('h','h','e','a'):
			return hhea_table.parseTable(table, table_length);

		// Table 'maxp' stores upper boundaries for several font properties, for example number of glyphs in a font.
		case MAKE_TAG('m','a','x','p'):
			return maxp_table.parseTable(table, table_length);

		// Table 'hmtx' stores horizontal metrics for glyphs.
		case MAKE_TAG('h','m','t','x'):
			if (maxp_table.getNumberOfGlyphs() == 0)
				return OpStatus::ERR;
			return hmtx_table.parseTable(table, table_length, maxp_table.getNumberOfGlyphs(), hhea_table.getNumberOfHMetrics());

		// Table 'loca' stores offsets in glyph table for all glyph indexes
		case MAKE_TAG('l','o','c','a'):
			if (maxp_table.getNumberOfGlyphs() == 0)
				return OpStatus::ERR;
			return loca_table.parseTable(table, table_length, maxp_table.getNumberOfGlyphs(), head_table.indexToLocFormatIsLong());

		// Table 'post' stores PostScript names for glyphs
		case MAKE_TAG('p','o','s','t'):
			RETURN_IF_ERROR(post_table.parseTable(table, table_length));
			RETURN_IF_ERROR(fontinfo.add("UnderlinePosition", head_table.convertUnit(post_table.getUnderlinePosition())));
			RETURN_IF_ERROR(fontinfo.add("UnderlineThickness", head_table.convertUnit(post_table.getUnderlineThickness())));
			return fontinfo.add("isFixedPitch", post_table.getIsFixedPitch() ? "true" : "false");

		// Table 'glyf' stores glyph outlines
		case MAKE_TAG('g','l','y','f'):
			return glyf_table.parseTable(table, table_length, table - data_holder->getData(), &loca_table);
	};

	return OpStatus::OK;
}


/** Read font name from font data
  *
  * @return OpStatus::OK if successful
  */
OP_STATUS TrueTypeConverter::readFontName()
{
	const UINT8* table;
	UINT16 table_length;

	DataHolder* data_holder = data_pointer.get();
	if (!data_holder)
		return OpStatus::ERR;

	if (!GetSFNTTable(data_holder->getData(), (size_t)data_holder->getLength(), MakeSFNTTag("name"), table, table_length))
		return OpStatus::ERR;

	// find name record
	uni_char* name16;
	if (OpStatus::IsSuccess(GetEnglishUnicodeName(table, table_length, 6, name16)))
	{
		OP_STATUS status = font_name.Set(name16);
		OP_DELETEA(name16);
		return status;
	}

	if (font_name.IsEmpty())
		RETURN_IF_ERROR(font_name.Set("<Unknown>"));

	return OpStatus::OK;
}


OpString8 TrueTypeConverter::getFontName() 
{
	if (font_name.IsEmpty())
	{
		if (OpStatus::IsError(readFontName()))
			font_name.Set("<Unknown>");
	}

	return font_name; 
}


/** Update the offset tables to have correct size and checksum fields by asking the tables that
  * might have changed values. This is needed to later calculate correct offsets and paddings. */
void TrueTypeConverter::updateOffsetTables()
{
	UINT32 offset = offset_table.getLength();

	AutoDeleteHead* table_record_list = offset_table.getTableRecordList();
	TrueTypeTableRecord* tr = static_cast<TrueTypeTableRecord*>(table_record_list->First());
	while (tr)
	{
		switch (tr->tag)
		{
		case MAKE_TAG('g','l','y','f'):
			tr->length = glyf_table.getLength();
			tr->checksum = glyf_table.getChecksum();
			break;
		case MAKE_TAG('h','e','a','d'):
			tr->length = head_table.getLength();
			tr->checksum = head_table.getChecksum();
			break;
		case MAKE_TAG('h','h','e','a'):
			tr->length = hhea_table.getLength();
			tr->checksum = hhea_table.getChecksum();
			break;
		case MAKE_TAG('m','a','x','p'):
			tr->length = maxp_table.getLength();
			tr->checksum = maxp_table.getChecksum();
			break;
		case MAKE_TAG('l','o','c','a'):
			tr->length = glyf_table.getLocaTable()->getLength();
			tr->checksum = glyf_table.getLocaTable()->getChecksum();
			break;
		case MAKE_TAG('h','m','t','x'):
			tr->length = glyf_table.getHmtxTable()->getLength();
			tr->checksum = glyf_table.getHmtxTable()->getChecksum();
			break;
		};

		tr->offset = offset;

		// Calculate next table offset and possible padding
		offset += tr->length;

		// TrueType spec requires tables to be 4 byte (long) aligned
		if (offset % 4)
		{
			tr->padding = 4 - offset % 4;
			offset += tr->padding;
		}

		tr = static_cast<TrueTypeTableRecord*>(tr->Suc());
	}
}


/** Update contents of SFNT tables before generating hexcode output */
void TrueTypeConverter::updateSFNTTables()
{
	maxp_table.setNumberOfGlyphs(glyf_table.getNumberOfGlyphs());

 	// Always use long format for loca tables in output
 	glyf_table.getLocaTable()->setFormatIsLong(head_table.indexToLocFormatIsLong());

	// Always output full hmetrics with both width and left side bearing in hmtx table, no extra lbearings
	hhea_table.setNumberOfHMetrics(glyf_table.getHmtxTable()->getNumberOfHMetrics());

	// Update max/min boundaries in tables
	maxp_table.setMaxContours(glyf_table.getMaxContours());
	
	INT16 x_min, y_min, x_max, y_max;
	glyf_table.getMaxBoundingBox(x_min, y_min, x_max, y_max);
	head_table.setMaxBoundingBox(x_min, y_min, x_max, y_max);

	hhea_table.setMaxAdvanceWidth(glyf_table.getHmtxTable()->getMaxAdvanceWidth());
	hhea_table.setMinLeftSideBearing(glyf_table.getHmtxTable()->getMinLeftSideBearing());
}


/** Generate the sfnts hex code data block containing the actual font data
  *
  * @param buffer buffer to write the PostScript output to
  * @return OpStatus::OK if successful.
  */
OP_STATUS TrueTypeConverter::generatePostScriptSFNTSData(PostScriptCommandBuffer& buffer)
{
	updateSFNTTables();
	updateOffsetTables();

	// Write offset table
	offset_table.generateSFNTData(buffer);

	bool add_padding;

	// Then write each table
	AutoDeleteHead* table_record_list = offset_table.getTableRecordList();
	TrueTypeTableRecord* tr = static_cast<TrueTypeTableRecord*>(table_record_list->First());
	while (tr)
	{
		add_padding = true;
		switch (tr->tag)
		{
		case MAKE_TAG('g','l','y','f'):
			RETURN_IF_ERROR(glyf_table.writeGlyfTableData(buffer));

 			for(UINT8 i = 0; i < tr->padding; i++)
 				RETURN_IF_ERROR(buffer.writeHexCodeUINT8(0));

 			RETURN_IF_ERROR(buffer.endHexCodeBlock());  // Make sure we don't get more tables in same block

			add_padding = false;
			break;

		case MAKE_TAG('h','e','a','d'):
			RETURN_IF_ERROR(buffer.fitHexCodeTable(tr->length));
			RETURN_IF_ERROR(head_table.generateSFNTData(buffer));
			break;

		case MAKE_TAG('h','h','e','a'):
			RETURN_IF_ERROR(buffer.fitHexCodeTable(tr->length));
			RETURN_IF_ERROR(hhea_table.generateSFNTData(buffer));
			break;

		case MAKE_TAG('m','a','x','p'):
			RETURN_IF_ERROR(buffer.fitHexCodeTable(tr->length));
			RETURN_IF_ERROR(maxp_table.generateSFNTData(buffer));
			break;

		case MAKE_TAG('l','o','c','a'):
			RETURN_IF_ERROR(buffer.fitHexCodeTable(tr->length));
			RETURN_IF_ERROR(glyf_table.getLocaTable()->generateSFNTData(buffer));
			break;

		case MAKE_TAG('h','m','t','x'):
			RETURN_IF_ERROR(buffer.fitHexCodeTable(tr->length));
			RETURN_IF_ERROR(glyf_table.getHmtxTable()->generateSFNTData(buffer));
			break;

		default:
			if (offset_table.checkSFNTTableCopied(tr->tag) && tr->copied_data)
			{
				// If table doesn't have TrueTypeTable subclass, just output the copied original data
				RETURN_IF_ERROR(buffer.fitHexCodeTable(tr->length));
				RETURN_IF_ERROR(buffer.writeHexCode(tr->copied_data, tr->length));
			}
			break;
		};

		// Padding with zeros if not last table, to follow Type42 spec which
		// requires SFNT tables to begin at 4 byte boundaries.
		if (add_padding && tr != static_cast<TrueTypeTableRecord*>(table_record_list->Last()))
			for(UINT8 i = 0; i < tr->padding; i++)
				RETURN_IF_ERROR(buffer.writeHexCodeUINT8(0));

		tr = static_cast<TrueTypeTableRecord*>(tr->Suc());
	}

	RETURN_IF_ERROR(buffer.endHexCodeBlock());

	return OpStatus::OK;
}


OP_STATUS TrueTypeConverter::getGlyphIdentifiers(UINT32 glyph_index, UINT8& ps_charcode, OpString8& glyph_name, bool& has_charcode, UINT8* font_data, UINT32 font_data_size)
{
	TrueTypeGlyph* glyph;
	RETURN_IF_ERROR(glyf_table.getGlyph(glyph_index, glyph, font_data, font_data_size, hmtx_table));
	
	ps_charcode = glyph->getPSCharcode();
	glyph->getGlyphName(glyph_name);
	has_charcode = glyph->hasCharcode();
	return OpStatus::OK;
}


/** Generate PostScript code for the Type42 conversion of this font.
  *
  * @param buffer buffer to write the PostScript output to
  * @param ps_fontname the PostScript name used to reference this font in the PostScript code
  * @return OpStatus::OK if successful.
  */
OP_STATUS TrueTypeConverter::generatePostScript(PostScriptCommandBuffer& buffer, OpString8& ps_fontname)
{
	OpString8 fontinfo_text;
	OpString8 font_bbox;
	RETURN_IF_ERROR(head_table.getFontBBox(font_bbox));

	RETURN_IF_ERROR(fontinfo.generate(fontinfo_text));

	RETURN_IF_ERROR(buffer.addFormattedCommand("%%%%BeginResource: font %s", ps_fontname.CStr()));
	RETURN_IF_ERROR(buffer.addFormattedCommand("%%!PS-TrueTypeFont-%d.%d-%d.%d", head_table.getTableVersion().integer, head_table.getTableVersion().fraction,
																				 head_table.getFontRevision().integer, head_table.getFontRevision().fraction));
	RETURN_IF_ERROR(buffer.addCommand(         "%%Creator: Opera"));
	RETURN_IF_ERROR(buffer.addFormattedCommand("%%- Original font name: %s", font_name.CStr()));
	RETURN_IF_ERROR(buffer.addCommand(         "9 dict dup begin"));
	RETURN_IF_ERROR(buffer.addFormattedCommand("  /FontName /%s def", ps_fontname.CStr()));
	RETURN_IF_ERROR(buffer.addCommand(         "  /FontType 42 def"));
	RETURN_IF_ERROR(buffer.addFormattedCommand("  /PaintType %d def", paint_type));
	RETURN_IF_ERROR(buffer.addCommand(         "  /FontMatrix [1 0 0 1 0 0] def"));
	RETURN_IF_ERROR(buffer.addFormattedCommand("  /FontBBox %s def", font_bbox.CStr()));

	OpString8 encoding_vector; 
	OpString8 char_string_defs;
	RETURN_IF_ERROR(glyf_table.getPostScriptGlyphMappingDefs(encoding_vector, char_string_defs));
	RETURN_IF_ERROR(buffer.addFormattedCommand("  /Encoding [\n%s ] def", encoding_vector.CStr()));

	RETURN_IF_ERROR(buffer.addFormattedCommand("  /FontInfo %s def", fontinfo_text.CStr()));

	RETURN_IF_ERROR(buffer.addCommand(         "  /sfnts ["));
	RETURN_IF_ERROR(generatePostScriptSFNTSData(buffer));
	RETURN_IF_ERROR(buffer.addCommand("] def"));

	RETURN_IF_ERROR(buffer.addFormattedCommand("  /CharStrings << %s >> def", char_string_defs.CStr()));
	RETURN_IF_ERROR(buffer.addFormattedCommand("end /%s exch definefont pop", ps_fontname.CStr()));
	RETURN_IF_ERROR(buffer.addCommand(         "%%EndResource"));

	return buffer.addCommand("");
}

#endif // VEGA_POSTSCRIPT_PRINTING
