/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Per Hedbor
** Peter Krefting
*/

#include "core/pch.h"

#ifdef TABLEMANAGER_DYNAMIC_REV_TABLES

#include "modules/encodings/tablemanager/reverse_table_builder.h"

#include "modules/encodings/tablemanager/tablemanager.h"
#include "modules/encodings/encoders/encoder-utility.h"

#ifdef NEEDS_RISC_ALIGNMENT
inline void EXTRACT_USHORT(uni_char &dest, const void *src)
{
	reinterpret_cast<char *>(&dest)[0] = reinterpret_cast<const char *>(src)[0];
	reinterpret_cast<char *>(&dest)[1] = reinterpret_cast<const char *>(src)[1];
}

inline void PUT_USHORT(void *dest, uni_char src)
{
	reinterpret_cast<char *>(dest)[0] = reinterpret_cast<char *>(&src)[0];
	reinterpret_cast<char *>(dest)[1] = reinterpret_cast<char *>(&src)[1];
}
#else
# define EXTRACT_USHORT(R,X) (R=(*reinterpret_cast<const uni_char *>(X)))
# define PUT_USHORT(R,X)     (*reinterpret_cast<uni_char *>(R)=(X))
#endif

ReverseTableBuilder::ReverseTableBuilder()
{
}

/**
 * The revbuild table contains several entries with this format:
 * <pre>
 *   char size, <size * char> name    : The name of the table the entries applies to
 *   char nrev, <nrev * <unichar start,unichar end>> noreverange : Range of codes not included in rev-table
 *   char next, <next * <unichar char,unichar value>> extras     : List of extra codes in rev-table
 * </pre>
 *
 * This table is then used by the AddExtras() and Exclude() functions.
 */
const void *ReverseTableBuilder::ExceptionTable(const char *table_name,
	const unsigned char *revbuild_table, unsigned long revbuild_table_size)
{
	if (!revbuild_table)
		return NULL;

	unsigned int l = op_strlen(table_name);
	const unsigned char *ptr = revbuild_table;

	while ((ptr = reinterpret_cast<const unsigned char *>
	              (op_memmem(ptr, revbuild_table_size - (ptr - revbuild_table),
	                         table_name, l))) != NULL)
	{
		if (ptr == revbuild_table)
		{
			// Can only happen with broken zz-revbuild table. Check
			// for it anyway to avoid buffer underrun reads.
			ptr += l;
		}
		else
		{
			// The name is preceded by the length of the name.
			if (ptr[-1] == l)
				return ptr + l;
			else
				ptr += l;
		}
	}
	return NULL;
}

/**
 * Given a table format and index, return the codepoint. Used for flat
 * (non-map) table types.
 */
uni_char ReverseTableBuilder::CodeForInd(
	TableFormat format, uni_char ind, int first)
{
#if defined ENCODINGS_HAVE_CHINESE || defined ENCODINGS_HAVE_JAPANESE || defined ENCODINGS_HAVE_KOREAN
	int cell, row, plane;
#endif
	
	switch (format)
	{
	default:
		OP_ASSERT(!"Hm. Odd format");
		// Fallthrough
	case USHORT:
		return ind + first;

#ifdef ENCODINGS_HAVE_JAPANESE
	case USHORT_ROWCELL_JIS0208:
		// JIS 208 is 94 rows with 94 cells in each row, offset by 0x2121.
		cell = ind % 94;
		row = ind / 94;
		cell += 0x21;
		row += 0x21;
		return (row << 8) | cell;
#endif // ENCODINGS_HAVE_JAPANESE

#ifdef ENCODINGS_HAVE_KOREAN
	case USHORT_ROWCELL_KSC5601:
		// This is somewhat more comples. Depending on the codepoint
		// the number of cells per row varies.
		//
		// Otherwise like JIS
		if (ind < 12460)
		{
			cell = ind % 178;
			row = ind / 178;
			if (cell > 51)
				cell += 6;
			if (cell > 25)
				cell += 6;
			cell += 0x41;
			row += 0x81;
		}
		else
		{
			ind -= 12460;
			cell = ind % 94;
			row = ind / 94;
			row += 0xc7;
			cell += 0xa1;
		}
		return (row << 8) | cell;
#endif // ENCODINGS_HAVE_KOREAN

#ifdef ENCODINGS_HAVE_CHINESE
	case USHORT_ROWCELL_BIG5:
		// Straight forward 191*191 ofset by 0xa140
		cell = ind % 191;
		row = ind / 191;
		cell += 0x40;
		row += 0xa1;
		return (row << 8) | cell;
#endif // ENCODINGS_HAVE_CHINESE

#ifdef ENCODINGS_HAVE_CHINESE
	case USHORT_ROWCELL_CNS11643:
		// 3 * 94 * 94, each plane individually offset by 0x2121
		row = ind / 94;
		cell = ind % 94;
		plane = (row / 93) + 1;
		row %= 93;
		row += 0x21;
		cell += 0x21;
		return (plane << 14) | (row << 7) | cell;
#endif // ENCODINGS_HAVE_CHINESE

#ifdef ENCODINGS_HAVE_CHINESE
	case USHORT_ROWCELL_GBK:
		// 191 * 191, offset by 0x8140
		row = ind / 191;
		cell = ind % 191;
		row += 0x81;
		cell += 0x40;
		return (row << 8) | cell;
#endif // ENCODINGS_HAVE_CHINESE
	}
}

