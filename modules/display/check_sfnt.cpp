/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * wonko
 */

// STANDALONE_CHECK_SFNT is used to compile this file (with some
// wrapper code) as a stand-alone font checking tool, which might be
// of use to QA.

#include "core/pch.h"

#ifndef STANDALONE_CHECK_SFNT

#include "modules/doc/frm_doc.h"
#include "modules/console/opconsoleengine.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opautoptr.h"
#include "modules/display/check_sfnt.h"
#include "modules/display/sfnt_base.h"

#ifdef MDE_MMAP_MANAGER
# include "modules/libgogi/mde_mmap.h"
#endif // MDE_MMAP_MANAGER

#endif // !STANDALONE_CHECK_SFNT

// compares stored and calculated checksum for all tables in the font
// some fonts have broken checksums - if this is enabled they will not be loaded
// #define VERIFY_TABLE_CHECKSUM

// compares stored and calculated checksum for the font
// some fonts have broken checksums - if this is enablked they will not be loaded
// #define VERIFY_FONT_CHECKSUM

// slow - traverses all entries in loca table and compares to bounds of glyf table
#define VERIFY_LOCA_ENTRIES

// checks the ftType entry in OS/2 table to see if font is allowed to be embedded
// this should be enabled, or we might violate font licensing rights

// update 2008-11-05 - howcome's informed us we should ignore this
// flag, until we're (perhaps) told to do otherwize.
// #define CHECK_EMBED_FLAGS

// verifies PKCS#7 signature (for signed fonts)
// currently not implemented - haavardm is working on support for DSIG verification
//#define VERIFY_DSIG

// verifies cmap table - used by OpFontManager implementations, so
// it's probably a good idea to check this since we're not sure if
// implementations can handle corrupt data gracefully
#define VERIFY_CMAP

// outputs information of what we considered broken in the font. good
// for debuging, but exposes our error detection approaches, which
// might aid creating "evil" fonts. should be left off, at least for
// public builds.
// #define INCLUDE_DETAILS_IN_ERROR_OUTPUT


#ifndef STANDALONE_CHECK_SFNT

#define BAIL(err, details, url, doc) do {					\
        ConsoleError(UNI_L(err), UNI_L(details), url, doc);	\
        return FALSE;										\
    } while (0)

OP_STATUS ConsoleError(const uni_char* msg, const uni_char* details,
					   const URL* url, FramesDocument* doc)
{
#ifdef OPERA_CONSOLE
    if (g_console && g_console->IsLogging())
    {
        OpConsoleEngine::Severity severity = OpConsoleEngine::Information;
        OpConsoleEngine::Source source = OpConsoleEngine::Internal;

        OpConsoleEngine::Message cmessage(source, severity);

        // Set the window id.
        if (doc && doc->GetWindow())
            cmessage.window = doc->GetWindow()->Id();

        const uni_char* info = UNI_L("webfont discarded - ");
        const UINT32 infolen = uni_strlen(info);
		UINT32 msglen = uni_strlen(msg);

#ifdef INCLUDE_DETAILS_IN_ERROR_OUTPUT
		const UINT32 extralen = details ? uni_strlen(details) + 2 : 0;
        // Set the error message itself.
        uni_char* message_p = cmessage.message.Reserve(infolen + msglen + extralen + 1);
#else // INCLUDE_DETAILS_IN_ERROR_OUTPUT
        // Set the error message itself.
        uni_char* message_p = cmessage.message.Reserve(infolen + msglen + 1);
#endif // INCLUDE_DETAILS_IN_ERROR_OUTPUT

        if (!message_p)
            return OpStatus::ERR_NO_MEMORY;

        uni_strcpy(message_p, info);
		message_p += infolen;
        uni_strcpy(message_p, msg);

#ifdef INCLUDE_DETAILS_IN_ERROR_OUTPUT
		if (extralen)
		{
			message_p += msglen;
			uni_strcpy(message_p, UNI_L(": "));
			message_p += 2;
			uni_strcpy(message_p, details);
		}
#endif // INCLUDE_DETAILS_IN_ERROR_OUTPUT

        if (url)
            RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, cmessage.url));

        g_console->PostMessageL(&cmessage);
    }
#endif // OPERA_CONSOLE
    return OpStatus::OK;
}

#endif // !STANDALONE_CHECK_SFNT

enum required_table
{
    r_cmap = 0,
    r_glyf,
    r_head,
    r_hhea,
    r_hmtx,
    r_loca,
    r_maxp,
    r_name,
    r_post,
    r_count
};
required_table GetTabIdx(const UINT32 tag)
{
    switch (tag)
    {
    case MAKE_TAG('c','m','a','p'):
        return r_cmap;
        break;
    case MAKE_TAG('g','l','y','f'):
        return r_glyf;
        break;
    case MAKE_TAG('h','e','a','d'):
        return r_head;
        break;
    case MAKE_TAG('h','h','e','a'):
        return r_hhea;
        break;
    case MAKE_TAG('h','m','t','x'):
        return r_hmtx;
        break;
    case MAKE_TAG('l','o','c','a'):
        return r_loca;
        break;
    case MAKE_TAG('m','a','x','p'):
        return r_maxp;
        break;
    case MAKE_TAG('n','a','m','e'):
        return r_name;
        break;
    case MAKE_TAG('p','o','s','t'):
        return r_post;
        break;
    default:
        return r_count;
    }
}

struct FontData
{
    FontData(const UINT8* data, const UINT32 size, const URL* url, FramesDocument* doc)
        : data(data), size(size), url(url), doc(doc), type(size >= 4 ? GetFontType(GetU32(data)) : t_unknown)
    {}

	FontData(const FontData& d, const FontType t)
		: data(d.data), size(d.size), url(d.url), doc(d.doc), type(t)
	{}

    const UINT8* const data;
    const UINT32 size;
    const URL* const url;
    FramesDocument* const doc;
    const FontType type;
};

/**
   check if table/subtable has a valid size. needed because some fonts
   store padded size. compares recorded to both unmodified and
   4-byte-padded expected.
   @param recorded the size stored in the table/subtable entry
   @param ecpected the expected size, based on table contents
   @return TRUE if the table size is valid, FALSE otherwise
 */