/**
 * Returns TRUE if the entry should be excluded from the
 * reverse-table. 65533 is always excluded, otherwise look in the
 * except table. This could be optimized by remembering the last
 * offset in the table, but since the longest table is currently five
 * entries long, it's probably somewhat over-ambitious to do that.
 */
BOOL ReverseTableBuilder::Exclude(uni_char entry, uni_char val,
	const void *except, int &i)
{
	if (val == 65533)
		return TRUE;
	if (!except)
		return FALSE;

	const unsigned char *tmp = reinterpret_cast<const unsigned char *>(except) + 1;
	unsigned char nelems = tmp[-1];

	for (; i < nelems; i++, tmp += 4)
	{
		uni_char slot[2]; /* ARRAY OK 2009-03-02 johanh */
		OP_ASSERT(sizeof(uni_char) >= 2 * sizeof(char));
		char *z = reinterpret_cast<char *>(slot);
		z[0] = tmp[0];
		z[1] = tmp[1];
		z[2] = tmp[2];
		z[3] = tmp[3];

		if (slot[0] <= entry && entry <= slot[1])
			return TRUE;
	}
	return FALSE;
}

/**
 * Add extra entries (into ptr) to the table from the exception table
 * entry (except) if any.
 *
 * This is only done for ushort/ushort tables. If we ever get a
 * non-ushort/ushort table with extra entries this code will have to
 * be adjusted.
 */
int ReverseTableBuilder::AddExtras(const void *except, uni_char *ptr)
{
	if (!except)
		return 0;

	const unsigned char *tmp = reinterpret_cast<const unsigned char *>(except);

	tmp += tmp[0] * 4 + 1;		// Skip exclude-list
	unsigned char nelems;
	if ((nelems = tmp[0]) != 0)
	{
		op_memcpy(ptr, tmp + 1, nelems * 4);	// Copy elements.
		return nelems * 2;
	}
	return 0;
}


/**
 * Build a reverse table for the table in fwd.
 *
 * table_name used to find which of the reverse-tables to build for
 * dual tables (the first is a flat list of codepoints, indexed by the
 * Unicode code with an offset applied, and the second is a map from
 * Unicode to codepoint)
 */