static inline
BOOL valid_size(size_t recorded, size_t expected)
{
	return
		recorded == expected ||
		// some fonts record the padded length, though this is not correct
		recorded == (((expected + 3) >> 2) << 2);
}

#ifdef VERIFY_TABLE_CHECKSUM // or the #if 0 below, if it ever changes condition.
static UINT32 CalculateTableCheckSum(const UINT8* font, size_t size, const TrueTypeTableRecord& tab)
{
	OP_ASSERT(tab.offset + tab.length <= size);
	// checksum adjustment shouldn't be included in head checksum
	if (tab.tag == MAKE_TAG('h', 'e', 'a', 'd'))
	{
		OP_ASSERT(tab.length > 12);
		return
			SFNTChecksumAdder::CalculateChecksum(font + tab.offset, 8) +
			SFNTChecksumAdder::CalculateChecksum(font + tab.offset + 12, tab.length - 12);
	}
	return SFNTChecksumAdder::CalculateChecksum(font + tab.offset, tab.length);
}
#endif // VERIFY_TABLE_CHECKSUM

#if 0 // <unused>
// assumes font is mutable
static void RecalculateTableCheckSum(UINT8* font, size_t size, const TrueTypeTableRecord& tab)
{
	UINT32 sum = CalculateTableCheckSum(font, size, tab);

	// find record
	UINT8* r = font + 12u;
	while (GetU32(r) != tab.tag)
		r += 16u;

	// write new checksum
	OP_ASSERT(r < font + size);
	SetU32(r + 4, sum);
}
#endif // </unused>

// assumes font is mutable
static void RecalculateFontCheckSum(UINT8* font, size_t size, const TrueTypeTableRecord* tabs, UINT32 tabCount)
{
	UINT32 sum;
	UINT32 adj_offs = 0;

	// update checksums for all tables
	const UINT32 headtag = MAKE_TAG('h','e','a','d');
	for (UINT32 i = 0; i < tabCount; ++i)
	{
		// need to zero adjustment before calculating checksum of head table
		if (tabs[i].tag == headtag)
		{
			OP_ASSERT(adj_offs == 0);
			adj_offs = tabs[i].offset + 8;
			OP_ASSERT(adj_offs + 4 <= tabs[i].offset + tabs[i].length);
			// zero adjustment
			SetU32(font + adj_offs, 0);
		}
		sum = SFNTChecksumAdder::CalculateChecksum(font + tabs[i].offset, tabs[i].length);
		SetU32(font + 12u + 16u*i + 4u, sum);
	}
	OP_ASSERT(adj_offs > 0);

	// calculate checksum for entire font and update adjustment
	sum = SFNTChecksumAdder::CalculateChecksum(font, size);
	sum = 0xb1b0afba - sum;
	SetU32(font + adj_offs, sum);
}

/**
   returns the correct size for a table type, or 0 if the table has no correct size
 */
#define SFNT_HEAD_TABLE_SIZE 54
UINT32 GetTabSize(const required_table tab, const UINT32 version)
{
    const UINT32 ver1 = 0x00010000;
    switch (tab)
    {
    case r_head:
        if (version == ver1)
            return SFNT_HEAD_TABLE_SIZE;
        break;
    case r_hhea:
        if (version == ver1)
            return 36;
        break;
    case r_maxp:
        switch (version)
        {
        case ver1:
            return 32;
            break;
        case 0x00005000:
            return 6;
            break;
        }
        break;
    }
    return 0;
}

#ifdef VERIFY_DSIG
BOOL CheckDSIG(const FontData& fd, const UINT32 offset, const UINT32 length)
{
    // FIXME: will default to TRUE - i.e. no verification is done
    // (though it will return FALSE for broken DSIG tables)

	const UINT8* font = fd.data;
	const UINT32 size = fd.size;  

    // not enough room for DSIG table
    if (size < offset + length)
        BAIL("malformed", fd.url, fd.doc);

    // not enough room for DSIG header
    if (length < 8)
        BAIL("malformed", fd.url, fd.doc);

    const UINT8* sig = font + offset;


    // DSIG header
    UINT32 version = GetU32(sig);
    UINT16 numSigs = GetU16(sig+4);
    UINT16 flags = GetU16(sig+6);

    // DEBUG
    fprintf(stderr, "font has %d signatures - version is 0x%.8x, flags is 0x%.4x\n",
            numSigs, version, flags);

    if (length < 8u + 12*numSigs)
        BAIL("malformed", fd.url, fd.doc);

    UINT32 o = 8;
    for (UINT32 i = 0; i < numSigs; ++i)
    {
        // format/offset table
        UINT32 sigFormat = GetU32(sig + o);
        UINT32 sigLen = GetU32(sig + o + 4);
        UINT32 sigOffs = GetU32(sig + o + 8);
        o += 12;

        // DEBUG
        fprintf(stderr, "* signature format 0x%.8x, length %d at %d\n",
                sigFormat, sigLen, sigOffs);

        // not enough room for signature block
        if (length < sigOffs + sigLen)
            BAIL("malformed", fd.url, fd.doc);

        // not enough room for signature block header
        if (sigLen < 8)
            BAIL("malformed", fd.url, fd.doc);

        // signature block
        const UINT8* sigBlock = sig + sigOffs;
        // reserved - should be zero
        if (GetU16(sigBlock) != 0 || GetU16(sigBlock+2) != 0)
            BAIL("malformed", fd.url, fd.doc);
        UINT32 pkcsLen = GetU32(sigBlock+4);

        if (sigLen != pkcsLen + 8)
            BAIL("malformed", fd.url, fd.doc); // maybe continue instead? (and bail at bottom)

        const UINT8* pkcsData = sigBlock + 8;

        // format 1 - whole font, with either truetype outlines and/or cff data
        if (sigFormat == 1)
        {
            // FIXME: check digital signature, probably something like
            // VerifyPKCS7DSIG(font, size, pkcsData - font, pkcsLen);
             OP_ASSERT(!"FIXME: DSIG verification not implemented");
        }
    }

    return TRUE;
}
#endif // VERIFY_DSIG