char *ReverseTableBuilder::BuildRev(
	const char *table_name, const void *fwd, long fwd_len, const void *except,
	TableFormat format, int first, int rev1_len, long &len)
{
	char *real_rev = NULL;
	const uni_char *src = reinterpret_cast<const uni_char *>(fwd);
	int actual_len = 0;			// in uni_chars

	int exclude_state = 0;
	fwd_len >>= 1;

	if (format == USHORT)
	{
		unsigned char *rev;

		// Simple forward map. Generate a USHORT-USHORT table. Maximum
		// possible size is fwd_len * 3 + extra_in_except_len, so not
		// really any need for dynamic reallocation. We do try to
		// shrink the table once it has been generated.
		if (fwd_len > 256)		// Hm. Odd. More than 256 ushorts.
			return NULL;

		rev = OP_NEWA(unsigned char, fwd_len * 3 + 8);
		real_rev = reinterpret_cast<char *>(rev);

		if (!rev)				// OOM
			return NULL;

		for (int i = 0; i < fwd_len; i++)
		{
			uni_char code;

			EXTRACT_USHORT(code, src + i);
			if (!Exclude(i, code, except, exclude_state))
			{
				PUT_USHORT((rev + actual_len), code);
				actual_len += 2;
				rev[actual_len++] = i + first;
			}
		}
		op_qsort(rev, actual_len / 3, 3, unichar_compare);
		len = actual_len;
	}
	else if (table_name[op_strlen(table_name) - 1] == '2')
	{
		// The second part: This is a sorted list of entries 
		// <unicode> : <native>

		int allocated_len = 512;
		uni_char *rev = OP_NEWA(uni_char, allocated_len);
		if (!rev)
			return NULL; // OOM

		// Add exception entries.
		actual_len += AddExtras(except, rev);

		for (int i = 0; i < fwd_len; i++)
		{
			uni_char code;

			EXTRACT_USHORT(code, src + i);
			if (((code < first) || (code - first >= rev1_len))
				&& !Exclude(i, code, except, exclude_state) && code)
			{
				if (actual_len + 1 >= allocated_len)
				{
#ifdef ENC_AGGRESSIVE_REVTABLE_ALLOC
					allocated_len <<= 1;	// O(1), wastes up to 50% memory
#else
					allocated_len += 128;	// bad ordo, saves memory
#endif
					uni_char *tmp = OP_NEWA(uni_char, allocated_len);

					if (!tmp)
					{
						OP_DELETEA(rev);
						return NULL;	// OOM.
					}
					for (int i = 0; i < actual_len; i++)
						tmp[i] = rev[i];
					OP_DELETEA(rev);
					rev = tmp;
				}
				rev[actual_len++] = src[i];
				rev[actual_len++] = CodeForInd(format, i, first);
			}
		}

		if (allocated_len - actual_len > 100)
		{
			uni_char *tmp = OP_NEWA(uni_char, actual_len);

			if (tmp)
			{
				for (int i = 0; i < actual_len; i++)
					tmp[i] = rev[i];
				OP_DELETEA(rev);
				rev = tmp;
			}
		}
		// Sort the table.
		op_qsort(rev, actual_len >> 1, 4, unichar_compare);

		unsigned short *zip = reinterpret_cast<unsigned short *>(rev);
		int last = -1, lastv = -1, j = 0;
		for (int i2 = 0; i2 < (actual_len >> 1); i2 ++)
		{
			if (zip[i2 * 2] != last)
			{
				last  = zip[j * 2]     = zip[i2 * 2];
				lastv = zip[j * 2 + 1] = zip[i2 * 2 + 1];
				j ++;
			}
			else if (zip[i2 * 2 + 1] < lastv)
			{
				zip[j * 2 - 1] = lastv = zip[i2 * 2 + 1];
			}
		}
		len = j * 4;

		// All done.
		real_rev = reinterpret_cast<char *>(rev);
	}
	else
	{
		// Simple flat mapping.
		uni_char *rev = OP_NEWA(uni_char, rev1_len * 2);
		if (!rev)
			return NULL; // OOM
		real_rev = reinterpret_cast<char *>(rev);

		op_memset(rev, 0, rev1_len * 2);
		actual_len = rev1_len;

		for (int i = 0; i < fwd_len; i++)
		{
			if (((src[i] >= first) && (src[i] - first < rev1_len))
				&& !Exclude(i, src[i], except, exclude_state))
				rev[src[i] - first] = CodeForInd(format, i, first);
		}
		len = actual_len * 2;
	}

	return real_rev;
}

BOOL ReverseTableBuilder::HasReverse(TableCacheManager *table_manager, const char *table_name)
{
	if (!table_manager)
		return FALSE;

	// Check if we can generate it. If so, we add it to the list of
	// known tables.
	if (GetTableFormat(table_manager, table_name, NULL) != UNKNOWN)
	{
		// Add this table to the list of known tables. We do not
		// generate it now, that is postponed until we actually
		// need it.
		TableCacheManager::TableDescriptor new_table;
		new_table.table_name = OP_NEWA(char, op_strlen(table_name) + 1);
		if (new_table.table_name)
		{
			op_strcpy(const_cast<char*>(new_table.table_name), table_name);
			new_table.start_offset = 0;
			new_table.byte_count = 0;
			new_table.final_byte_count = 0;
			new_table.table_info = 0;
			new_table.table = NULL;
			new_table.table_was_allocated = FALSE;
			new_table.ref_count = 0;
			OP_STATUS rc = table_manager->AddTable(new_table);
			if (OpStatus::IsSuccess(rc))
			{
				return TRUE;
			}
			else
			{
				// Clean up
				OP_DELETEA(const_cast<char*>(new_table.table_name));
			}
		}
	}

	return FALSE;
}

/**
 * Return the table format we would use for the table, or UNKNOWN if
 * this is not a valid table.
 *
 * @param table_name Name of requested table
 * @param fw_table_name Name of source (forward) table (if known)
 * @return Format for reverse table
 */