#ifdef VERIFY_CMAP
BOOL CheckCMAPData(const FontData& fd, const UINT8* cmap, const UINT32 size)
{
	// version should be 0
	const UINT16 version = GetU16(cmap);
	if (version != 0)
		BAIL("malformed", "CMAP version is non-zero", fd.url, fd.doc);

	if (size < 4)
		BAIL("malformed", "CMAP subtables don't fit in table", fd.url, fd.doc);
	// number of subtables
	UINT16 numSubTables = GetU16(cmap+2);
	// entries don't fit in table
	if (4u + 8u*numSubTables > size)
		BAIL("malformed", "CMAP subtables don't fit in table", fd.url, fd.doc);

	for (UINT32 i = 0; i < numSubTables; ++i)
	{
// 			const UINT16 platformID = GetU16(cmap + 4 + 8*i);
// 			const UINT16 platformSpecificID = GetU16(cmap + 4 + 8*i + 2);
		const UINT32 tabOffs = GetU32(cmap + 4 + 8*i + 4);


		const UINT8* tab = cmap + tabOffs;
		UINT32 format = GetU16(tab);
		UINT32 length; // length of current table, in bytes
		if (format <= 6)
		{
			if (tabOffs + 6 > size)
				BAIL("malformed", "premature end of CMAP table", fd.url, fd.doc);
			length = GetU16(tab + 2);
		}
		else if (format == 14)
		{
			if (tabOffs + 10 > size)
				BAIL("malformed", "premature end of CMAP table", fd.url, fd.doc);
			length = GetU32(tab + 2);
		}
		else
		{
			if (tabOffs + 12 > size)
				BAIL("malformed", "premature end of CMAP table", fd.url, fd.doc);
			length = GetU32(tab + 4);
		}

		if (length > size - tabOffs)
			BAIL("malformed", "premature end of CMAP table", fd.url, fd.doc);

		switch (format)
		{
		case 0:
		{
			// format 0 is 3*u16 + 256*u8 = 262 bytes big
			const UINT32 f0size = 262u;

			if (tabOffs + f0size > size)
				BAIL("malformed", "format 0 CMAP subtable out of range", fd.url, fd.doc);
			if (!valid_size(length, f0size))
				BAIL("malformed", "actual size of format 0 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);
			break;
		}

		case 2:
		{
			// format 2 is 3*u16 + 256*u16 + vaiable >= 518 bytes big
			const UINT32 f2basesize = 518u;

			if (length < f2basesize)
				BAIL("malformed", "malformed format 2 CMAP subtable", fd.url, fd.doc);

			// traverse subheaders
			for (UINT32 j = 0; j < 256; ++j)
			{
				UINT16 k = GetU16(tab + 6 + 2*j);

				if (k & 0x7)
					BAIL("malformed", "format 2 CMAP subtable subHeaderKey not divisible by eight", fd.url, fd.doc);
				if (length < f2basesize + k)
					BAIL("malformed", "format 2 CMAP subtable subHeaderKey out of bounds", fd.url, fd.doc);

				const UINT32 subHeaderOffs = f2basesize + k;

				// check sub header range
				if (tabOffs + subHeaderOffs + 8 > size)
					BAIL("malformed", "format 2 CMAP subtable subheader out of range", fd.url, fd.doc);

				UINT16 firstCode = GetU16(tab + subHeaderOffs);
				UINT16 entryCount = GetU16(tab + subHeaderOffs + 2);
				// UINT16 idDelta = GetU16(tab + subHeaderOffs + 4);
				UINT16 idRangeOffset = GetU16(tab + subHeaderOffs + 6);

				if (firstCode + entryCount > 255)
					BAIL("malformed", "format 2 CMAP subtable subheader subrange out of range", fd.url, fd.doc);
				if (entryCount == 0)
					BAIL("malformed", "empty format 2 CMAP subtable subheader", fd.url, fd.doc);
				if (subHeaderOffs + 6 + idRangeOffset + 2*entryCount > length)
					BAIL("malformed", "format 2 CMAP subtable subheader out of range", fd.url, fd.doc);
			}
			break;
		}

		case 4:
		{
			// format 4 is 7*u16 + variable 1*u16 + variable >= 16 bytes big
			const UINT32 f4basesize = 16u;

			if (length < f4basesize)
				BAIL("malformed", "malformed format 4 CMAP subtable", fd.url, fd.doc);
			UINT16 segCount2 = GetU16(tab + 6);
			if (segCount2 & 1)
				BAIL("malformed", "format 4 CMAP subtable 2*segcount not even", fd.url, fd.doc);
			if (length < f4basesize + 4u*segCount2)
				BAIL("malformed", "actual size of format 4 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);

			// reserved pad
			if (GetU16(tab + 14 + segCount2) != 0)
				BAIL("malformed", "malformed format 4 CMAP subtable - non-zero reserved pad", fd.url, fd.doc);

			const UINT8* endCode = tab + 14;
			const UINT8* startCode = tab + 14 + segCount2 + 2;
			const UINT8* rangeOffset = tab + 14 + 3*segCount2 + 2;
			const UINT32 segCount = segCount2 >> 1;
			UINT16 start, end = 0;
			for (UINT32 j = 0; j < segCount; ++j)
			{
				start = GetU16(startCode + 2*j);
				end = GetU16(endCode + 2*j);

				// last entry might be 0xffff to 0xffff
				if (j == segCount - 1 && start == 0xffff && start == end)
					continue;

				if (start <= end)
				{
					UINT16 offs = GetU16(rangeOffset + 2*j);
					if (offs)
					{
						UINT32 rangeEnd = offs + 2*(end - start) + (rangeOffset-tab) + 2*j;
						if (rangeEnd > length)
							BAIL("malformed", "format 4 CMAP subtable range out of bounds", fd.url, fd.doc);
					}
				}
				else
					BAIL("malformed", "format 4 CMAP subtable range inverted", fd.url, fd.doc);
			}
			if (end != 0xffff)
				BAIL("malformed", "last format 4 CMAP subtable doesn't end with 0xffff", fd.url, fd.doc);
			break;
		}

		case 6:
		{
			// format 6 is 5*u16 + vairable >= 10 bytes big
			const UINT32 f6basesize = 10u;

			if (length < f6basesize)
				BAIL("malformed", "malformed format 6 CMAP subtable", fd.url, fd.doc);
			UINT16 entryCount = GetU16(tab + f6basesize - 2);
			if (!valid_size(length, f6basesize + 2u*entryCount))
				BAIL("malformed", "actual size of format 6 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);
			break;
		}

		case 8:
		{
			// format 8 is 2*u16 + 2*u32 + 65536*u8 + 1*u32 + variable >= 65552 bytes big
			const UINT32 f8basesize = 65552u;

			if (length < f8basesize)
				BAIL("malformed", "malformed format 8 CMAP subtable", fd.url, fd.doc);
			UINT32 nGroups = GetU32(tab + f8basesize - 4);
			if (!valid_size(length, f8basesize + 12*nGroups))
				BAIL("malformed", "actual size of format 8 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);
			break;
		}

		case 10:
		{
			// format 10 is 2*u16 + 4*u32 + variable >= 20 bytes big
			const UINT32 f10basesize = 20u;

			if (length < f10basesize)
				BAIL("malformed", "malformed format 10 CMAP subtable", fd.url, fd.doc);
			UINT32 numChars = GetU32(tab + f10basesize - 4);
			if (!valid_size(length, f10basesize + 2*numChars))
				BAIL("malformed", "actual size if format 10 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);
			break;
		}

		case 12:
		{
			// format 12 is 2*u16 + 3*u32 + variable >= 16 bytes big
			const UINT32 f12basesize = 16u;

			if (length < f12basesize)
				BAIL("malformed", "malformed format 12 CMAP subtable", fd.url, fd.doc);
			UINT32 nGroups = GetU32(tab + f12basesize - 4);
			if (!valid_size(length, f12basesize + 12*nGroups))
				BAIL("malformed", "actual size of format 12 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);
			break;
		}

		case 13:
		{
			// format 13 is 2*u16 + 3*u32 + variable >= 16 bytes big
			const UINT32 f13basesize = 16u;

			if (length < f13basesize)
				BAIL("malformed", "malformed format 13 CMAP subtable", fd.url, fd.doc);
			UINT32 nGroups = GetU32(tab + f13basesize - 4);
			if (!valid_size(length, f13basesize + 12*nGroups))
				BAIL("malformed", "actual size of format 13 CMAP subtable doesn't match expected size",
					 fd.url, fd.doc);
			break;
		}

		case 14:
		{
			// format 14 is 1*u16 + 2*u32 + variable >= 10 bytes big
			const UINT32 f14basesize = 10u;

			if (length < f14basesize)
				BAIL("malformed", "malformed format 14 CMAP subtable", fd.url, fd.doc);
			const UINT32 numVarSelectorRecords = GetU32(tab + f14basesize - 4);
			if (length < f14basesize + 11*numVarSelectorRecords)
				BAIL("malformed", "malformed format 14 CMAP subtable",
					 fd.url, fd.doc);

			UINT32 varSelector = 0;
			for (UINT32 r = 0; r < numVarSelectorRecords; ++r)
			{
				// UINT24
				const UINT32 _varSelector = encode_value<UINT32, 3>(tab + f14basesize + 11*r);
				if (r && _varSelector <= varSelector) // should be ordered
					BAIL("malformed", "malformed format 14 CMAP subtable",
						 fd.url, fd.doc);
				varSelector = _varSelector;

				const UINT32 defaultUVSOffset    = GetU32(tab + f14basesize + 11*r + 3);
				const UINT32 nonDefaultUVSOffset = GetU32(tab + f14basesize + 11*r + 7);

				if (defaultUVSOffset)
				{
					if (length < defaultUVSOffset + 4)
						BAIL("malformed", "malformed format 14 CMAP subtable",
							 fd.url, fd.doc);

					const UINT32 numUnicodeValueRanges = GetU32(tab + defaultUVSOffset);
					if (length < defaultUVSOffset + 4 + numUnicodeValueRanges*4)
						BAIL("malformed", "malformed format 14 CMAP subtable",
							 fd.url, fd.doc);

					UINT32 startUnicodeValue = 0;
					UINT32 endUnicodeValue = 0;
					for (UINT32 u = 0; u < numUnicodeValueRanges; ++u)
					{
						// UINT24
						const UINT32 _startUnicodeValue = encode_value<UINT32, 3>(tab + defaultUVSOffset + 4 + 4*u);
						if (u && _startUnicodeValue <= startUnicodeValue) // should be ordered
							BAIL("malformed", "malformed format 14 CMAP subtable",
								 fd.url, fd.doc);
						startUnicodeValue = _startUnicodeValue;
						if (u && endUnicodeValue >= startUnicodeValue) // should not overlap
							BAIL("malformed", "malformed format 14 CMAP subtable",
								 fd.url, fd.doc);
						const UINT8 additionalCount = tab[defaultUVSOffset + 4 + 4*u + 3];
						endUnicodeValue = startUnicodeValue + additionalCount;
						if (endUnicodeValue > 0xffffff) // no entries bigger than 0xffffff
							BAIL("malformed", "malformed format 14 CMAP subtable",
								 fd.url, fd.doc);
					}
				}

				if (nonDefaultUVSOffset)
				{
					if (length < nonDefaultUVSOffset + 4)
						BAIL("malformed", "malformed format 14 CMAP subtable",
							 fd.url, fd.doc);

					const UINT32 numUVSMappings = GetU32(tab + nonDefaultUVSOffset);
					if (length < nonDefaultUVSOffset + 4 + numUVSMappings*4)
						BAIL("malformed", "malformed format 14 CMAP subtable",
							 fd.url, fd.doc);

					UINT32 unicodeValue = 0;
					for (UINT32 u = 0; u < numUVSMappings; ++u)
					{
						// UINT24
						const UINT32 _unicodeValue = encode_value<UINT32, 3>(tab + nonDefaultUVSOffset + 4 + 5*u);
						if (_unicodeValue <= unicodeValue) // should be ordered
							BAIL("malformed", "malformed format 14 CMAP subtable",
								 fd.url, fd.doc);
						unicodeValue = _unicodeValue;
					}
				}
			}
			break;
		}

		default:
			BAIL("malformed", "unknown CMAP subtable format", fd.url, fd.doc);
		}
	}

	return TRUE;
}
#endif // VERIFY_CMAP

#define TAB_IN_RANGE(tab,size) ((tab).offset < (size) && (tab).length <= (size) - (tab).offset)

/**
   tries to verify font integrity
   @return OpStatus::OK if the font is ok, OpStatus::ERR if font is likely to be corrupt
 */
BOOL CheckSfntData(const FontData& fd, const UINT32 offset)
{
	const UINT8* font = fd.data;
	const UINT32 size = fd.size;

    if (offset >= size)
        BAIL("malformed", "offset to SFNT data out of range", fd.url, fd.doc);

    // too small for offset subtable
    if (size - offset < 12)
        BAIL("malformed", "SFNT data out of range", fd.url, fd.doc);

    UINT16 numTables = GetU16(font + offset + 4);

    // not enough tables
    if (fd.type == t_ttf && numTables < 9)
        BAIL("malformed", "not enough tables for ttf font", fd.url, fd.doc);

    // too small for table directory
    if (12u + numTables*16u > size - offset)
        BAIL("malformed", "SFNT data out of range", fd.url, fd.doc);

    TrueTypeTableRecord reqTabs[r_count];

    // check table directories
    UINT32 o = offset + 12;
    for (UINT32 i = 0; i < numTables; ++i)
    {
        // bounds-check table
        TrueTypeTableRecord tab(font + o);
		if (!TAB_IN_RANGE(tab,size))
            BAIL("malformed", "SFNT table out of range", fd.url, fd.doc);

        // check for required tables
        const required_table idx = GetTabIdx(tab.tag);
        if (idx < r_count)
        {
            // not enough room for table version
            if (tab.length < 4)
                BAIL("malformed", "malformed required table", fd.url, fd.doc);
            const UINT32 version = GetU32(font + tab.offset);
            // check size
            UINT32 tabSize = GetTabSize(idx, version);
            if (tabSize && !valid_size(tab.length, tabSize))
                BAIL("malformed", "required table size doesn't match expected", fd.url, fd.doc);

            reqTabs[idx].Read(font + o);
        }

#ifdef VERIFY_DSIG
        else if (tab.tag == MAKE_TAG('D','S','I','G'))
        {
            if (!CheckDSIG(fd, tab.offset, tab.length))
                return FALSE;
        }
#endif // VERIFY_DSIG

#ifdef CHECK_EMBED_FLAGS
        // check the fsType (embed) flag
        else if (tab.tag == MAKE_TAG('O','S','/','2'))
        {
            UINT16 fsType = GetU16(font + tab.offset + 8);
            // remove subsetting bit, since we always load the whole font
            fsType &= ~0x0100;

            // restricted embedding - not allowed to embed font at all
            if (fsType == 0x0002)
                BAIL("not allowed to be embedded", "", fd.url, fd.doc);

            // bitmap embedding only - not allowed to use font outlines
            // FIXME: we should have a way to signal this, and load
            // only the bitmaps rather than discard the font entirely
            if (fsType == 0x0200)
            {
                OP_ASSERT(!"FIXME: discarding font entirely because it's marked 'bitmap embedding only'");
                BAIL("outlines not allowed to be embedded", "", fd.url, fd.doc);
            }
        }
#endif // CHECK_EMBED_FLAGS

#ifdef VERIFY_TABLE_CHECKSUM
        {
            // table checksum
            UINT32 sum = CalculateTableCheckSum(font, size, tab);
            if (sum != tab.checksum)
                BAIL("table checksum mismatch", "", fd.url, fd.doc);
        }
#endif // VERIFY_TABLE_CHECKSUM

        o += 16;
    }

    // required table missing
    if (fd.type == t_ttf)
	{
        for (UINT32 i = 0; i < r_count; ++i)
            if (!reqTabs[i].offset)
                BAIL("required table missing", "", fd.url, fd.doc);
	}
	else
		if (!reqTabs[r_head].offset)
			BAIL("head table missing", "", fd.url, fd.doc);

#ifdef VERIFY_FONT_CHECKSUM
    {
        // font checksum
		OP_ASSERT(reqTabs[r_head].length > 12);
		// don't include the checksum adjustment
		UINT32 adj_offs = reqTabs[r_head].offset + 8;
		// not sure if this'll happen, but if it does font checksum's
		// not calculated correctly
		OP_ASSERT(adj_offs % 4 == 0);
		UINT32 fadj = GetU32(font + adj_offs);
		UINT32 sum = SFNTChecksumAdder::CalculateChecksum(font, adj_offs);
		adj_offs += 4;
		sum += SFNTChecksumAdder::CalculateChecksum(font + adj_offs, size - adj_offs);
        if (0xb1b0afba - sum != fadj)
            BAIL("font checksum mismatch", "", fd.url, fd.doc);
    }
#endif // VERIFY_FONT_CHECKSUM

    // FIXME: check the CFF table if it's present

    UINT16 numGlyphs = 0;

    // compare number of glyphs with number of metrics
    if (fd.type == t_ttf || (reqTabs[r_maxp].offset && reqTabs[r_hhea].offset && reqTabs[r_hmtx].offset))
    {
        numGlyphs = GetU16(font + reqTabs[r_maxp].offset + 4);
        if (!numGlyphs) // no glyphs, no point
            BAIL("contains no glyphs", "", fd.url, fd.doc);
        UINT16 numLongMetrics = GetU16(font + reqTabs[r_hhea].offset + reqTabs[r_hhea].length - 2);
        // more metrics than glyphs
        if (numLongMetrics > numGlyphs)
            BAIL("malformed", "more long metrics in hhea table than glyphs in font", fd.url, fd.doc);
        // assume every glyph has either a long or short horizontal metric
        if (!valid_size(reqTabs[r_hmtx].length, 4u*numLongMetrics + 2u*(UINT16)(numGlyphs-numLongMetrics)))
            BAIL("malformed", "size of hmtx table doesn't correspond to number of metrics",
				 fd.url, fd.doc);
    }

    if (fd.type == t_ttf || (reqTabs[r_loca].offset && reqTabs[r_glyf].offset))
    {
        // compare loca and glyf tables
	    UINT16 locaFormat = GetU16(font + reqTabs[r_head].offset + SFNT_HEAD_TABLE_SIZE - 4);
        if (locaFormat > 1) // invalid format
            BAIL("malformed", "unknown loca format", fd.url, fd.doc);
        UINT16 locaEntrySize = locaFormat ? 4 : 2;
        const size_t computed_size = (numGlyphs + 1u) * locaEntrySize;
        // each glyph should have a loca entry
        if (!valid_size(reqTabs[r_loca].length, computed_size))
            BAIL("malformed", "size of loca tables doesn't correspond to number of glyphs", fd.url, fd.doc);
        // make sure last loca entry points to end of glyf table
        const UINT8* lastLocaEntry = font + reqTabs[r_loca].offset + computed_size - locaEntrySize;
        const UINT32 lastLocaOffset = (locaEntrySize == 4 ? GetU32(lastLocaEntry) : (UINT32)(GetU16(lastLocaEntry) << 1));
        if (!valid_size(reqTabs[r_glyf].length, lastLocaOffset) &&
			// seems there are a bunch of these around (eg UnDotum), and it's no harm really
			lastLocaOffset + 4 != reqTabs[r_glyf].length)
            BAIL("malformed", "last loca offset doesn't correspond to size of loca table (or alternative)",
				 fd.url, fd.doc);

#ifdef VERIFY_LOCA_ENTRIES
        // SLOW: loop through all loca entries and verify that they're in bounds of glyf table
        o = reqTabs[r_loca].offset;
        for (UINT32 i = 0; i < numGlyphs; ++i)
        {
            UINT32 this_offset, next_offset;
            if (locaEntrySize == 2)
            {
                this_offset = GetU16(font + o) << 1;
                o += locaEntrySize;
                next_offset = GetU16(font + o) << 1;
            }
            else
            {
                this_offset = GetU32(font + o);
                o += locaEntrySize;
                next_offset = GetU32(font + o);
            }
            // table is not ordered
            if (this_offset > next_offset)
                BAIL("malformed", "loca table not ordered", fd.url, fd.doc);
            // glyph is out of range
            if (next_offset > reqTabs[r_glyf].length)
                BAIL("malformed", "glyph in loca table out of range", fd.url, fd.doc);
        }
#endif // VERIFY_LOCA_ENTRIES
    }

#ifdef VERIFY_CMAP
	if (fd.type == t_ttf || reqTabs[r_cmap].offset)
	{
		if (!CheckCMAPData(fd, font + reqTabs[r_cmap].offset, reqTabs[r_cmap].length))
			return FALSE;
	}
#endif // VERIFY_CMAP

    return TRUE;
}

/*
   calls CheckSfntData for all fonts in collection, and checks any DSIG.
 */
BOOL CheckTTC(const FontData& fd)
{
	const UINT8* font = fd.data;
	const UINT32 size = fd.size;

    OP_ASSERT(size >= 12);
    OP_ASSERT(GetU32(font) == MAKE_TAG('t','t','c','f'));

    // TTC header versions 1.0 and 2.0
#ifndef OPERA_BIG_ENDIAN
    const UINT32 ver1 = 0x00010000, ver2 = 0x00020000;
#else
    const UINT32 ver1 = 0x00001000, ver2 = 0x00002000;
#endif // OPERA_BIG_ENDIAN

    const UINT32 version = GetU32(font + 4);
    if (version != ver1 && version != ver2)
        return FALSE;

    UINT32 numFonts = GetU32(font + 8);

    // TTC header version 2.0 has a digital signature
    if (version == ver2)
    {
        const UINT32 fontsEnd = 12 + 4*numFonts;
        // not enough room for font list and DSIG header
        if (size < fontsEnd + 12)
            return FALSE;

        // check DSIG
        UINT32 dsigTag = GetU32(font + fontsEnd);
        if (dsigTag == MAKE_TAG('D','S','I','G'))
        {
            UINT32 dsigLen = GetU32(font + fontsEnd + 4);
            UINT32 dsigOffs = GetU32(font + fontsEnd + 8);
            if (!dsigLen || !dsigOffs) // maybe better to just skip if zero, but it indicates corruption
                return FALSE;
            // TTC DSIG table is supposed to be the last table in the file
            if (dsigOffs + dsigLen != size) // again: might be better to just skip
                return FALSE;

#ifdef VERIFY_DSIG
            if (!CheckDSIG(font, dsigOffs, dsigLen))
                return FALSE;
#endif // VERIFY_DSIG
        }
        // v2 ttc's should have a DSIG entry after the offset table
        else
            return FALSE;
    }
    // not enough room for font list
    else if (size < 12 + 4*numFonts)
        return FALSE;

	// all subfonts should be of type ttf
	FontData sub(fd, t_ttf);

    // process all fonts in file
    for (UINT32 i = 0; i < numFonts; ++i)
    {
        UINT32 offs = GetU32(font + 12 + 4*i);
        if (offs + 4 >= size) // maybe better to just skip this font, but it indicates corruption
            return FALSE;
        FontType type = GetFontType(GetU32(font + offs));
        if (type != t_ttf)
            return FALSE;
        if (!CheckSfntData(sub, offs)) // again: might be better to just skip this font
            return FALSE;
    }
        
    // all fonts checked out just fine
    return TRUE;
}

BOOL CheckSfnt(const FontData& fd)
{
    if (fd.size < 12)
        BAIL("malformed", "font too small", fd.url, fd.doc);
    if (fd.type == t_unknown)
        BAIL("malformed", "unknown type", fd.url, fd.doc);

    return fd.type == t_ttc ? CheckTTC(fd) : CheckSfntData(fd, 0);
}

BOOL CheckSfnt(const UINT8* font, const UINT32 size, const URL* url/* = 0*/, FramesDocument* doc/* = 0*/)
{
    FontData fd(font, size, url, doc);
    return CheckSfnt(fd);
}

#ifndef STANDALONE_CHECK_SFNT
OP_STATUS CheckSfnt(const uni_char* fn, const URL* url/* = 0*/, FramesDocument* doc/* = 0*/)
{
    if (!fn)
        return OpStatus::ERR;

	FontHandle handle;
	RETURN_IF_ERROR(handle.LoadFile(fn));
    const UINT8* font = handle.Data();
    UINT32 size = handle.Size();

    return CheckSfnt(font, size, url, doc) ? OpStatus::OK : OpStatus::ERR;
}
#endif // !STANDALONE_CHECK_SFNT

/**
   creates a format 0 name table containing the following entries:
   * Windows unicode BMP US English Font Family name
   * Windows unicode BMP US English Font Subamily name
   * Windows unicode BMP US English Unique font identifier (in the form "<name>-<subname>:<random 10-digit number>")
   * Windows unicode BMP US English Full font name
   * Windows unicode BMP US English Postscript name
   * Windows unicode BMP US English Preferred Family
   * Windows unicode BMP US English Preferred Subamily

   @param name the Font Family name (unicode BMP English, big endian)
   @param namelen the length of name, in bytes
   @param sub  the Font Subfamily name (unicode BMP English, big endian)
   @param tab (out) the created name table - caller obtains ownership and should OP_DELETEA
   @param len (in/out)
    in:  should contain the length (in bytes) of the original name table
    out: will be set to the length (in bytes) of the created name table
 */
OP_STATUS CreateNameTable(const UINT8* name, const UINT16 namelen,
						  const UINT8* sub,  const UINT16 sublen,
						  UINT8*& tab, UINT32& len)
{
	UINT16 i;
	const UINT16 origlen = len; // length of original name table - needed for byte alignment
	const UINT16 n = 7u; // number of entries in name table
	const UINT16 namestart = 6u + 12u*n; // start of name string data, relative to start of table

	// names share the same string - total string will be in the form: <name>-<subname>:<ten random digits>
	const UINT16 randlen = 10u; // number of random _digits_ (not bytes)
	const UINT16 fullnamelen = namelen + 2u + sublen;
	const UINT16 textlen = fullnamelen + 2u + 2u*randlen;

	const UINT16 neededlen = namestart + textlen;

	// apparently four byte alignment for rest of font needs to be maintained
	const int delta = (int)origlen - (int)neededlen;
	int delta4 = 4 * (delta / 4);
	if (delta < 0)
		delta4 -= 4;
	OP_ASSERT(!(delta4 % 4));
	len = origlen - delta4;
	OP_ASSERT(len >= neededlen);
	OP_ASSERT((len % 4) == ((UINT32)origlen % 4));

	tab = OP_NEWA(UINT8, len);
	if (!tab)
		return OpStatus::ERR_NO_MEMORY;

	// if Font Subfamily name is "Regular" Full font name is same as
	// Font Family name, otherwise Full font name is combination of
	// Font Family name and Font Subfamily name
	const BOOL regular = sublen == 14 && !op_memcmp(sub, "\0R\0e\0g\0u\0l\0a\0r", 14);

	const UINT16 suboffs = fullnamelen - sublen; // offset from start of name data to start of subname string
	// name id, length, offset
	UINT16 names[n][3] = {
		{  1, namelen, 0, },						// Font Family name
		{  2, sublen, suboffs, },					// Font Subfamily name
		{  3, textlen, 0, },						// Unique font identifier
		{  4, regular ? namelen : fullnamelen, 0, },// Full font name
		{  6, fullnamelen, 0, },					// Postscript name
		{  16, namelen, 0, },						// Preferred Family
		{  17, sublen, suboffs, },					// Preferred Subfamily
	};

	// write table header
	SetU16(tab + 0, 0);			// format
	SetU16(tab + 2, n);			// count
	SetU16(tab + 4, namestart);	// offset

	// write name records
	for (i = 0; i < n; ++i)
	{
		SetU16(tab + 6 + 12*i +  0, 3);				// windows
		SetU16(tab + 6 + 12*i +  2, 1);				// unicode BMP
		SetU16(tab + 6 + 12*i +  4, 0x409);			// English United States
		SetU16(tab + 6 + 12*i +  6, names[i][0]);	// name id
		SetU16(tab + 6 + 12*i +  8, names[i][1]);	// length
		SetU16(tab + 6 + 12*i + 10, names[i][2]);	// offset
	}

	// write strings
	UINT8* target = tab + namestart;
	// name
	OP_ASSERT(tab + namestart + names[0][2] == target);
	op_memcpy(target, name, namelen);
	target += namelen;
	// separator
	SetU16(target, '-');
	target += 2;
	// subname
	OP_ASSERT(tab + namestart + names[1][2] == target);
	op_memcpy(target, sub, sublen);
	target += sublen;
	// separator
	SetU16(target, ':');
	target += 2;
	// unique-ish ending
	for (i = 0; i < randlen; ++i)
	{
		SetU16(target, (op_rand() % 10) + '0');
		target += 2;
	}
	OP_ASSERT(target == tab + neededlen);

	// zero pad bytes
	if (neededlen < len)
	{
		const UINT16 delta = len - neededlen;
		op_memset(target, 0, delta);
		target += delta;
	}
	OP_ASSERT(target == tab + len);

	return OpStatus::OK;
}

// FIXME: this won't fly for TTCs!
OP_STATUS RenameFont(const FontData& fd, const UINT32 offset,
					 const uni_char* name,
					 const UINT8*& out_font, size_t& out_size,
					 uni_char** replaced_name/* = 0*/)
{
	const UINT8* in_font = fd.data;
	const UINT32 in_size = fd.size;

    OP_ASSERT(offset < in_size);
    OP_ASSERT(in_size - offset >= 12);

    const UINT16 numTables = GetU16(in_font + offset + 4);

    OP_ASSERT(fd.type != t_ttf || numTables >= 9);
    OP_ASSERT(offset + 12u + numTables*16u < in_size);

    TrueTypeTableRecord* const in_tabs = OP_NEWA(TrueTypeTableRecord, 2*numTables);
	if (!in_tabs)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<TrueTypeTableRecord> _tabp(in_tabs);
	TrueTypeTableRecord* const out_tabs = in_tabs + numTables;


    // collect table entries
    UINT32 o = offset + 12;
	int name_idx = -1;
#ifdef DEBUG_ENABLE_OPASSERT
	int head_idx = -1;
#endif // DEBUG_ENABLE_OPASSERT
	const  UINT32 nametag = MAKE_TAG('n', 'a', 'm', 'e');
	const  UINT32 headtag = MAKE_TAG('h', 'e', 'a', 'd');
	UINT16 i;
    for (i = 0; i < numTables; ++i)
    {
        // bounds-check table
        in_tabs[i].Read(in_font + o);
		OP_ASSERT(TAB_IN_RANGE(in_tabs[i],in_size));
		out_tabs[i].Read(in_font + o);

		// name tag
		if (in_tabs[i].tag == nametag)
		{
			OP_ASSERT(name_idx < 0);
			name_idx = i;
		}
		// head tag
		if (in_tabs[i].tag == headtag)
		{
			OP_ASSERT(head_idx < 0);
#ifdef DEBUG_ENABLE_OPASSERT
			head_idx = i;
#endif // DEBUG_ENABLE_OPASSERT
		}

        o += 16;
	}

	OP_ASSERT(name_idx >= 0);
	OP_ASSERT(head_idx >= 0);

	// verify that no table is overlapping the name table. in theory
	// this could happen, in which case the name table cannot be
	// replaced
    for (i = 0; i < numTables; ++i)
		if (i != name_idx &&
			(!(in_tabs[i].offset >= in_tabs[name_idx].offset + in_tabs[name_idx].length ||
			   in_tabs[i].offset + in_tabs[i].length <= in_tabs[name_idx].offset)))
		{
			OP_ASSERT(!"looks like this font contains overlapping tables\n");
			return OpStatus::ERR;
		}

	// fetch name to be replaced
	// FIXME: this is messy, and possibly not what's wanted - should
	// be solved from outside if needed
	OpAutoArray<uni_char> _replaced_name;
	if (replaced_name)
	{
		RETURN_IF_ERROR(GetEnglishUnicodeName(in_font + in_tabs[name_idx].offset,
											  in_tabs[name_idx].length,
											  4/* Full font name */,
											  *replaced_name));
		_replaced_name.reset(*replaced_name);
	}

	// convert name to big endian
	const UINT16 namelen = 2*uni_strlen(name);
	UINT8* const name_be = OP_NEWA(UINT8, namelen);
	if (!name_be)
		return OpStatus::ERR_NO_MEMORY;
	OpAutoArray<UINT8> _name_be(name_be);
	op_memcpy(name_be, name, namelen);
	SwitchEndian(name_be, namelen);

	// get subname
	const UINT8* sub;
	UINT16 sublen;
	if (!GetNameEntry(in_font + in_tabs[name_idx].offset, in_tabs[name_idx].length,
					  3, 1, 0x409, 2, // Windows, unicode BMP, US English, Font Subamily name
					  sub, sublen))
		return OpStatus::ERR;
	OP_ASSERT(sublen % 2 == 0);

	// create new name table
	UINT8* new_nametab;
	RETURN_IF_ERROR(CreateNameTable(name_be, namelen,
									sub, sublen,
									new_nametab, out_tabs[name_idx].length));
	OpAutoArray<UINT8> _namep(new_nametab);
	const INT32 name_delta = out_tabs[name_idx].length - in_tabs[name_idx].length;

#ifdef _DEBUG
	{
		// check names in new name table
		const UINT8* tname;
		UINT16 tlen;
		// name
		OP_ASSERT(GetNameEntry(new_nametab, out_tabs[name_idx].length,
							   3, 1, 0x409, 1,
							   tname, tlen));
		OP_ASSERT(tlen == namelen && !op_memcmp(name_be, tname, tlen));
		// subname
		OP_ASSERT(GetNameEntry(new_nametab, out_tabs[name_idx].length,
							   3, 1, 0x409, 2,
							   tname, tlen));
		OP_ASSERT(tlen == sublen && !op_memcmp(sub, tname, tlen));

		// convert big endian name back to little endian and compare to original
		SwitchEndian(name_be, namelen);
		OP_ASSERT(!op_memcmp(name_be, name, namelen));
	}
#endif // _DEBUG

	// allocate storage for output font
	OP_ASSERT(in_size + name_delta > 0);
	out_size = in_size + name_delta;
	UINT8* new_font = OP_NEWA(UINT8, out_size);
	if (!new_font)
		return OpStatus::ERR_NO_MEMORY;

	// adjust entries in out_tabs
	for (i = 0; i < numTables; ++i)
	{
		if (out_tabs[i].offset > out_tabs[name_idx].offset)
		{
			out_tabs[i].offset += name_delta;
		}
	}

	// copy initial header
	op_memcpy(new_font, in_font, 12);

	// write table directory
	for (i = 0; i < numTables; ++i)
		out_tabs[i].Write(new_font + 12u + 16u*i);

	// copy everything before name table to new font
	const size_t data_start = 12u + 16u*numTables;
	OP_ASSERT(out_tabs[name_idx].offset >= data_start);
	op_memcpy(new_font + data_start, in_font + data_start, out_tabs[name_idx].offset - data_start);
	// copy new name table to new font
	op_memcpy(new_font + out_tabs[name_idx].offset, new_nametab, out_tabs[name_idx].length);
	// copy everything after name table to new font
	const UINT32 in_name_end = in_tabs[name_idx].offset + in_tabs[name_idx].length;
	op_memcpy(new_font + out_tabs[name_idx].offset + out_tabs[name_idx].length, in_font + in_name_end, in_size - in_name_end);
	out_font = new_font;

	if (replaced_name)
		_replaced_name.release();

	// update global font checksum
	RecalculateFontCheckSum(new_font, out_size, out_tabs, numTables);
#ifdef _DEBUG
	// slow, but verifies integrity of new font
	OP_ASSERT(OpStatus::IsSuccess(CheckSfntData(fd, offset)));
#endif // _DEBUG

	return OpStatus::OK;
}

OP_STATUS RenameFont(const UINT8* font, UINT32 size, const uni_char* name,
					 const UINT8*& out_font, size_t& out_size,
					 uni_char** replaced_name/* = 0*/)
{
    FontData fd(font, size, 0, 0);
	return RenameFont(fd, 0, name, out_font, out_size, replaced_name);
}

OP_STATUS RenameFont(const uni_char* fn, const uni_char* name,
					 const UINT8*& out_font, size_t& out_size,
					 uni_char** replaced_name/* = 0*/)
{
	FontHandle handle;
	RETURN_IF_ERROR(handle.LoadFile(fn));
    const UINT8* font = handle.Data();
    UINT32 size = handle.Size();
	return RenameFont(font, size, name, out_font, out_size, replaced_name);
}