ReverseTableBuilder::TableFormat ReverseTableBuilder::GetTableFormat(OpTableManager *table_manager, const char *table_name, const char *fw_table_name)
{
	const char *tmp;
	if ((tmp = op_strstr(table_name, "-rev")) != NULL)
	{
#ifdef ENCODINGS_HAVE_CHINESE
		if (strni_eq(table_name, "BIG5", 4))
		{
			return USHORT_ROWCELL_BIG5;
		}
		if (strni_eq(table_name, "CNS11643", 8))
		{
			return USHORT_ROWCELL_CNS11643;
		}
		if (strni_eq(table_name, "GBK", 3))
		{
			return USHORT_ROWCELL_GBK;
		}
#endif // ENCODINGS_HAVE_CHINESE
#ifdef ENCODINGS_HAVE_JAPANESE
		if (strni_eq(table_name, "JIS-0208", 7) ||
		    strni_eq(table_name, "JIS-0212", 7))
		{
			return USHORT_ROWCELL_JIS0208;
		}
#endif // ENCODINGS_HAVE_JAPANESE
#ifdef ENCODINGS_HAVE_KOREAN
		if (strni_eq(table_name, "KSC5601", 7))
		{
			return USHORT_ROWCELL_KSC5601;
		}
#endif // ENCODINGS_HAVE_KOREAN

		// Regular eight-bit table; check that the forward table
		// is available.
		char tmp_table_name[32]; /* ARRAY OK 2009-03-02 johanh - boundary checked */
		if (!fw_table_name)
		{
			size_t namelen = op_strlen(table_name);
			if (namelen > sizeof tmp_table_name)
			{
				// This should never occur, since we don't have names that are
				// this long. But better safe than sorry.
				namelen = sizeof tmp_table_name;
			}
			op_memcpy(tmp_table_name, table_name, namelen);
			*(tmp_table_name + (tmp - table_name)) = 0;
			fw_table_name = tmp_table_name;
		}

		if (table_manager && table_manager->Has(fw_table_name))
		{
			return USHORT;
		}
	}

	// Not a reverse table, or table format not known
	return UNKNOWN;
}

void ReverseTableBuilder::BuildTable(OpTableManager* table_manager, TableCacheManager::TableDescriptor *table)
{
	if (!table_manager)
		return;

	char *fw_table_name, *tmp;
	const void *fw_table, *exception_table;
	long fw_table_size;
	TableFormat format = UNKNOWN;

	fw_table_name = OP_NEWA(char, op_strlen(table->table_name) + 9);
	if (!fw_table_name)
		return; // OOM

	op_strcpy(fw_table_name, table->table_name);

	if ((tmp = op_strstr(fw_table_name, "-rev")))
		*tmp = 0;
	else
		return;

	fw_table = table_manager->Get(fw_table_name, fw_table_size);
	if (!fw_table)
	{
		op_strcat(fw_table_name, "-table");
		fw_table = table_manager->Get(fw_table_name, fw_table_size);
	}

	if (!fw_table)				// No forward table found. Cannot build rev-table
	{
		OP_DELETEA(fw_table_name);
		return;
	}

	uni_char cutoff = 0x4e00, table1_size = 0;

	format = GetTableFormat(table_manager, table->table_name, fw_table_name);
	switch (format)
	{
#ifdef ENCODINGS_HAVE_CHINESE
	case USHORT_ROWCELL_BIG5:
		table1_size = 20901;
		break;

	case USHORT_ROWCELL_CNS11643:
		table1_size = 20902;
		break;

	case USHORT_ROWCELL_GBK:
		table1_size = 20902;
		break;
#endif // ENCODINGS_HAVE_CHINESE

#ifdef ENCODINGS_HAVE_JAPANESE
	case USHORT_ROWCELL_JIS0208:
		table1_size = 20897;
		break;
#endif // ENCODINGS_HAVE_JAPANESE

#ifdef ENCODINGS_HAVE_KOREAN
	case USHORT_ROWCELL_KSC5601:
		cutoff = 0xac00;
		table1_size = 11172;
		break;
#endif // ENCODINGS_HAVE_KOREAN

	case USHORT:
		if (fw_table_size == 256 || fw_table_size == 512)
		{
			if (fw_table_size == 256)
				cutoff = 128;
			else
				cutoff = 0;

			break;
		}
		/* else fall through */
	default: // UNKNOWN
		if (fw_table)
		{
			table_manager->Release(fw_table);
		}

		OP_DELETEA(fw_table_name);
		return;
	}

	long revbuild_table_size;
	const unsigned char *revbuild_table =
		reinterpret_cast<const unsigned char *>
		(table_manager->Get("zz-revbuild", revbuild_table_size));

	exception_table =
		ExceptionTable(fw_table_name, revbuild_table, revbuild_table_size);
	OP_DELETEA(fw_table_name);

	char *res = BuildRev(table->table_name, fw_table, fw_table_size,
						 exception_table, format, cutoff,
						 table1_size,
						 table->byte_count);

	table->final_byte_count = table->byte_count;
	table->table_was_allocated = TRUE;

	if (revbuild_table)
		table_manager->Release(revbuild_table);

	table_manager->Release(fw_table);
	table->table = reinterpret_cast<const UINT8*>(res);
}
#endif // TABLEMANAGER_DYNAMIC_REV_TABLES
