/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGManagerImpl.h"

#include "modules/svg/src/svgdom/svgdompathseglistimpl.h"

#ifdef SVG_PATH_EXPENSIVE_SYNC_TESTS
# define CHECK_SYNC() CheckSync();
#else
# define CHECK_SYNC() ((void)0)
#endif

static const char s_svg_pathseg_type_chars[] =
{
	'?', 'Z', 'M', 'm', 'L', 'l', 'C', 'c', 'Q',
	'q', 'A', 'a', 'H', 'h', 'V', 'v', 'S', 's',
	'T', 't'
};

char SVGPathSeg::GetSegTypeAsChar() const
{
	return s_svg_pathseg_type_chars[info.type];
}

BOOL
SVGPathSeg::operator==(const SVGPathSeg& other) const
{
	return (m_seg_info_packed_init == other.m_seg_info_packed_init &&
			x.Close(other.x)   &&
			y.Close(other.y)   &&
			x1.Close(other.x1) &&
			y1.Close(other.y1) &&
			x2.Close(other.x2) &&
			y2.Close(other.y2));
}

#ifdef SVG_FULL_11
OP_STATUS SVGCompoundSegment::AddNormalizedCopy(const SVGPathSeg& seg)
{
	SVGPathSegObject* new_seg_obj = OP_NEW(SVGPathSegObject, (seg));
	if (!new_seg_obj)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = m_normalized_seg.Add(new_seg_obj);
	if (OpStatus::IsSuccess(status))
	{
		SVGObject::IncRef(new_seg_obj);
		return OpStatus::OK;
	}
	else
	{
		OP_DELETE(new_seg_obj);
		return status;
	}
}

OP_STATUS
SVGPathSegObject::Sync()
{
	OP_STATUS status = OpStatus::OK;
	if (member.compound)
	{
		if (member.idx == (UINT32)-1)
		{
			status = member.compound->Sync(this);
		}
		else
		{
			member.compound->InvalidateSeg();
		}
	}
	return status;
}

/* static */ void
SVGPathSegObject::Release(SVGPathSegObject* obj)
{
	if (obj)
	{
		if (obj->GetInList())
			obj->GetInList()->DropObject(obj);
		obj->member = Membership();
	}
}

BOOL
SVGPathSegObject::IsValid(UINT32 idx)
{
	// A pathseg object is valid as long as it still is member of a
	// list and has the same idx
	if (member.idx == (UINT32)-1)
	{
		// This pathseg is a un-normalized path segment and is valid
		// as long as it is member of a segment
		return member.compound != NULL;
	}
	else
		return (member.compound && member.idx == idx);
}
#endif // SVG_FULL_11

OP_STATUS SVGPathSeg::Clone(SVGPathSeg** outcopy) const
{
	SVGPathSeg* cpy = OP_NEW(SVGPathSeg, (*this));
	if(!cpy)
		return OpStatus::ERR_NO_MEMORY;

	*outcopy = cpy;
	return OpStatus::OK;
}

OpBpath::OpBpath() :
		SVGObject(SVGOBJECT_PATH),
		m_pathlist(NULL)
#ifdef COMPRESS_PATH_STRINGS
		, m_compressed_str_rep(NULL)
#endif // COMPRESS_PATH_STRINGS
{
}

OpBpath::~OpBpath()
{
#ifdef COMPRESS_PATH_STRINGS
	OP_DELETEA(m_compressed_str_rep);
#endif // COMPRESS_PATH_STRINGS
	OP_DELETE(m_pathlist);
}

OP_STATUS OpBpath::Make(OpBpath*& outpath, BOOL used_by_dom /* = TRUE */, UINT32 number_of_segments_hint /* = 0 */)
{
	outpath = NULL;

	OpBpath* path = OP_NEW(OpBpath, ());
	if(!path)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = path->SetUsedByDOM(used_by_dom);
	if(OpStatus::IsError(err))
	{
		OP_ASSERT(!"Error when setting used by DOM on path");
		OP_DELETE(path);
	}
	else
	{
		if (number_of_segments_hint > 0 &&
			path->m_pathlist->GetAsNormalizedPathSegList())
		{
			path->m_pathlist->GetAsNormalizedPathSegList()->SetAllocationStepSize(number_of_segments_hint);
		}

		outpath = path;
	}
	return err;
}

OP_STATUS OpBpath::MoveTo(const SVGNumberPair& coord, BOOL relative, BOOL is_explicit)
{
	return CommonMoveTo(coord.x, coord.y, relative, is_explicit);
}

OP_STATUS OpBpath::MoveTo(SVGNumber x, SVGNumber y, BOOL relative)
{
	return CommonMoveTo(x, y, relative);
}

OP_STATUS OpBpath::CommonMoveTo(SVGNumber x, SVGNumber y, BOOL relative, BOOL is_explicit)
{
	SVGPathSeg pseg;

	pseg.info.type = relative ? SVGPathSeg::SVGP_MOVETO_REL : SVGPathSeg::SVGP_MOVETO_ABS;
	pseg.info.is_explicit = is_explicit ? 1 : 0;
	pseg.x = x;
	pseg.y = y;

	return AddCopy(pseg);
}

OP_STATUS OpBpath::LineTo(const SVGNumberPair& coord, BOOL relative)
{
	return LineTo(coord.x, coord.y, relative);
}

OP_STATUS OpBpath::LineTo(SVGNumber x, SVGNumber y, BOOL relative)
{
	SVGPathSeg newseg;

	newseg.info.type = relative ? SVGPathSeg::SVGP_LINETO_REL : SVGPathSeg::SVGP_LINETO_ABS;
	newseg.x = x;
	newseg.y = y;

	return AddCopy(newseg);
}

OP_STATUS OpBpath::HLineTo(SVGNumber x, BOOL relative)
{
	SVGPathSeg newseg;

	newseg.info.type = relative ? SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL : SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS;
	newseg.x = x;
	newseg.y = 0;

	return AddCopy(newseg);
}

OP_STATUS OpBpath::VLineTo(SVGNumber y, BOOL relative)
{
	SVGPathSeg newseg;

	newseg.info.type = relative ? SVGPathSeg::SVGP_LINETO_VERTICAL_REL : SVGPathSeg::SVGP_LINETO_VERTICAL_ABS;
	newseg.x = 0;
	newseg.y = y;

	return AddCopy(newseg);
}

OP_STATUS OpBpath::CubicCurveTo(SVGNumber cp1x, SVGNumber cp1y,
								SVGNumber cp2x, SVGNumber cp2y,
								SVGNumber endx, SVGNumber endy,
								BOOL smooth, BOOL relative)
{
	SVGPathSeg newseg;

	if(smooth)
	{
		newseg.info.type = relative ? SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL : SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS;
	}
	else
	{
		newseg.info.type = relative ? SVGPathSeg::SVGP_CURVETO_CUBIC_REL : SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
	}

	newseg.x = endx;
	newseg.y = endy;
	newseg.x1 = cp1x;
	newseg.y1 = cp1y;
	newseg.x2 = cp2x;
	newseg.y2 = cp2y;

	return AddCopy(newseg);
}

OP_STATUS OpBpath::QuadraticCurveTo(SVGNumber cp1x, SVGNumber cp1y, SVGNumber endx, SVGNumber endy, BOOL smooth, BOOL relative)
{
	SVGPathSeg newseg;

	if(smooth)
	{
		newseg.info.type = relative ? SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL : SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS;
	}
	else
	{
		newseg.info.type = relative ? SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL : SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS;
	}

	newseg.x = endx;
	newseg.y = endy;
	newseg.x1 = cp1x;
	newseg.y1 = cp1y;

	return AddCopy(newseg);
}

OP_STATUS OpBpath::ArcTo(SVGNumber rx, SVGNumber ry,
						 SVGNumber xrot,
						 BOOL large,
						 BOOL sweep,
						 SVGNumber x,
						 SVGNumber y, BOOL relative)
{
#ifdef SVG_SUPPORT_ELLIPTICAL_ARCS
	SVGPathSeg newseg;

	newseg.info.type = relative ? SVGPathSeg::SVGP_ARC_REL : SVGPathSeg::SVGP_ARC_ABS;
	newseg.x = x;
	newseg.y = y;
	newseg.x1 = rx;
	newseg.y1 = ry;
	newseg.x2 = xrot;
	newseg.info.large = large ? 1 : 0;
	newseg.info.sweep = sweep ? 1 : 0;

	return AddCopy(newseg);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // SVG_SUPPORT_ELLIPTICAL_ARCS
}

OP_STATUS OpBpath::Close()
{
	SVGPathSeg newseg;

	newseg.info.type = SVGPathSeg::SVGP_CLOSE;

	return AddCopy(newseg);
}

/**
* Converts a quadratic command into a cubic command.
*
* @param cmd The path command for a quadratic curve (in absolute coordinates)
* @param lx Last X coordinate (default is 0)
* @param ly Last Y coordinate (default is 0)
* @returns SVGPathCommand* An allocated path cubic curve command
*/
void OpBpath::ConvertQuadraticToCubic(SVGNumber cp1x, SVGNumber cp1y, SVGNumber endx, SVGNumber endy, BOOL relative, SVGNumber curx, SVGNumber cury, SVGPathSeg& cubiccmd)
{
	SVGNumber cp1xabs = cp1x;
	SVGNumber cp1yabs = cp1y;
	SVGNumber endxabs = endx;
	SVGNumber endyabs = endy;
	if(relative)
	{
		cp1xabs += curx;
		cp1yabs += cury;
		endxabs += curx;
		endyabs += cury;
	}

	cubiccmd.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
	cubiccmd.x1 = curx + ((cp1xabs - curx)*2)/3;
	cubiccmd.y1 = cury + ((cp1yabs - cury)*2)/3;
	cubiccmd.x2 = cp1xabs + (endxabs - cp1xabs)/3;
	cubiccmd.y2 = cp1yabs + (endyabs - cp1yabs)/3;
	cubiccmd.x = endxabs;
	cubiccmd.y = endyabs;
}

#ifdef COMPRESS_PATH_STRINGS
#ifdef PATH_COMPRESSION_STATISTICS
class CompressionStatistics
{
public:
	int in_size;
	int out_size;
	int ascii_usage[128];
	CompressionStatistics()
	{
		for (int i = 0; i < 128; i++)
		{
			ascii_usage[i] = 0;
		}
	}

	~CompressionStatistics()
	{
		if (in_size > 0)
		{
			int saved = in_size - out_size;
			float saved_percent = 100.0f*(float)saved/(float)in_size;
			char output_string[400]; /* ARRAY OK 2009-04-23 ed */
			sprintf(output_string, "Paths strings: %d bytes were compressed to %d. That is a save of %d, i.e. %g%%\n",
				in_size, out_size, saved, saved_percent);
#ifdef WIN32
			OutputDebugStringA(output_string);
#else
			fprintf(stderr, "%s", output_string);
#endif // WIN32
		}

		for (int i = 0; i < 128; i++)
		{
			if (ascii_usage[i])
			{
				char output_string[400]; /* ARRAY OK 2009-04-23 ed */
				sprintf(output_string, "The letter '%c' (0x%x) was used %d times.\n",
					(char)i, i, ascii_usage[i]);
#ifdef WIN32
				OutputDebugStringA(output_string);
#else
				fprintf(stderr, "%s", output_string);
#endif // WIN32
			}
		}
	}
};

static CompressionStatistics g_compression_stats;
#endif // PATH_COMPRESSION_STATISTICS

// The compression is really good with numbers, worse with ASCII and sucks for general unicode
// 0-9 -> 0x0 - 0x9
// '.' -> 0xa
// '-' -> 0xb
// ' ' -> 0xc
// 'l' -> 0xd
// ',' -> 0xe

// Some common ascii letters gets one byte representation
// 'c' -> 0xf0
// 'M' -> 0xf1
// 'h' -> 0xf2
// 'v' -> 0xf3

// decoding
// 0xff -> next byte is a one byte ascii
// 0xfe -> next two bytes are two byte utf-16 (highest byte first, uni_char = a[0]*256+a[1])

static int GetSetOffset(uni_char c)
{
	unsigned int number_val = c - '0';
	if (number_val < 10)
	{
		return number_val;
	}
	if (c <= '.')
	{
		if (c == '.')
		{
			return 0xa;
		}
		if (c == '-')
		{
			return 0xb;
		}
		if (c == ' ')
		{
			return 0xc;
		}
		if (c == ',')
		{
			return 0xe;
		}
	}
	else if (c == 'l') // small L
	{
		return 0xd;
	}

	return -1;
}

static uni_char OffsetToChar(int offset)
{
	OP_ASSERT(offset >= 0 && offset <= 0xe);
	char c = "0123456789.- l,"[offset];
	return c;
}

static UINT8 GetAsciiCode(char c)
{
// 'c' -> 0xf0
// 'M' -> 0xf1
// 'h' -> 0xf2
// 'v' -> 0xf3
	switch(c)
	{
	case 'c': return 0xf0;
	case 'M': return 0xf1;
	case 'h': return 0xf2;
	case 'v': return 0xf3;
	case 's': return 0xf4;
	case 'z': return 0xf5;
	case ' ': return 0xf6; // Often adjacent an uncoded char
	case 'C': return 0xf7;
	case 'L': return 0xf8;
	case 'A': return 0xf9;
	}
	return 0xff;
}

static uni_char AsciiCodeDecode(UINT8 code)
{
	OP_ASSERT(code >= 0xf0 && code <= 0xf9);
	return "cMhvsz CLA"[code-0xf0];
}

/**
 * Compresses a path string into something that is roughly half the
 * size of the ascii representation. Almost as good as zlib but much
 * faster.
 *
 * If out_buf is NULL, then the needed size is returned.
 */
static unsigned PathCompress(const uni_char* in_buf, unsigned in_len,
							 UINT8* out_buf)
{
	unsigned in_offset = 0;
	unsigned out_offset = 0;
	// Depends on the existance of null termination
	while (in_offset < in_len)
	{
		int offset_1 = GetSetOffset(in_buf[in_offset]);
		if (offset_1 >= 0)
		{
			int offset_2 = GetSetOffset(in_buf[in_offset+1]);
			if (offset_2 >= 0)
			{
				// Compressable
				if (out_buf)
				{
					out_buf[out_offset] = static_cast<UINT8>(offset_1)<<4 | static_cast<UINT8>(offset_2);
				}
				out_offset++;
				in_offset += 2;
				continue;
			}
		}

		if (in_buf[in_offset] < 128)
		{
			// Store as ASCII (1-2 bytes)
#ifdef PATH_COMPRESSION_STATISTICS
			g_compression_stats.ascii_usage[in_buf[in_offset]]++;
#endif // PATH_COMPRESSION_STATISTICS

			UINT8 code = GetAsciiCode(static_cast<char>(in_buf[in_offset]));
			if (code == 0xff)
			{
				// This is stored in 2 bytes
				if (out_buf)
				{
					out_buf[out_offset++] = static_cast<UINT8>(0xff);
					out_buf[out_offset++] = static_cast<UINT8>(in_buf[in_offset++]);
				}
				else
				{
					in_offset++;
					out_offset += 2;
				}
			}
			else
			{
				// This is stored in 1 byte
				if (out_buf)
				{
					out_buf[out_offset++] = code;
				}
				else
				{
					out_offset++;
				}
				in_offset++;
			}
		}
		else
		{
			// Store as UTF-16, 3 bytes
			if (out_buf)
			{
				out_buf[out_offset++] = static_cast<UINT8>(0xfe);
				uni_char c = in_buf[in_offset++];
				out_buf[out_offset++] = static_cast<UINT8>((c & 0xff00) >> 8);
				out_buf[out_offset++] = static_cast<UINT8>(c & 0xff);
			}
			else
			{
				in_offset++;
				out_offset += 3;
			}
		}
	}
	return out_offset;
}

/**
 * Decompresses a string compressed with PathCompress into out_buf or
 * returns the buffer size needed (in chars) if out_buf is NULL.
 */
static unsigned PathDecompress(const UINT8* in_buf, unsigned in_len,
							   uni_char* out_buf)
{
	if (!out_buf)
	{
		unsigned in_offset = 0;
		unsigned out_offset = 0;
		// First calculate out_size
		while (in_offset < in_len)
		{
			UINT8 code = in_buf[in_offset];
			switch (code)
			{
			case 0xff:
				// ASCII
				in_offset+=2;
				out_offset++;
				break;
			case 0xfe:
				// UTF-16
				in_offset+=3;
				out_offset++;
				break;
			default:
				in_offset++;
				if (code >= 0xf0)
				{
					// Single byte ascii
					out_offset+=1;
				}
				else
				{
					out_offset+=2;
				}
			}
		}
		return out_offset + 1; // For a trailing NULL
	}

	unsigned in_offset = 0;
	unsigned out_offset = 0;
	while (in_offset < in_len)
	{
		UINT8 code = in_buf[in_offset];
		switch (code)
		{
		case 0xff:
			// ASCII
			in_offset++;
			out_buf[out_offset++] = in_buf[in_offset++];
			break;
		case 0xfe:
			// UTF-16
			in_offset++;
			{
				char c_1 = in_buf[in_offset++];
				char c_2 = in_buf[in_offset++];
				out_buf[out_offset++] = static_cast<int>(c_1) << 8 | static_cast<int>(c_2);
			}
			break;
		default:
			in_offset++;
			if (code >= 0xf0)
			{
				// Single byte ascii
				out_buf[out_offset++] = AsciiCodeDecode(code);
			}
			else
			{
				out_buf[out_offset++] = OffsetToChar((code & 0xf0) >> 4);
				out_buf[out_offset++] = OffsetToChar(code & 0x0f);
			}
		}
	}
	out_buf[out_offset++] = '\0'; // Is sometimes included in the compressed data and sometimes not
	return out_offset;
}

#if defined(SVG_DEBUG_OPBPATH_TESTCOMPRESSION)
struct TestCompression
{
	TestCompression()
	{
		const uni_char* teststrings[] =
		{
			NULL,
			UNI_L(""),
			UNI_L("0"),
			UNI_L("00"),
			UNI_L("000"),
			UNI_L("abc"),
			UNI_L("M 1 0"),
			UNI_L("x 1 0") // The x will be changed to a unicode char
		};

		const int teststring_count = sizeof(teststrings)/sizeof(*teststrings);

		for (int i = 0; i < teststring_count; i++)
		{
			OpString original, test_result;
			OpString8 compressed;
			unsigned compressed_len;
			OP_STATUS status = original.Set(teststrings[i]);
			if (i == teststring_count - 1)
			{
				*original.CStr() = 0x6543;
			}
			OP_ASSERT(OpStatus::IsSuccess(status));
			compressed_len = PathCompress(original, original.Length(), NULL);
			if (!compressed.Reserve(compressed_len))
			{
				OP_ASSERT(!"OOM");
			}
			UINT8* compressed_data = reinterpret_cast<UINT8*>(compressed.CStr());
			PathCompress(original, original.Length(), compressed_data);
			unsigned uncompressed_len = PathDecompress(compressed_data,
													   compressed_len, NULL);
			uni_char* out_buf = test_result.Reserve(uncompressed_len);
			if (!out_buf)
			{
				OP_ASSERT(!"OOM");
				return;
			}
			PathDecompress(compressed_data, compressed_len, out_buf);

			OP_ASSERT(original.Compare(test_result) == 0);
		}
	}
};

static TestCompression g_test;
#endif // SVG_DEBUG_OPBPATH_TESTCOMPRESSION


OP_STATUS
OpBpath::SetString(const uni_char *str, UINT32 len)
{
	OP_DELETEA(m_compressed_str_rep);
	m_compressed_str_rep = NULL;
	if (len == 0)
	{
		return OpStatus::OK;
	}
	unsigned compressed_len;
	compressed_len = PathCompress(str, len, NULL);
	m_compressed_str_rep = OP_NEWA(UINT8, compressed_len);
	if (!m_compressed_str_rep)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	PathCompress(str, len, m_compressed_str_rep);
	m_compressed_str_rep_length = compressed_len;
#ifdef PATH_COMPRESSION_STATISTICS
	g_compression_stats.in_size += len;
	g_compression_stats.out_size += compressed_len;
#endif // PATH_COMPRESSION_STATISTICS
	return OpStatus::OK;
}
#endif // COMPRESS_PATH_STRINGS


BOOL
OpBpath::GetInterpolatedCmd(const SVGPathSeg* c1,
							const SVGPathSeg* c2,
							SVGPathSeg& c3,
							SVGNumber where)
{
	if (c1->info.type != c2->info.type)
	{
		// FIXME: Interpolation between "compatible" segments
		// (such as L and l, H and h and so on)
		OP_ASSERT(0);
		return FALSE;
	}

	c3.info.type = c1->info.type;
	OP_ASSERT((c1->info.is_explicit ^ c2->info.is_explicit) == 0);
	c3.info.is_explicit = c1->info.is_explicit;

	switch(c1->info.type)
	{
		case SVGPathSeg::SVGP_CLOSE:
			return TRUE;
		case SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS:
		case SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL:
			c3.x = c1->x + (c2->x - c1->x) * where;
#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g\n", c1->GetSegTypeAsChar(), c3.x));
#endif
			return TRUE;
		case SVGPathSeg::SVGP_LINETO_VERTICAL_ABS:
		case SVGPathSeg::SVGP_LINETO_VERTICAL_REL:
			c3.y = c1->y + (c2->y - c1->y) * where;
#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g\n", c1->GetSegTypeAsChar(), c3.y));
#endif
			return TRUE;
		case SVGPathSeg::SVGP_MOVETO_ABS:
		case SVGPathSeg::SVGP_MOVETO_REL:
		case SVGPathSeg::SVGP_LINETO_ABS:
		case SVGPathSeg::SVGP_LINETO_REL:
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS:
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL:
		{
			c3.x = c1->x + (c2->x - c1->x) * where;
			c3.y = c1->y + (c2->y - c1->y) * where;
#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g, %g\n", c1->GetSegTypeAsChar(), c3.x, c3.y));
#endif
			return TRUE;
		}
		break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL:
		{
			c3.x = c1->x + (c2->x - c1->x) * where;
			c3.y = c1->y + (c2->y - c1->y) * where;
			c3.x1 = c1->x1 + (c2->x1 - c1->x1) * where;
			c3.y1 = c1->y1 + (c2->y1 - c1->y1) * where;
#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g, %g, %g, %g\n", c1->GetSegTypeAsChar(), c3.x, c3.y, c3.x1, c3.y1));
#endif
			return TRUE;
		}
		break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS:
		case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL:
		{
			c3.x = c1->x + (c2->x - c1->x) * where;
			c3.y = c1->y + (c2->y - c1->y) * where;
			c3.x2 = c1->x2 + (c2->x2 - c1->x2) * where;
			c3.y2 = c1->y2 + (c2->y2 - c1->y2) * where;
#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g, %g, %g, %g\n", c1->GetSegTypeAsChar(), c3.x, c3.y, c3.x2, c3.y2));
#endif
			return TRUE;
		}
		break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
		case SVGPathSeg::SVGP_CURVETO_CUBIC_REL:
		{
			c3.x = c1->x + (c2->x - c1->x) * where;
			c3.y = c1->y + (c2->y - c1->y) * where;
			c3.x1 = c1->x1 + (c2->x1 - c1->x1) * where;
			c3.y1 = c1->y1 + (c2->y1 - c1->y1) * where;
			c3.x2 = c1->x2 + (c2->x2 - c1->x2) * where;
			c3.y2 = c1->y2 + (c2->y2 - c1->y2) * where;
#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g, %g, %g, %g, %g, %g\n", c1->GetSegTypeAsChar(), c3.x, c3.y, c3.x1, c3.y1, c3.x2, c3.y2));
#endif
			return TRUE;
		}
		break;
#ifdef SVG_SUPPORT_ELLIPTICAL_ARCS
		case SVGPathSeg::SVGP_ARC_ABS:
		case SVGPathSeg::SVGP_ARC_REL:
		{
			c3.x = c1->x + (c2->x - c1->x) * where;
			c3.y = c1->y + (c2->y - c1->y) * where;
			c3.x1 = c1->x1 + (c2->x1 - c1->x1) * where;
			c3.y1 = c1->y1 + (c2->y1 - c1->y1) * where;
			c3.x2 = c1->x2 + (c2->x2 - c1->x2) * where;
			SVGNumber large1(c1->info.large);
			SVGNumber large2(c2->info.large);
			SVGNumber large3 = large1 + (large2 - large1) * where;
			c3.info.large = (large3.NotEqual(SVGNumber(0))) ? 1 : 0;

			SVGNumber sweep1(c1->info.sweep);
			SVGNumber sweep2(c2->info.sweep);
			SVGNumber sweep3 = sweep1 + (sweep2 - sweep1) * where;
			c3.info.sweep = (sweep3.NotEqual(SVGNumber(0))) ? 1 : 0;

#ifdef SVG_DEBUG_INTERPOLATION
			OP_DBG(("Interpolated: %c - %g, %g, %g, %g, %g, %d, %d\n", c1->GetSegTypeAsChar(), c3.x, c3.y, c3.x1, c3.y1, c3.x2, c3.info.large, c3.info.sweep));
#endif
			return TRUE;
		}
		break;
#endif // SVG_SUPPORT_ELLIPTICAL_ARCS
	}

	OP_ASSERT(!"Not reached");
	return FALSE;
}

/* virtual */ BOOL
OpBpath::IsEqual(const SVGObject& obj) const
{
	if (obj.Type() == SVGOBJECT_PATH)
	{
		const OpBpath& other = static_cast<const OpBpath&>(obj);

		if(GetCount() == other.GetCount())
		{
			OpAutoPtr<PathSegListIterator> iter1(GetPathIterator(TRUE));
			OpAutoPtr<PathSegListIterator> iter2(other.GetPathIterator(TRUE));
			if (!iter1.get() || !iter2.get())
				return FALSE;

			for(unsigned int i = 0; i < GetCount(); i++)
			{
				const SVGPathSeg* cmd1 = iter1->GetNext();
				const SVGPathSeg* cmd2 = iter2->GetNext();
				if (!cmd1 || !cmd2 || !(*cmd1 == *cmd2))
				{
					return FALSE;
				}
			}

			return TRUE;
		}
	}

	return FALSE;
}

void
OpBpath::PrepareInterpolatedCmd(const SVGPathSeg* c1, SVGNumberPair current_point, SVGPathSeg& out)
{
	// Start with a copy
	out = *c1;

	// Then add current_point as needed. This isn't a full normalize,
	// just a conversion from rel -> abs and short-hand to full-hand
	switch(c1->info.type)
	{
		case SVGPathSeg::SVGP_CURVETO_CUBIC_REL:
		case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL:
			out.x2 = c1->x2 + current_point.x;
			out.y2 = c1->y2 + current_point.y;
			/* Fall-through */
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL:
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL:
			out.x1 = c1->x1 + current_point.x;
			out.y1 = c1->y1 + current_point.y;
			/* Fall-through */
		case SVGPathSeg::SVGP_MOVETO_REL:
		case SVGPathSeg::SVGP_LINETO_REL:
		case SVGPathSeg::SVGP_ARC_REL:
			out.x = c1->x + current_point.x;
			out.y = c1->y + current_point.y;
	}

	// Convert path seg type
	if (c1->info.type == SVGPathSeg::SVGP_MOVETO_REL)
	{
		out.info.type = SVGPathSeg::SVGP_MOVETO_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_LINETO_REL)
	{
		out.info.type = SVGPathSeg::SVGP_LINETO_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_ARC_REL)
	{
		out.info.type = SVGPathSeg::SVGP_ARC_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_REL)
	{
		out.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL)
	{
		out.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL)
	{
		out.info.type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL)
	{
		out.info.type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS)
	{
		out.info.type = SVGPathSeg::SVGP_LINETO_ABS;
		out.x = c1->x;
		out.y = current_point.y;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL)
	{
		out.info.type = SVGPathSeg::SVGP_LINETO_ABS;
		out.x = c1->x + current_point.x;
		out.y = current_point.y;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_LINETO_VERTICAL_ABS)
	{
		out.info.type = SVGPathSeg::SVGP_LINETO_ABS;
		out.x = current_point.x;
		out.y = c1->y;
	}
	else if (c1->info.type == SVGPathSeg::SVGP_LINETO_VERTICAL_REL)
	{
		out.info.type = SVGPathSeg::SVGP_LINETO_ABS;
		out.x = current_point.x;
		out.y = c1->y + current_point.y;
	}
}

OP_STATUS
OpBpath::Interpolate(const OpBpath &p1, const OpBpath& p2, SVG_ANIMATION_INTERVAL_POSITION interval_position)
{
	BOOL append = (GetCount() == 0);

	// Used when making relative segments absolute and short-hand
	// values complete
	SVGNumberPair current_point_1, current_point_2;

	UINT32 p1c = p1.GetCount(FALSE);
	UINT32 p2c = p2.GetCount(FALSE);

	if(p1c == p2c)
	{
		OpAutoPtr<PathSegListIterator> iter1(p1.GetPathIterator(FALSE));
		OpAutoPtr<PathSegListIterator> iter2(p2.GetPathIterator(FALSE));
		OpAutoPtr<PathSegListIterator> iter3(NULL);

		if (!append)
			iter3.reset(GetPathIterator(FALSE));

		if (!iter1.get() || !iter2.get() || (!append && !iter3.get()))
			return OpStatus::ERR_NO_MEMORY;

		for(unsigned int i = 0; i < p2c; i++)
		{
			const SVGPathSeg* cmd1 = iter1->GetNext();
			const SVGPathSeg* cmd2 = iter2->GetNext();

			SVGPathSeg prepared_cmd1, prepared_cmd2;
			PrepareInterpolatedCmd(cmd1, current_point_1, prepared_cmd1);
			PrepareInterpolatedCmd(cmd2, current_point_2, prepared_cmd2);

			if (append)
			{
				SVGPathSeg cmd3;
				if (!GetInterpolatedCmd(&prepared_cmd1, &prepared_cmd2, cmd3, interval_position))
					return OpStatus::ERR_NOT_SUPPORTED;
				if (OpStatus::IsMemoryError(AddCopy(cmd3)))
				{
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			else
			{
				SVGPathSeg *cmd3 = NULL;
#ifdef SVG_FULL_11
				SVGPathSegObject *cmd3_obj = iter3->GetNextObject();
				cmd3 = cmd3_obj ? &cmd3_obj->seg : NULL;
#else
				cmd3 = iter3->GetNext();
#endif
				if (cmd3)
				{
					GetInterpolatedCmd(&prepared_cmd1, &prepared_cmd2, *cmd3, interval_position);
				}

#ifdef SVG_FULL_11
				if (cmd3_obj)
					OpStatus::Ignore(cmd3_obj->Sync());
#endif
			}

			current_point_1.x = prepared_cmd1.x;
			current_point_1.y = prepared_cmd1.y;

			current_point_2.x = prepared_cmd2.x;
			current_point_2.y = prepared_cmd2.y;
		}

		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
}

/* virtual */ OP_STATUS
OpBpath::Copy(const OpBpath &p1)
{
	CopyFlags(p1);
	if (OpStatus::IsError(m_pathlist->Copy(p1.m_pathlist)))
	{
		m_pathlist->Clear();
		return Concat(p1);
	}
	return OpStatus::OK;
}

/* virtual */ SVGObject *
OpBpath::Clone() const
{
	OpBpath* path;
	UINT32 lengthhint = m_pathlist->GetCount(FALSE);
	OP_STATUS status = OpBpath::Make(path, !!m_pathlist->GetAsSynchronizedPathSegList(), lengthhint);
	OP_ASSERT(OpStatus::IsSuccess(status) || OpStatus::IsMemoryError(status));
	if (OpStatus::IsError(status))
		return NULL;

	path->CopyFlags(*this);

	status = path->Concat(*this);
	if (OpStatus::IsSuccess(status))
	{
		if (m_compressed_str_rep)
		{
			// Need to clone the string representation also, in case the clone
			// will be used from DOM
			path->m_compressed_str_rep = OP_NEWA(UINT8, m_compressed_str_rep_length);
			if (!path->m_compressed_str_rep)
			{
				OP_DELETE(path);
				return NULL;
			}
			op_memcpy(path->m_compressed_str_rep, m_compressed_str_rep, m_compressed_str_rep_length);
			path->m_compressed_str_rep_length = m_compressed_str_rep_length;
		}
		return path;
	}
	else
	{
		OP_DELETE(path);
		return NULL;
	}
}

/* virtual */ OP_STATUS
OpBpath::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	// If we have the saved string, use that
#ifdef COMPRESS_PATH_STRINGS
	if (m_compressed_str_rep)
	{
		OpString str_rep_utf16;
		unsigned uncompressed_len = PathDecompress(m_compressed_str_rep,
												   m_compressed_str_rep_length,
												   NULL);
		uni_char* buf = str_rep_utf16.Reserve(uncompressed_len);
		if (!buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		PathDecompress(m_compressed_str_rep,
					   m_compressed_str_rep_length,
					   buf);
		return buffer->Append(buf);
	}
#endif // COMPRESS_PATH_STRINGS

	const SVGPathSeg* lastseg = NULL;
	OpAutoPtr<PathSegListIterator> iter(GetPathIterator(FALSE));
	if (!iter.get())
		return OpStatus::ERR_NO_MEMORY;

	unsigned int listcount = GetCount(FALSE);
	for (unsigned int i = 0; i < listcount; i++)
	{
		const SVGPathSeg* seg = iter->GetNext();

		switch(seg->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
		case SVGPathSeg::SVGP_MOVETO_REL:
			if(lastseg &&
			   lastseg->info.type == seg->info.type &&
			   seg->info.is_explicit == 0)
			{
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			else
			{
				RETURN_IF_ERROR(buffer->Append(seg->GetSegTypeAsChar()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS:
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL:
		case SVGPathSeg::SVGP_LINETO_ABS:
		case SVGPathSeg::SVGP_LINETO_REL:
			{
				RETURN_IF_ERROR(buffer->Append(seg->GetSegTypeAsChar()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL:
			{
				RETURN_IF_ERROR(buffer->Append(seg->GetSegTypeAsChar()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x1.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y1.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS:
		case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL:
			{
				RETURN_IF_ERROR(buffer->Append(seg->GetSegTypeAsChar()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x2.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y2.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
		case SVGPathSeg::SVGP_CURVETO_CUBIC_REL:
			{
				RETURN_IF_ERROR(buffer->Append(seg->GetSegTypeAsChar()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x1.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y1.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x2.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y2.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			break;
		case SVGPathSeg::SVGP_ARC_REL:
		case SVGPathSeg::SVGP_ARC_ABS:
			{
				RETURN_IF_ERROR(buffer->Append(seg->GetSegTypeAsChar()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x1.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y1.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x2.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->Append(seg->info.sweep + '0')); // seg->info.sweep is TRUE (1) or FALSE (0)
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->Append(seg->info.large + '0')); // seg->info.large is TRUE (1) or FALSE (0)
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->x.GetFloatValue()));
				RETURN_IF_ERROR(buffer->Append(' '));
				RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(seg->y.GetFloatValue()));
			}
			break;
		case SVGPathSeg::SVGP_CLOSE:
			{
				RETURN_IF_ERROR(buffer->Append("Z", 1));
			}
			break;
		}

		lastseg = seg;
	}
	return OpStatus::OK;
}

OP_STATUS
OpBpath::Concat(const OpBpath &p1)
{
	OpAutoPtr<PathSegListIterator> iter(p1.GetPathIterator(FALSE));
	if (!iter.get())
		return OpStatus::ERR_NO_MEMORY;

	for(unsigned int i = 0; i < p1.GetCount(FALSE); i++)
	{
		RETURN_IF_ERROR(AddCopy(*iter->GetNext()));
	}
	return OpStatus::OK;
}

static inline BOOL ExtendsBBox4(SVGNumber min, SVGNumber max, SVGNumber p1, SVGNumber p2)
{
	return (p1 < min || p2 < min || p1 > max || p2 > max);
}

#ifdef SVG_KEEP_QUADRATIC_CURVES
static inline BOOL ExtendsBBox3(SVGNumber min, SVGNumber max, SVGNumber p)
{
	return (p < min || p > max);
}

static inline SVGNumber EvaluateQuad(SVGNumber t, SVGNumber a, SVGNumber b, SVGNumber c)
{
	return c + t * (b + t * a);
}

static void CalcQuadExtrema(SVGNumber p0, SVGNumber p1, SVGNumber p2,
							SVGNumber& min, SVGNumber& max)
{
	SVGNumber a = p0 - p1 + p2 - p1;

	if (a.Close(0))
		return;

	SVGNumber b = -p0 + p1;
	SVGNumber t = -b / a;

	if (t <= 0 || t >= 1)
		return;

	SVGNumber p = EvaluateQuad(t, a, b * 2, p0);
	if (p > max)
		max = p;
	if (p < min)
		min = p;
}
#endif // SVG_KEEP_QUADRATIC_CURVES

static inline SVGNumber EvaluateCubic(SVGNumber t, SVGNumber a, SVGNumber b, SVGNumber c, SVGNumber d)
{
	return d + t * (c + t * (b + t * a));
}

static void CalcCubicExtrema(SVGNumber p0, SVGNumber p1, SVGNumber p2, SVGNumber p3,
							 SVGNumber& min, SVGNumber& max)
{
	SVGNumber t[2];
	unsigned int num_solutions = 0;

	SVGNumber a = -p0 + p1 * 3 - p2 * 3 + p3;
	SVGNumber b = p0 - p1 * 2 + p2;
	SVGNumber c = -p0 + p1;

	SVGNumber div = a * 2;
	if (div.Close(0))
	{
		if (b.Close(0))
			return; // No extrema

		t[num_solutions++] = -c / (b * 2);
	}
	else
	{
		SVGNumber discr = b * b * 4 - a  * c * 4;
		if (discr < 0)
			return; // No real-valued solutions

		if (discr.Close(0))
		{
			t[num_solutions++] = (-b * 2) / div;
		}
		else
		{
			discr = discr.sqrt();

			t[num_solutions++] = (-b * 2 + discr) / div;
			t[num_solutions++] = (-b * 2 - discr) / div;
		}
	}

	OP_ASSERT(num_solutions > 0);
	for (unsigned int i = 0; i < num_solutions; ++i)
	{
		if (t[i] <= 0 || t[i] >= 1)
			continue;

		// a = -p0 + p1 * 3 - p2 * 3 + p3;
		// b = (p0 - p1 * 2 + p2) * 3;
		// c = (-p0 + p1) * 3;
		// d = p0
		SVGNumber p = EvaluateCubic(t[i], a, b * 3, c * 3, p0);
		if (p > max)
			max = p;
		if (p < min)
			min = p;
	}
}

void OpBpath::UpdateBoundingBox(const SVGNumberPair& curr_pt, SVGPathSeg const * cmd,
								SVGBoundingBox &bbox)
{
	if (!cmd)
		return;

	switch(cmd->info.type)
	{
	case SVGPathSeg::SVGP_MOVETO_ABS:
	case SVGPathSeg::SVGP_LINETO_ABS:
		bbox.minx = SVGNumber::min_of(bbox.minx, cmd->x);
		bbox.miny = SVGNumber::min_of(bbox.miny, cmd->y);
		bbox.maxx = SVGNumber::max_of(bbox.maxx, cmd->x);
		bbox.maxy = SVGNumber::max_of(bbox.maxy, cmd->y);
		break;

#ifdef SVG_KEEP_QUADRATIC_CURVES
	case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
		bbox.minx = SVGNumber::min_of(bbox.minx, cmd->x);
		bbox.maxx = SVGNumber::max_of(bbox.maxx, cmd->x);
		bbox.miny = SVGNumber::min_of(bbox.miny, cmd->y);
		bbox.maxy = SVGNumber::max_of(bbox.maxy, cmd->y);

		if (ExtendsBBox3(bbox.minx, bbox.maxx, cmd->x1))
			CalcQuadExtrema(curr_pt.x, cmd->x1, cmd->x, bbox.minx, bbox.maxx);
		if (ExtendsBBox3(bbox.miny, bbox.maxy, cmd->y1))
			CalcQuadExtrema(curr_pt.y, cmd->y1, cmd->y, bbox.miny, bbox.maxy);

		break;
#endif // SVG_KEEP_QUADRATIC_CURVES

	case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
		bbox.minx = SVGNumber::min_of(bbox.minx, cmd->x);
		bbox.maxx = SVGNumber::max_of(bbox.maxx, cmd->x);
		bbox.miny = SVGNumber::min_of(bbox.miny, cmd->y);
		bbox.maxy = SVGNumber::max_of(bbox.maxy, cmd->y);

		if (ExtendsBBox4(bbox.minx, bbox.maxx, cmd->x1, cmd->x2))
			CalcCubicExtrema(curr_pt.x, cmd->x1, cmd->x2, cmd->x, bbox.minx, bbox.maxx);
		if (ExtendsBBox4(bbox.miny, bbox.maxy, cmd->y1, cmd->y2))
			CalcCubicExtrema(curr_pt.y, cmd->y1, cmd->y2, cmd->y, bbox.miny, bbox.maxy);

		break;

#ifdef _DEBUG
	case SVGPathSeg::SVGP_CLOSE:
		break;
	default:
		OP_ASSERT(!"Bad pathseg type, please normalize first!");
		break;
#endif
	}
}

OP_STATUS
OpBpath::NormalizeSegment(const SVGPathSeg* cmd,
						  const SVGPathSeg* lastcmd,
						  const SVGPathSeg* lastMovetoNorm,
						  const SVGPathSeg* lastcpy,
						  const SVGPathSeg* lastlastcpy,
						  NormalizedPathSegListInterface* outlist)
{
	OP_STATUS status = OpStatus::OK;
	SVGPathSeg cpy;
	SVGNumberPair currentPos;

	if(!outlist)
		return OpStatus::ERR_NULL_POINTER;

	if(lastcpy)
	{
		currentPos.x = lastcpy->x;
		currentPos.y = lastcpy->y;
	}

	switch(cmd->info.type)
	{
	case SVGPathSeg::SVGP_MOVETO_ABS:
	case SVGPathSeg::SVGP_MOVETO_REL:
	{
		SVGPathSeg::SVGPathSegType type = SVGPathSeg::SVGP_MOVETO_ABS;
		if(!lastcmd)
		{
			// "If a relative moveto (m) appears as the first element of the path,
			//  then it is treated as a pair of absolute coordinates."
			currentPos.x = cmd->x;
			currentPos.y = cmd->y;
		}
		else
		{
			if(cmd->info.type == SVGPathSeg::SVGP_MOVETO_ABS)
			{
				currentPos.x = cmd->x;
				currentPos.y = cmd->y;
			}
			else
			{
				currentPos.x += cmd->x;
				currentPos.y += cmd->y;
			}

			if (cmd->info.is_explicit == 0) /* Not explicit */
			{
				// "If a moveto is followed by multiple pairs of coordinates,
				//  the subsequent pairs are treated as implicit lineto commands."
				if(lastcmd && (lastcmd->info.type == SVGPathSeg::SVGP_MOVETO_ABS ||
							   lastcmd->info.type == SVGPathSeg::SVGP_MOVETO_REL))
				{
					type = SVGPathSeg::SVGP_LINETO_ABS;
				}
			}
		}

		cpy.info.is_explicit = 1; //cmd->info.is_explicit;
		cpy.info.type = type;
		cpy.x = currentPos.x;
		cpy.y = currentPos.y;
		status = outlist->AddNormalizedCopy(cpy);
	}
	break;
	case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
	case SVGPathSeg::SVGP_CURVETO_CUBIC_REL:
	case SVGPathSeg::SVGP_LINETO_ABS:
		cpy = *cmd;
		if(cmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_REL)
		{
			cpy.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
			cpy.x += currentPos.x;
			cpy.y += currentPos.y;
			cpy.x1 += currentPos.x;
			cpy.y1 += currentPos.y;
			cpy.x2 += currentPos.x;
			cpy.y2 += currentPos.y;
		}
		status = outlist->AddNormalizedCopy(cpy);
		break;
	case SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL:
	case SVGPathSeg::SVGP_LINETO_VERTICAL_REL:
	case SVGPathSeg::SVGP_LINETO_REL:
		cpy = *cmd;
		cpy.info.type = SVGPathSeg::SVGP_LINETO_ABS;
		cpy.x += currentPos.x;
		cpy.y += currentPos.y;
		status = outlist->AddNormalizedCopy(cpy);
		break;
	case SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS:
	case SVGPathSeg::SVGP_LINETO_VERTICAL_ABS:
		cpy = *cmd;
		cpy.info.type = SVGPathSeg::SVGP_LINETO_ABS;
		if(cmd->info.type == SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS)
		{
			cpy.y = currentPos.y;
		}
		else
		{
			cpy.x = currentPos.x;
		}
		status = outlist->AddNormalizedCopy(cpy);
		break;
	case SVGPathSeg::SVGP_ARC_ABS:
	case SVGPathSeg::SVGP_ARC_REL:
		{
			UINT32 numadded;
			SVGPathSeg segments[5];
			status = OpBpath::ConvertArcToBezier(cmd, currentPos, segments, numadded);

			for(UINT32 i = 0; i < numadded && OpStatus::IsSuccess(status); i++)
				status = outlist->AddNormalizedCopy(segments[i]);
		}
		break;
	case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
	case SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL:
		{
#ifdef SVG_KEEP_QUADRATIC_CURVES
			cpy = *cmd;
			if(cmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL)
			{
				cpy.info.type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS;
				cpy.x += currentPos.x;
				cpy.y += currentPos.y;
				cpy.x1 += currentPos.x;
				cpy.y1 += currentPos.y;
			}
#else
			OpBpath::ConvertQuadraticToCubic(cmd->x1, cmd->y1, cmd->x, cmd->y, (cmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL), currentPos.x, currentPos.y, cpy);
#endif
			status = outlist->AddNormalizedCopy(cpy);
		}
		break;
	case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS:
	case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL:
	{
		cpy = *cmd;
		cpy.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;

		if (lastcmd)
		{
			if (lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS ||
				lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_REL ||
				lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS ||
				lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL)
			{
				if(!lastcpy)
					return OpStatus::ERR_NULL_POINTER;

				cpy.x1 = currentPos.x + (currentPos.x - lastcpy->x2);
				cpy.y1 = currentPos.y + (currentPos.y - lastcpy->y2);
			}
			else
			{
				cpy.x1 = currentPos.x;
				cpy.y1 = currentPos.y;
			}
		}
		else
		{
			cpy.x1 = currentPos.x;
			cpy.y1 = currentPos.y;
		}

		if (cmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL)
		{
			cpy.x2 += currentPos.x;
			cpy.y2 += currentPos.y;
			cpy.x += currentPos.x;
			cpy.y += currentPos.y;
		}

		status = outlist->AddNormalizedCopy(cpy);
	}
	break;
	case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS:
	case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL:
	{
		SVGNumber cp1x;
		SVGNumber cp1y;
		SVGNumber endx = cmd->x;
		SVGNumber endy = cmd->y;

#ifdef SVG_KEEP_QUADRATIC_CURVES
		if(cmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL)
		{
			endx += currentPos.x;
			endy += currentPos.y;
		}
#endif // SVG_KEEP_QUADRATIC_CURVES

		if(lastcmd)
        {
            if (lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS ||
				lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL ||
				lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS ||
				lastcmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL)
            {
				if(!lastcpy 
#ifndef SVG_KEEP_QUADRATIC_CURVES
					|| !lastlastcpy
#endif // !SVG_KEEP_QUADRATIC_CURVES
					)
					return OpStatus::ERR_NULL_POINTER;

				// (c1,c2) should be the reflection of the last (c1,c2) controlpoint relative to the current point
                SVGNumber last_cp1x = 0;
                SVGNumber last_cp1y = 0;

#ifdef SVG_KEEP_QUADRATIC_CURVES
				last_cp1x = lastcpy->x1;
				last_cp1y = lastcpy->y1;
#else
				// We need the last c1 controlpoint, but it has been converted to cubic in the normalized case
				if(lastlastcpy)
				{
					last_cp1x = lastlastcpy->x + ((lastcpy->x1 - lastlastcpy->x)*3)/2;
					last_cp1y = lastlastcpy->y + ((lastcpy->y1 - lastlastcpy->y)*3)/2;
				}
#endif // SVG_KEEP_QUADRATIC_CURVES

                cp1x = currentPos.x + (currentPos.x - last_cp1x);
                cp1y = currentPos.y + (currentPos.y - last_cp1y);

#ifndef SVG_KEEP_QUADRATIC_CURVES
				// Now we have an absolute cp1, but ConvertQuadraticToCubic expects a relative cp if we're
				// doing a relative smooth curveto.
				if(cmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL)
                {
                    cp1x -= currentPos.x;
                    cp1y -= currentPos.y;
                }
#endif // !SVG_KEEP_QUADRATIC_CURVES
			}
            else
            {
				cp1x = currentPos.x;
                cp1y = currentPos.y;
            }
        }
        else
        {
            cp1x = currentPos.x;
            cp1y = currentPos.y;
        }

#ifdef SVG_KEEP_QUADRATIC_CURVES
		cpy.info.type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS;
		cpy.x = endx;
		cpy.y = endy;
		cpy.x1 = cp1x;
		cpy.y1 = cp1y;
#else
		OpBpath::ConvertQuadraticToCubic(cp1x, cp1y, endx, endy, (cmd->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL), currentPos.x, currentPos.y, cpy);
#endif // SVG_KEEP_QUADRATIC_CURVES
		status = outlist->AddNormalizedCopy(cpy);
	}
	break;
	case SVGPathSeg::SVGP_CLOSE:
	{
		if(lastMovetoNorm && (!currentPos.x.Close(lastMovetoNorm->x) ||
 			!currentPos.y.Close(lastMovetoNorm->y)))
		{
			currentPos.x = lastMovetoNorm->x;
			currentPos.y = lastMovetoNorm->y;
		}

		// Add the close command
		cpy.info.type = SVGPathSeg::SVGP_CLOSE;
		cpy.x = currentPos.x;
		cpy.y = currentPos.y;
		status = outlist->AddNormalizedCopy(cpy);
	}
	break;
	}

	// Note that our calls to AddNormalizedCopy might have reallocated the buffer that contained the SVGPathSegPointers so don't use them any more.

	return status;
}

OP_STATUS OpBpath::Bezierize(OpBpath** newpath) const
{
	SVGNumberPair currentPos;

	if(!newpath)
		return OpStatus::ERR_NULL_POINTER;

	OpBpath* path;
	RETURN_IF_ERROR(OpBpath::Make(path, FALSE, m_pathlist ? m_pathlist->GetCount(TRUE) : 0));
	OpAutoPtr<OpBpath> np(path);

	OpAutoPtr<PathSegListIterator> iter(GetPathIterator(TRUE)); /* normalized */
	if (!iter.get())
		return OpStatus::ERR_NO_MEMORY;

	unsigned int listcount = GetCount();
	for (unsigned int i = 0; i < listcount; i++)
	{
		const SVGPathSeg* cmd = iter->GetNext();
		switch(cmd->info.type)
		{
		case SVGPathSeg::SVGP_LINETO_ABS:
			{
				// convert line to cubic bezier
				SVGNumber one_third(0.333333);
				SVGNumber two_thirds(0.666666);
				SVGNumber dx = cmd->x - currentPos.x;
				SVGNumber dy = cmd->y - currentPos.y;

				RETURN_IF_ERROR(np->CubicCurveTo(
					currentPos.x + dx*one_third, currentPos.y + dy*one_third,
					currentPos.x + dx*two_thirds, currentPos.y + dy*two_thirds,
					cmd->x, cmd->y, FALSE, FALSE));
			}
			break;
#ifdef SVG_KEEP_QUADRATIC_CURVES
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
            {
				SVGPathSeg cubic;
				OpBpath::ConvertQuadraticToCubic(cmd->x1, cmd->y1, cmd->x, cmd->y,
					FALSE, currentPos.x, currentPos.y, cubic);

				RETURN_IF_ERROR(np->CubicCurveTo(cubic.x1, cubic.y1, cubic.x2, cubic.y2, cubic.x, cubic.y, FALSE, FALSE));
            }
            break;
#endif // SVG_KEEP_QUADRATIC_CURVES
#if 0
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			{
				SVGNumber x01, y01, x12, y12, x23, y23, x012, y012, x123, y123, x1234, y1234;

				x01 = (currentPos.x + cmd->x1) * SVGNumber(0.500000);
				y01 = (currentPos.y + cmd->y1) * SVGNumber(0.500000);
				x12 = (cmd->x1 + cmd->x2) * SVGNumber(0.500000);
				y12 = (cmd->y1 + cmd->y2) * SVGNumber(0.500000);
				x23 = (cmd->x2 + cmd->x) * SVGNumber(0.500000);
				y23 = (cmd->y2 + cmd->y) * SVGNumber(0.500000);
				x012 = (x01 + x12) * SVGNumber(0.500000);
				y012 = (y01 + y12) * SVGNumber(0.500000);
				x123 = (x12 + x23) * SVGNumber(0.500000);
				y123 = (y12 + y23) * SVGNumber(0.500000);
				x1234 = (x012 + x123) * SVGNumber(0.500000);
				y1234 = (y012 + y123) * SVGNumber(0.500000);

				RETURN_IF_ERROR(np->CubicCurveTo (	x01, y01,
													x012, y012,
													x1234, y1234,
													FALSE, FALSE));
				RETURN_IF_ERROR(np->CubicCurveTo (	x123, y123,
													x23, y23,
													cmd->x, cmd->y,
													FALSE, FALSE));
			}
			break;
#endif // 0
		default:
			{
				RETURN_IF_ERROR(np->AddCopy(*cmd));
			}
			break;
		}

		currentPos.x = cmd->x;
		currentPos.y = cmd->y;
	}

	*newpath = np.release();

	return OpStatus::OK;
}

OP_STATUS OpBpath::ConvertArcToBezier(const SVGPathSeg* arcseg, SVGNumberPair& currentPos, SVGPathSeg* outlist, UINT32& outidx)
{
	OP_STATUS status = LowConvertArcToBezier(arcseg, currentPos, outlist, outidx);
	if (OpStatus::IsSuccess(status))
	{
		if (outidx == 0)
		{
			SVGPathSeg& curve = outlist[outidx++];
			curve.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
			curve.x1 = currentPos.x;
			curve.y1 = currentPos.y;
			curve.x2 = currentPos.x;
			curve.y2 = currentPos.y;
			curve.x = currentPos.x;
			curve.y = currentPos.y;
		}
		return OpStatus::OK;
	}
	else
	{
		return status;
	}
}

#define MARK_CONVERTED_ARC(seg) do { (seg).info.large = (seg).info.sweep = 1; } while(0)

/* NOTE: It is assumed that this function does not generate more than five
 * SVGPathSegs. If that is changed, callning code must be adjusted. */
OP_STATUS OpBpath::LowConvertArcToBezier(const SVGPathSeg* arcseg, SVGNumberPair& currentPos, SVGPathSeg* outlist, UINT32& outidx)
{
	// Fix the radius
	SVGNumber x = arcseg->x;
	SVGNumber y = arcseg->y;
	SVGNumber rx = arcseg->x1;
	SVGNumber ry = arcseg->y1;
	SVGNumber xrot = arcseg->x2;
	outidx = 0;

	if(arcseg->info.type == SVGPathSeg::SVGP_ARC_REL)
	{
		x += currentPos.x;
		y += currentPos.y;
	}

	SVGNumber rxsq, rysq;
	rxsq = rx * rx;
	rysq = ry * ry;
	if( rxsq.Equal(0) || rysq.Equal(0) )
	{
		SVGPathSeg* curve = &outlist[outidx++];
		curve->info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
		if (outidx != 1)
			MARK_CONVERTED_ARC(*curve);
		curve->x1 = x;
		curve->y1 = y;
		curve->x2 = x;
		curve->y2 = y;
		curve->x = x;
		curve->y = y;
		return OpStatus::OK;
	}
	if (rx < 0)
		rx = -rx;
	if (ry < 0)
		ry = -ry;

	SVGNumber sx, sy;
	sx = currentPos.x;
	sy = currentPos.y;

	SVGNumber dx, dy;
	dx = sx-x;
	dy = sy-y;
	if (dx.Close(0) && dy.Close(0))
		return OpStatus::OK;
	dx = dx / 2;
	dy = dy / 2;

	SVGNumber xprim;
	SVGNumber yprim;
	SVGNumber cosfi, sinfi;

	// xrot is in degrees, cos&sin expect radians
	SVGNumber xrotrad = xrot * SVGNumber::pi() / 180;
	cosfi = xrotrad.cos();
	sinfi = xrotrad.sin();
	xprim = (cosfi * dx) + (sinfi * dy);
	yprim = (cosfi * dy) - (sinfi * dx);

	SVGNumber radcheck = (xprim * xprim) / rxsq;
	radcheck += (yprim * yprim) / rysq;
	if (radcheck > SVGNumber(1)){
		radcheck = radcheck.sqrt();
		rx = radcheck * rx;
		ry = radcheck * ry;
		rxsq = rx * rx;
		rysq = ry * ry;
	}

	// Now we know the radius is valid
	// Find the center of the ellipse
	SVGNumber cxprim, cyprim;
	SVGNumber clen;
	clen = rxsq * yprim * yprim;
	clen += rysq * xprim * xprim;
	// Fixed point underflow quirk
	if (clen.NotEqual(0))
		clen = ((rxsq * rysq)-clen) / clen;
	else
		clen = SVGNumber::max_num();
	if (clen < 0)
		clen = -clen;
	clen = clen.sqrt();
	if( arcseg->info.large == arcseg->info.sweep )
		clen = -clen;
	cxprim = (rx * yprim) / ry;
	cyprim = -(ry * xprim) / rx;
	cxprim = cxprim * clen;
	cyprim = cyprim * clen;

	SVGNumber cx, cy;
	cx = cosfi * cxprim;
	cx -= sinfi * cyprim;
	cy = sinfi * cxprim;
	cy += cosfi * cyprim;
	cx += (sx+x) / 2;
	cy += (sy+y) / 2;

	// Find start and stop angle
	SVGNumber startangle;
	SVGNumber angle;

	SVGNumber ux, uy, vx, vy, ulen, vlen;
	// start angle
	vx = (xprim-cxprim) / rx;
	vy = (yprim-cyprim) / ry;

	vlen = vx * vx;
	vlen += vy * vy;
	vlen = vlen.sqrt();
	startangle = vx / vlen;
	if (startangle < SVGNumber(-1))
		startangle = SVGNumber(-1);
	if (startangle > SVGNumber(1))
		startangle = SVGNumber(1);
	startangle = startangle.acos();
	if (vy < 0)
		startangle = -startangle;

	startangle *= SVGNumber(180) / SVGNumber::pi(); // is in rad, deg is wanted

	// delta angle
	ux = vx;
	uy = vy;
	ulen = vlen;

	vx = (-xprim-cxprim) / rx;
	vy = (-yprim-cyprim) / ry;

	vlen = vx * vx;
	vlen += vy * vy;
	vlen = vlen.sqrt();

	angle = ((ux * vx)+(uy * vy)) / (ulen * vlen);
	if (angle < SVGNumber(-1))
		angle = SVGNumber(-1);
	if (angle > SVGNumber(1))
		angle = SVGNumber(1);
	angle = angle.acos() * 180 / SVGNumber::pi(); // is in rad, deg is wanted

	// Calculate winding / collinearity
	SVGNumber col = ux * vy - uy * vx;
	if (col != 0 && angle.Equal(0))
		angle = SVGNumber::eps();
	if (col > 0)
		angle = -angle;

	if (angle < SVGNumber(-180))
		angle = SVGNumber(-180);

	if (angle > SVGNumber(180))
		angle = SVGNumber(180);

	if (!arcseg->info.sweep && angle < 0)
	{
		angle += SVGNumber(360);
	}

	if (arcseg->info.sweep && angle > 0)
	{
		angle -= SVGNumber(360);
	}

	// Some value used to make sure we don't get a div by zero
	while(angle > SVGNumber(1) || angle < SVGNumber(-1)){
		SVGNumber curangle, currot;
		bool swap = false;
		SVGNumber x0, y0, x1, y1, x2, y2, x3, y3;
		if (angle > SVGNumber(90))
		{
			curangle = SVGNumber(90);
			angle -= SVGNumber(90);
			currot = startangle - (curangle/2);
			startangle -= SVGNumber(90);
		}
		else if (angle < SVGNumber(-90))
		{
			curangle = SVGNumber(90);
			angle += SVGNumber(90);
			currot = startangle + (curangle/2);
			swap = true;
			startangle += SVGNumber(90);
		}
		else if (angle > 0)
		{
			curangle = angle;
			angle = 0;
			currot = startangle - (curangle/2);
		}
		else
		{
			curangle = -angle;
			angle = 0;
			currot = startangle + (curangle/2);
			swap = true;
		}
		// This is the control points of the bezier curve from curangle/2 to -curangle/2
		// curangle is in degrees, radians is wanted
		SVGNumber curanglerad = ((curangle/2)*SVGNumber::pi()/180);
		x0 = curanglerad.cos();
		y0 = curanglerad.sin();
		x1 = (SVGNumber(4)-x0)/3;
		y1 = (SVGNumber(1)-x0) * (SVGNumber(3)-x0);
		y1 = y1 / (y0*3);
		x2 = x1;
		y2 = -y1;
		x3 = x0;
		y3 = -y0;

		// rotate
		SVGMatrix trans;
		trans.LoadRotation(currot);	// expects degrees
		trans.ApplyToCoordinate(&x0, &y0);
		trans.ApplyToCoordinate(&x1, &y1);
		trans.ApplyToCoordinate(&x2, &y2);
		trans.ApplyToCoordinate(&x3, &y3);
		// stretch
		x0 = x0 * rx;
		y0 = y0 * ry;
		x1 = x1 * rx;
		y1 = y1 * ry;
		x2 = x2 * rx;
		y2 = y2 * ry;
		x3 = x3 * rx;
		y3 = y3 * ry;
		// rotate
		trans.LoadRotation(xrot); // expects degrees
		trans.ApplyToCoordinate(&x0, &y0);
		trans.ApplyToCoordinate(&x1, &y1);
		trans.ApplyToCoordinate(&x2, &y2);
		trans.ApplyToCoordinate(&x3, &y3);
		// translate
		x0 += cx;
		y0 += cy;
		x1 += cx;
		y1 += cy;
		x2 += cx;
		y2 += cy;
		x3 += cx;
		y3 += cy;
		if (swap)
		{
			SVGNumber tx, ty;
			tx = x0;
			ty = y0;
			x0 = x3;
			y0 = y3;
			x3 = tx;
			y3 = ty;
			tx = x1;
			ty = y1;
			x1 = x2;
			y1 = y2;
			x2 = tx;
			y2 = ty;
		}
		if (angle.Equal(0))
		{
			x3 = x;
			y3 = y;
		}

		SVGPathSeg newseg;

		newseg.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
		if (outidx != 0)
			MARK_CONVERTED_ARC(newseg);
		newseg.x = x3;
		newseg.y = y3;
		newseg.x1 = x1;
		newseg.y1 = y1;
		newseg.x2 = x2;
		newseg.y2 = y2;

		outlist[outidx++] = newseg;
	}

	SVGPathSeg* lastseg = NULL;
	if(outidx > 0)
	{
		lastseg = &outlist[outidx-1];
	}

	if (!lastseg || (!lastseg->x.Close(x) || !lastseg->y.Close(y)))
	{
		SVGPathSeg& curve = outlist[outidx++];
		curve.info.type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS;
		if (outidx != 1)
			MARK_CONVERTED_ARC(curve);
		curve.x1 = x;
		curve.y1 = y;
		curve.x2 = x;
		curve.y2 = y;
		curve.x = x;
		curve.y = y;
	}
	return OpStatus::OK;
}

#undef MARK_CONVERTED_ARC

#ifdef SVG_FULL_11
SynchronizedPathSegList::SynchronizedPathSegList() : m_normalized_count(0), m_count(0)
{
}

OP_STATUS
SynchronizedPathSegList::Sync(UINT32 comp_idx, SVGPathSegObject* newval)
{
	SVGCompoundSegment* seg = m_segments.Get(comp_idx);
	if (!seg)
		return OpStatus::ERR;

#ifdef _DEBUG
	OP_ASSERT(seg->GetSeg() == newval); // This means that the segment isn't located at the index it belives.
#endif // _DEBUG

	UINT32 old_count = seg->GetCount();
	UINT32 old_normalized_count = seg->GetNormalizedCount();

	RETURN_IF_ERROR(SetupNewSegment(seg, newval, comp_idx, 0, FALSE));

	m_count += seg->GetCount() - old_count;
	m_normalized_count += seg->GetNormalizedCount() - old_normalized_count;

	UpdateMembership(comp_idx);
	RebuildBoundingBox();
	return OpStatus::OK;
}

void
SVGCompoundSegment::InvalidateSeg()
{
	packed.seg_invalid = 1;
	SVGPathSegObject::Release(m_seg);
	SVGObject::DecRef(m_seg);
	m_seg = NULL;
}
#endif // SVG_FULL_11

OP_STATUS NormalizedPathSegList::AddCopy(const SVGPathSeg& src)
{
	OP_STATUS status = OpStatus::OK;
	if ((src.info.type == SVGPathSeg::SVGP_MOVETO_ABS &&
		 src.info.is_explicit == 1) ||
		src.info.type == SVGPathSeg::SVGP_LINETO_ABS ||
#ifdef SVG_KEEP_QUADRATIC_CURVES
		src.info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS ||
#endif // SVG_KEEP_QUADRATIC_CURVES
		src.info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS)
	{
		status = AddNormalizedCopy(src);
	}
	else
	{
		SVGPathSeg* prevSeg = NULL;
		SVGPathSeg* lastMoveto = NULL;
		SVGPathSeg* prevPrevSeg = NULL;

		// Find last moveto
#ifdef FLAT_SEGMENT_STORAGE

		if(m_segment_array_used > 0)
		{
			prevSeg = m_segment_array+(m_segment_array_used - 1);

#ifdef KEEP_LAST_MOVETO_INDEX
			if(m_last_explicit_moveto_index != -1)
			{
				lastMoveto = m_segment_array+m_last_explicit_moveto_index;
			}
#else
			SVGPathSeg* item = m_segment_array+m_segment_array_used;
			do {
				item--;
				if(item && item->info.type == SVGPathSeg::SVGP_MOVETO_ABS &&
				   item->info.is_explicit == 1)
				{
					lastMoveto = item;
					break;
				}
			} while (item != m_segment_array);
#endif // KEEP_LAST_MOVETO_INDEX
		}

		if(m_segment_array_used > 1)
		{
			prevPrevSeg = m_segment_array + m_segment_array_used - 2;
		}

		// NOTE: This might reallocate the pathseg array so no pointers will be valid after that
		status = OpBpath::NormalizeSegment(&src, m_segment_array_used ? &m_last_added_seg : NULL, lastMoveto, prevSeg, prevPrevSeg, this);
#else
		UINT32 count = m_segments.GetCount();
		if(count > 0)
		{
			prevSeg = m_segments.Get(count - 1);

#ifdef KEEP_LAST_MOVETO_INDEX
			if(m_last_explicit_moveto_index != -1)
			{
				lastMoveto = m_segments.Get(m_last_explicit_moveto_index);
			}
#else
			SVGPathSeg* item;
			UINT32 i = count-1;
			do {
				item = m_segments.Get(i);
				if(item && item->info.type == SVGPathSeg::SVGP_MOVETO_ABS &&
				   item->info.is_explicit == 1)
				{
					lastMoveto = item;
					break;
				}
			} while (i-- != 0);
#endif // KEEP_LAST_MOVETO_INDEX
		}

		if(count > 1)
		{
			prevPrevSeg = m_segments.Get(count - 2);
		}

		status = OpBpath::NormalizeSegment(&src, m_segments.GetCount() == 0 ? NULL : &m_last_added_seg, lastMoveto, prevSeg, prevPrevSeg, this);
#endif // FLAT_SEGMENT_STORAGE
	}

	m_last_added_seg = src;
	return status;
}

/* private */ OP_STATUS
NormalizedPathSegList::SetArraySize(UINT32 new_size)
{
	OP_ASSERT(new_size >= m_segment_array_used || !"We are cutting away part of the path and possibly crashing");
	if (!new_size)
	{
		OP_DELETEA(m_segment_array);
		m_segment_array = NULL;
		m_segment_array_size = 0;
		return OpStatus::OK;
	}

	if (m_segment_array_size != new_size)
	{
		SVGPathSeg* new_array = OP_NEWA(SVGPathSeg, new_size);
		if (!new_array)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (m_segment_array_used > 0)
		{
			OP_ASSERT(m_segment_array);
			op_memcpy(new_array, m_segment_array, m_segment_array_used*sizeof(*m_segment_array));
		}
		OP_DELETEA(m_segment_array);
		m_segment_array = new_array;
		m_segment_array_size = new_size;
	}
	return OpStatus::OK;
}
/* virtual */ OP_STATUS
NormalizedPathSegList::AddNormalizedCopy(const SVGPathSeg& item)
{
#ifdef FLAT_SEGMENT_STORAGE
	if (m_segment_array_size == m_segment_array_used)
	{
		UINT32 new_size = MAX(m_segment_array_size*3/2, m_segment_array_size + 10);
		RETURN_IF_ERROR(SetArraySize(new_size));
	}

	OP_ASSERT(m_segment_array); // Should be impossible to get here without an array
	OP_ASSERT(m_segment_array_size > m_segment_array_used);

#ifdef KEEP_LAST_MOVETO_INDEX
	if(item.info.type == SVGPathSeg::SVGP_MOVETO_ABS && item.info.is_explicit == 1)
		m_last_explicit_moveto_index = m_segment_array_used;
#endif // KEEP_LAST_MOVETO_INDEX

	SVGNumberPair curr_pt;
	if (m_segment_array_used > 0)
	{
		curr_pt.x = m_segment_array[m_segment_array_used - 1].x;
		curr_pt.y = m_segment_array[m_segment_array_used - 1].y;
	}

	m_segment_array[m_segment_array_used++] = item;
 	OpBpath::UpdateBoundingBox(curr_pt, &item, m_bbox);

	return OpStatus::OK;
#else
	SVGPathSeg* newval = OP_NEW(SVGPathSeg, (item));
	if (!newval)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err = m_segments.Add(newval);
	if(OpStatus::IsSuccess(err))
	{
#ifdef KEEP_LAST_MOVETO_INDEX
		if(item.info.type == SVGPathSeg::SVGP_MOVETO_ABS && item.info.is_explicit == 1)
			m_last_explicit_moveto_index = m_segments.GetCount() - 1;
#endif // KEEP_LAST_MOVETO_INDEX

		SVGNumberPair curr_pt;
		if (m_segments.GetCount() > 1)
		{
			SVGPathSeg* seg = m_segments.Get(m_segments.GetCount() - 2);
			curr_pt.x = seg->x;
			curr_pt.y = seg->y;
		}
		OpBpath::UpdateBoundingBox(curr_pt, newval, m_bbox);
	}

	return  err;
#endif
}

/* virtual */ UINT32
NormalizedPathSegList::GetCount(BOOL normalized /* = TRUE */) const
{
#ifdef FLAT_SEGMENT_STORAGE
	return m_segment_array ? m_segment_array_used : 0;
#else
	return m_segments.GetCount();
#endif
}

#ifdef SVG_FULL_11
/* virtual */ OP_STATUS
NormalizedPathSegList::Add(SVGPathSegObject* newval)
{
	OP_ASSERT(!"You must use SynchronizedPathSegList for manipulations of the list");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS NormalizedPathSegList::Insert(UINT32 idx, SVGPathSegObject* newval, BOOL normalized)
{
	OP_ASSERT(!"You must use SynchronizedPathSegList for manipulations of the list");
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS NormalizedPathSegList::Replace(UINT32 idx, SVGPathSegObject* newval, BOOL normalized)
{
	// Used by Bezierize/Warp and OpBpath::Set (used in Interpolation)
#ifdef FLAT_SEGMENT_STORAGE
	OP_ASSERT(m_segment_array && idx < m_segment_array_used);
	m_segment_array[idx] = *SVGPathSegObject::p(newval);
	OP_DELETE(newval);
	RebuildBoundingBox();
	return OpStatus::OK;
#else
	SVGPathSeg* copy;
	OP_STATUS status = SVGPathSegObject::p(newval)->Clone(&copy);
	OP_DELETE(newval); newval = NULL;
	RETURN_IF_ERROR(status);

	OP_DELETE(m_segments.Get(idx));
	OP_STATUS err = m_segments.Replace(idx, copy);
	if(OpStatus::IsSuccess(err))
	{
		RebuildBoundingBox();
	}
	else
	{
		OP_DELETE(copy);
	}
	return err;
#endif
}

/* virtual */ OP_STATUS
NormalizedPathSegList::Delete(UINT32 idx, BOOL normalized)
{
	OP_ASSERT(!"You must use SynchronizedPathSegList for manipulations of the list");
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ SVGPathSegObject*
NormalizedPathSegList::Get(UINT32 idx, BOOL normalized /* = TRUE */) const
{
	OP_ASSERT(!"You must use SynchronizedPathSegList for manipulations of the list");
	return NULL;
}
#endif // SVG_FULL_11

/* virtual */ void
NormalizedPathSegList::Clear()
{
	// Used by Bezierize/Warp when not warping
#ifdef FLAT_SEGMENT_STORAGE
	OP_DELETEA(m_segment_array);
	m_segment_array = NULL;
	m_segment_array_used = 0;
	m_segment_array_size = 0;
#else
	m_segments.DeleteAll();
#endif // FLAT_SEGMENT_STORAGE
	m_bbox.Clear();
}

void PathSegList::RebuildBoundingBox()
{
	m_bbox.Clear();
	PathSegListIterator* iter = GetIterator(TRUE);
	if(!iter)
		return; // FIXME: Handle OOM better

	SVGPathSeg* seg = iter->GetNext();
	SVGNumberPair curr_pt;
	while(seg)
	{
		OpBpath::UpdateBoundingBox(curr_pt, seg, m_bbox);

		curr_pt.x = seg->x;
		curr_pt.y = seg->y;

		seg = iter->GetNext();
	}
	OP_DELETE(iter);
}

OP_STATUS NormalizedPathSegList::Copy(PathSegList* plist)
{
	UINT32 count = GetCount();

	if(plist->GetAsNormalizedPathSegList())
	{
		NormalizedPathSegList* olist = plist->GetAsNormalizedPathSegList();
		if(olist->GetCount() == count)
		{
			for(UINT32 i = 0; i < count; i++)
			{
#ifdef FLAT_SEGMENT_STORAGE
				m_segment_array[i] = olist->m_segment_array[i];
#else
				m_segments.Get(i)->Copy(olist->m_segments.Get(i));
#endif
			}
		}
		else
		{
			Clear();

			OpAutoPtr<PathSegListIterator> iter(olist->GetIterator(TRUE));
			if(!iter.get())
				return OpStatus::ERR_NO_MEMORY;

			SVGPathSeg* seg = iter->GetNext();
			while(seg)
			{
				RETURN_IF_ERROR(AddNormalizedCopy(*seg));
				seg = iter->GetNext();
			}
		}
	}
#ifdef SVG_FULL_11
	else if(plist->GetAsSynchronizedPathSegList())
	{
		SynchronizedPathSegList* olist = plist->GetAsSynchronizedPathSegList();
		UINT32 olistcount = olist->GetCount(TRUE);
		if(olistcount == count)
		{
			for(UINT32 i = 0; i < count; i++)
			{
#ifdef FLAT_SEGMENT_STORAGE
				m_segment_array[i] = *SVGPathSegObject::p(olist->Get(i));
#else
				*m_segments.Get(i) = SVGPathSegObject::p(olist->Get(i));
#endif
			}
		}
		else
		{
			Clear();

			OpAutoPtr<PathSegListIterator> iter(olist->GetIterator(TRUE));
			if(!iter.get())
				return OpStatus::ERR_NO_MEMORY;

			SVGPathSeg* seg = iter->GetNext();
			while(seg)
			{
				RETURN_IF_ERROR(AddCopy(*seg));
				seg = iter->GetNext();
			}
		}
	}
#endif // SVG_FULL_11

	return OpStatus::OK;
}

#ifdef SVG_FULL_11
OP_STATUS SynchronizedPathSegList::Copy(PathSegList* plist)
{
	if(plist->GetAsNormalizedPathSegList())
	{
		// This code may be unused now since it isn't used from SetUsedByDOM().
		Clear(); // Tear down old list

		OpAutoPtr<PathSegListIterator> iter(plist->GetIterator(TRUE));
		if(!iter.get())
			return OpStatus::ERR_NO_MEMORY;
		
		SVGPathSeg* seg = iter->GetNext();
		while(seg)
		{
			RETURN_IF_ERROR(AddCopy(*seg));
			seg = iter->GetNext();
		}
		CHECK_SYNC();
	}
	else if(plist->GetAsSynchronizedPathSegList())
	{
		if (plist->GetCount(TRUE) != GetCount(TRUE) ||
			plist->GetCount(FALSE) != GetCount(FALSE))
			return OpStatus::ERR;

		SynchronizedPathSegList* pslist = plist->GetAsSynchronizedPathSegList();
		OpAutoVector<SVGCompoundSegment>& compound_segments = pslist->m_segments;
		UINT32 count = compound_segments.GetCount();
		for (UINT32 i = 0; i < count; i++)
		{
			SVGCompoundSegment* p1cseg = compound_segments.Get(i);
			SVGCompoundSegment* p2cseg = m_segments.Get(i);
			if(OpStatus::IsError(p2cseg->Copy(p1cseg)))
				return OpStatus::ERR;
		}
		UpdateMembership();

		m_count = pslist->m_count;
		m_normalized_count = pslist->m_normalized_count;
	}
	return OpStatus::OK;
}

/* virtual */ void
SynchronizedPathSegList::Clear()
{
	for (UINT32 i=0;i<m_segments.GetCount();i++)
	{
		SVGCompoundSegment* compound_seg = m_segments.Get(i);
		compound_seg->member.list = NULL;
		compound_seg->member.idx = (UINT32)-1;
		compound_seg->UpdateMembership();
	}
	m_segments.DeleteAll();
	m_count = 0;
	m_normalized_count = 0;
	m_bbox.Clear();
}

SVGPathSegObject* SynchronizedPathSegList::Get(UINT32 idx, BOOL normalized) const
{
	INT32 segidx = 0;
	INT32 compidx = CompoundIndex(idx, normalized, segidx);

	if (normalized)
		return GetNormSeg(compidx, segidx);
	else
		return GetSeg(compidx, segidx);
}

static BOOL MatchSegment(const SVGPathSeg* seg)
{
	if (!seg)
		return FALSE;
	return
		(seg->info.type == SVGPathSeg::SVGP_MOVETO_ABS) &&
		// Only explicit moveto:s are "real" moveto:s
		(seg->info.is_explicit == 1);
}

OP_STATUS SynchronizedPathSegList::AddCopy(const SVGPathSeg& src)
{
	SVGPathSegObject* newval = OP_NEW(SVGPathSegObject, (src));
	if (!newval)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = Add(newval);
	if (OpStatus::IsSuccess(status))
	{
		return OpStatus::OK;
	}
	else
	{
		OP_DELETE(newval);
		return status;
	}
}

OP_STATUS SynchronizedPathSegList::Add(SVGPathSegObject* newval)
{
	OpAutoPtr<SVGCompoundSegment> newseg(OP_NEW(SVGCompoundSegment, ()));
	if(!newseg.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(SetupNewSegment(newseg.get(), newval, m_segments.GetCount(), 0, FALSE));

	OP_STATUS err = m_segments.Add(newseg.get());

	if(OpStatus::IsSuccess(err))
	{
		m_count++;
		newseg->member.list = this;
		newseg->member.idx = m_segments.GetCount() - 1;
		newseg->member.normidx = m_normalized_count;
		newseg->UpdateMembership();

		m_normalized_count += newseg->GetNormalizedCount();
		newseg.release();
	}

	CHECK_SYNC();
	return err;
}

INT32 SynchronizedPathSegList::CompoundIndex(UINT32 idx, BOOL normalized, INT32& segindex) const
{
	CHECK_SYNC();

	UINT32 globidx = 0;
	for(UINT32 compidx = 0; compidx < m_segments.GetCount(); compidx++)
	{
		SVGCompoundSegment* seg = m_segments.Get(compidx);
		if (!seg)
			break;

		if (globidx + (normalized ? seg->GetNormalizedCount() : seg->GetCount()) > idx)
		{
			// found correct segment
			segindex = idx - globidx; // this is the index in the SVGCompoundSegment
			return compidx;
		}

		globidx += (normalized ? seg->GetNormalizedCount() : seg->GetCount());
	}

	OP_ASSERT(0);
	return -1;
}

const SVGPathSegObject* SynchronizedPathSegList::GetPrevSeg(INT32 compidx, INT32 segidx)
{
	SVGCompoundSegment* seg = m_segments.Get(compidx);
	if (segidx > 0)
		return seg->Get(segidx-1);

	if (compidx == 0)
		return NULL;

	seg = m_segments.Get(compidx-1);
	return seg->Get(seg->GetCount()-1);
}

void SynchronizedPathSegList::PrevNormIdx(INT32& compidx, INT32& segidx)
{
	OP_ASSERT(segidx >= 0);
	if (segidx == 0)
	{
		while (compidx >= 0)
		{
			compidx--;
			if (compidx < 0)
				return;
			SVGCompoundSegment* seg = m_segments.Get(compidx);
			segidx = seg->GetNormalizedCount() - 1;
			if (segidx >= 0)
				break;
		}
	}
	else
		segidx--;
}

const SVGPathSegObject* SynchronizedPathSegList::FindLastMoveTo(INT32 idx, INT32 segidx)
{
	const SVGPathSegObject* lastMoveToNormSeg = NULL;
	while (idx >= 0)
	{
		PrevNormIdx(idx, segidx);
		const SVGPathSegObject* candidate = GetNormSeg(idx, segidx);
		if (MatchSegment(SVGPathSegObject::p(candidate)))
		{
			lastMoveToNormSeg = candidate;
			break;
		}
	}
	return lastMoveToNormSeg;
}

SVGPathSegObject* SynchronizedPathSegList::GetNormSeg(INT32 compidx, INT32 segidx) const
{
	SVGCompoundSegment* seg = m_segments.Get(compidx);
	return seg ? seg->GetNormalized(segidx) : NULL;
}

SVGPathSegObject* SynchronizedPathSegList::GetSeg(INT32 compidx, INT32 segidx) const
{
	SVGCompoundSegment* seg = m_segments.Get(compidx);
	return seg ? seg->Get(segidx) : NULL;
}
#endif // SVG_FULL_11

SVGPathSeg* NormalizedPathSegList::Iterator::GetNext()
{
#ifdef FLAT_SEGMENT_STORAGE
	if ( m_path->m_segment_array && m_index < (int)m_path->m_segment_array_used)
	{
		return m_path->m_segment_array+(m_index++);
	}
	return NULL;
#else
	return m_path->m_segments.Get(m_index++);
#endif
}

#ifdef SVG_FULL_11
void
SynchronizedPathSegList::UpdateMembership(UINT32 start_segment_index /* = 0 */)
{
	UINT32 normidx = 0;
	if (start_segment_index > 0 && start_segment_index < m_segments.GetCount())
	{
		SVGCompoundSegment* compound_seg = m_segments.Get(start_segment_index-1);
		normidx = compound_seg->member.normidx + compound_seg->GetNormalizedCount();
	}

	for (UINT32 i=start_segment_index;
		 i<m_segments.GetCount();
		 i++)
	{
		SVGCompoundSegment* compound_seg = m_segments.Get(i);
		compound_seg->member.list = this;
		compound_seg->member.idx = i;
		compound_seg->member.normidx = normidx;
		compound_seg->UpdateMembership();

		normidx += compound_seg->GetNormalizedCount();
	}

	CHECK_SYNC();
}

SVGPathSegObject* SynchronizedPathSegList::Iterator::GetNextObject()
{
	SVGCompoundSegment* seg = path->m_segments.Get(compidx);
	if (!seg)
		return NULL;

	SVGPathSegObject* pseg = normalized ? seg->GetNormalized(segidx) : seg->Get(segidx);

	segidx++;
	UINT32 segcount = normalized ? seg->GetNormalizedCount() : seg->GetCount();
	if (segidx >= (INT32)segcount)
	{
		segidx = 0;
		compidx++;
		INT32 psegcount = (INT32)path->m_segments.GetCount();
		while (compidx < psegcount)
		{
			// Special case: No normalized segments...
			SVGCompoundSegment* seg = path->m_segments.Get(compidx);
			if (!seg)
				break;

			segcount = normalized ? seg->GetNormalizedCount() : seg->GetCount();
			if (segidx < (INT32)segcount)
				break;
			compidx++;
		}
	}

	return pseg;
}

SVGPathSeg* SynchronizedPathSegList::Iterator::GetNext()
{
	return SVGPathSegObject::p(GetNextObject());
}

OP_STATUS SynchronizedPathSegList::SetupNewSegment(SVGCompoundSegment* newseg, SVGPathSegObject* newval, INT32 compidx, INT32 segidx, BOOL normalized)
{
	// Get*() returns NULL if out-of-bounds
	const SVGPathSegObject* prevSeg = GetPrevSeg(compidx, segidx);
	INT32 norm_compidx = compidx;
	INT32 norm_segidx = segidx;
	PrevNormIdx(norm_compidx, norm_segidx);
	const SVGPathSegObject* prevNormSeg = GetNormSeg(norm_compidx, norm_segidx);
	PrevNormIdx(norm_compidx, norm_segidx);
	const SVGPathSegObject* prevPrevNormSeg = GetNormSeg(norm_compidx, norm_segidx);
	const SVGPathSegObject* lastMoveToNormSeg = NULL;

	// Only search for last move if it is a close command
	if (newval->seg.info.type == SVGPathSeg::SVGP_CLOSE)
		lastMoveToNormSeg = FindLastMoveTo(compidx, segidx);

	OP_STATUS err = newseg->Reset(newval, segidx, normalized, prevSeg, lastMoveToNormSeg, prevNormSeg, prevPrevNormSeg);
	if(OpStatus::IsSuccess(err) && newseg->GetNormalizedCount() > 0)
	{
		SVGNumberPair curr_pt;
		if (prevNormSeg)
		{
			curr_pt.x = prevNormSeg->seg.x;
			curr_pt.y = prevNormSeg->seg.y;
		}

		UINT32 norm_count = newseg->GetNormalizedCount();
		for (UINT32 i = 0; i < norm_count; ++i)
		{
			const SVGPathSeg* seg = SVGPathSegObject::p(newseg->GetNormalized(i));

			OpBpath::UpdateBoundingBox(curr_pt, seg, m_bbox);

			curr_pt.x = seg->x;
			curr_pt.y = seg->y;
		}
	}

	return err;
}

OP_STATUS SynchronizedPathSegList::Insert(UINT32 idx, SVGPathSegObject* newval, BOOL normalized)
{
	CHECK_SYNC();
	UINT32 count = GetCount(normalized);
	if (idx >= count) // Append case
		return Add(newval);

	INT32 segidx = 0;
	INT32 compidx = CompoundIndex(idx, normalized, segidx);
	INT32 first_compidx = compidx;

	OP_STATUS status = OpStatus::ERR;
	if (!normalized)
	{
		SVGCompoundSegment* seg = m_segments.Get(compidx);
		if (seg->GetCount() > 1 && segidx > 0)
		{
			// Insert "into" un-normalized compound
			// Split the compound into several compounds (one for each subnode),
			// insert the new compound nodes, and then insert the new compound node
			OpAutoVector<SVGCompoundSegment> split;
			RETURN_IF_ERROR(seg->Split(split));
			for (INT32 i = split.GetCount()-1; i >= 0; i--)
			{
				SVGCompoundSegment* cseg = split.Remove(i);
				m_segments.Insert(compidx+1, cseg);
			}

			m_segments.Delete(compidx);
			// The new compound index
			compidx += segidx;
			segidx = 0;
		}

		OpAutoPtr<SVGCompoundSegment> newseg(OP_NEW(SVGCompoundSegment, ()));
		if (!newseg.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(SetupNewSegment(newseg.get(), newval, compidx, segidx, normalized));

		status = m_segments.Insert(compidx + segidx, newseg.get());
		if (OpStatus::IsSuccess(status))
		{
			m_normalized_count += newseg->GetNormalizedCount();
			m_count++;
			newseg.release();
			UpdateMembership(first_compidx);
		}
	}
	else /* Insert inside normalized segment */
	{
		SVGCompoundSegment* seg = m_segments.Get(compidx);
		OP_ASSERT(seg);

		UINT32 old_count = seg->GetCount();

		status = seg->InsertNormalized(segidx, newval);
		if (OpStatus::IsSuccess(status))
		{
			m_normalized_count++;
			m_count += (seg->GetCount() - old_count);
			UpdateMembership(first_compidx);
		}
	}

	CHECK_SYNC();
	return status;
}

OP_STATUS SynchronizedPathSegList::Replace(UINT32 idx, SVGPathSegObject* newval, BOOL normalized)
{
	CHECK_SYNC();
	INT32 segidx = 0;
	INT32 compidx = CompoundIndex(idx, normalized, segidx);
	SVGCompoundSegment* seg = m_segments.Get(compidx);
	if (!seg)
		return OpStatus::ERR;

	UINT32 old_count = seg->GetCount();
	UINT32 old_normalized_count = seg->GetNormalizedCount();

	RETURN_IF_ERROR(SetupNewSegment(seg, newval, compidx, segidx, normalized));

	m_count += seg->GetCount() - old_count;
	m_normalized_count += seg->GetNormalizedCount() - old_normalized_count;

	UpdateMembership(compidx);
	RebuildBoundingBox();
	CHECK_SYNC();
	return OpStatus::OK;
}

OP_STATUS SynchronizedPathSegList::Delete(UINT32 idx, BOOL normalized)
{
	CHECK_SYNC();
	INT32 segidx = 0;
	INT32 compidx = CompoundIndex(idx, normalized, segidx);
	SVGCompoundSegment* seg = m_segments.Get(compidx);
	if (!seg)
		return OpStatus::ERR;

	OP_STATUS status = OpStatus::ERR;
	if ((normalized && seg->GetNormalizedCount() > 1) ||
		(!normalized && seg->GetCount() > 1))
	{
		BOOL unnorm_can_grow = seg->GetCount() != seg->GetNormalizedCount();
		status = seg->DeleteNormalized(segidx);
		if (OpStatus::IsSuccess(status))
		{
			// (Assumes m_count is the un-normalized count)
			// m_count can change in the following ways:
			// - It shrinks with one if the segment already is "invalid"
			// - It grows with (normalized_count - 1) if the segment becomes invalid

			if (unnorm_can_grow)
				m_count += seg->GetNormalizedCount() - 1;
			else
				m_count--;

			m_normalized_count--;
		}
	}
	else
	{
		OP_ASSERT(seg->GetCount() == 1);
		// Remove entire segment
		UINT32 new_count = m_count - seg->GetCount();
		UINT32 new_normalized_count = m_normalized_count - seg->GetNormalizedCount();

		seg->member.list = NULL;
		seg->member.idx = (UINT32)-1;
		seg->UpdateMembership();
		m_segments.Delete(seg); // Delete by item
		m_count = new_count;
		m_normalized_count = new_normalized_count;
		status = OpStatus::OK;
	}

	if(OpStatus::IsSuccess(status))
	{
		RebuildBoundingBox();
		UpdateMembership(compidx);
	}

	CHECK_SYNC();
	return status;
}

UINT32 SynchronizedPathSegList::GetCount(BOOL normalized) const
{
	if(normalized)
		return m_normalized_count;
	else
		return m_count;
}

SVGCompoundSegment::~SVGCompoundSegment()
{
	for (UINT32 i=0;i<m_normalized_seg.GetCount();++i)
		m_normalized_seg.Get(i)->member.compound = NULL;
	EmptyPathSegObjectList(m_normalized_seg);

	if (m_seg)
		m_seg->member.compound = NULL;
	SVGObject::DecRef(m_seg);
}

OP_STATUS
SVGCompoundSegment::Sync(SVGPathSegObject* obj)
{
	return member.list ? member.list->Sync(member.idx, obj) : OpStatus::OK;
}

OP_STATUS
SVGCompoundSegment::InsertNormalized(UINT32 idx, SVGPathSegObject* seg)
{
	OP_STATUS status = m_normalized_seg.Insert(idx, seg);
	if (OpStatus::IsSuccess(status))
	{
		SVGObject::IncRef(seg);
		InvalidateSeg();
	}
	return status;
}

OP_STATUS
SVGCompoundSegment::DeleteNormalized(UINT32 idx)
{
	SVGPathSegObject::Release(m_normalized_seg.Get(idx));
	SVGObject::DecRef(m_normalized_seg.Get(idx));
	m_normalized_seg.Remove(idx);

	InvalidateSeg();
	return OpStatus::OK;
}

void
SVGCompoundSegment::UpdateMembership()
{
	for (UINT32 i=0;i<m_normalized_seg.GetCount();++i)
	{
		SVGPathSegObject* obj = m_normalized_seg.Get(i);
		obj->member.compound = this;
		obj->member.idx = member.normidx + i;
	}

	if (m_seg)
	{
		m_seg->member.compound = packed.seg_invalid ? NULL : this;
		m_seg->member.idx = (UINT32)-1;
	}
}

/* static */ void
SVGCompoundSegment::EmptyPathSegObjectList(SVGPathSegObjectList& lst)
{
	for (UINT32 i=0;i<lst.GetCount();i++)
	{
		SVGPathSegObject::Release(lst.Get(i));
		SVGObject::DecRef(lst.Get(i));
	}
	lst.Clear();
}

OP_STATUS
SVGCompoundSegment::Copy(SVGCompoundSegment* from)
{
	if(from && m_normalized_seg.GetCount() == from->m_normalized_seg.GetCount())
	{
		packed = from->packed;
		if(from->m_seg)
		{
			m_seg->Copy(*from->m_seg);
		}
		for(unsigned int i = 0; i < m_normalized_seg.GetCount(); i++)
		{
			SVGPathSegObject* dst = m_normalized_seg.Get(i);
			SVGPathSegObject* src = from->m_normalized_seg.Get(i);
			if (src)
				dst->Copy(*src);
		}

		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS
SVGCompoundSegment::Split(OpVector<SVGCompoundSegment>& outlist)
{
	OpAutoPtr<SVGCompoundSegment> newseg(NULL);

	// For each normalized segment, create a new compound
	for (UINT32 i = 0; i < m_normalized_seg.GetCount(); ++i)
	{
		newseg.reset(OP_NEW(SVGCompoundSegment, ()));
		if (!newseg.get())
			return OpStatus::ERR_NO_MEMORY;

		SVGPathSegObject* norm_seg = m_normalized_seg.Get(i);
		SVGPathSegObject* copy = NULL;

		copy = static_cast<SVGPathSegObject*>(norm_seg->Clone());
		if (!copy)
			return OpStatus::ERR_NO_MEMORY;

		newseg->SetSegment(copy);
		EmptyPathSegObjectList(newseg->m_normalized_seg); // Probably redundant

		// Should be normalized already
		RETURN_IF_ERROR(OpBpath::NormalizeSegment(SVGPathSegObject::p(copy), NULL, NULL, NULL, NULL, newseg.get()/*->m_normalized_seg*/));
		newseg->UpdateMembership();
		RETURN_IF_ERROR(outlist.Add(newseg.get()));
		newseg.release();
	}
	return OpStatus::OK;
}

OP_STATUS
SVGCompoundSegment::Reset(SVGPathSegObject* newseg_obj, UINT32 idx, BOOL normalized,
						  const SVGPathSegObject* prevSeg,
						  const SVGPathSegObject* lastMovetoNormSeg,
						  const SVGPathSegObject* prevNormSeg,
						  const SVGPathSegObject* prevPrevNormSeg)
{
	SVGPathSeg* newseg = SVGPathSegObject::p(newseg_obj);
	if (normalized || packed.seg_invalid)
	{
		if (newseg->info.type == SVGPathSeg::SVGP_MOVETO_ABS ||
			newseg->info.type == SVGPathSeg::SVGP_LINETO_ABS ||
			newseg->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS ||
			newseg->info.type == SVGPathSeg::SVGP_CLOSE)
		{
			SVGPathSegObject* oldseg = m_normalized_seg.Get(idx);
			RETURN_IF_ERROR(m_normalized_seg.Replace(idx, newseg_obj));
			SVGObject::IncRef(newseg_obj);
			SVGPathSegObject::Release(oldseg);
			SVGObject::DecRef(oldseg);
			packed.seg_invalid = TRUE;
		}
		else
			return OpSVGStatus::TYPE_ERROR;
	}
	else
	{
		if (m_seg != newseg_obj)
		{
			SVGPathSegObject::Release(m_seg);
			SVGObject::DecRef(m_seg);
			m_seg = newseg_obj;
			SVGObject::IncRef(newseg_obj);
		}

		EmptyPathSegObjectList(m_normalized_seg);
		return OpBpath::NormalizeSegment(newseg, SVGPathSegObject::p(prevSeg),
								SVGPathSegObject::p(lastMovetoNormSeg),
								SVGPathSegObject::p(prevNormSeg),
								SVGPathSegObject::p(prevPrevNormSeg), this /*m_normalized_seg*/);
	}
	return OpStatus::OK;
}
#endif // SVG_FULL_11

OP_STATUS OpBpath::SetUsedByDOM(BOOL usedbydom)
{
	if(!m_pathlist)
	{
#ifdef SVG_FULL_11
		if(usedbydom)
		{
			m_pathlist = OP_NEW(SynchronizedPathSegList, ());
		}
		else
#endif // SVG_FULL_11
		{
			m_pathlist = OP_NEW(NormalizedPathSegList, ());
		}

		if(!m_pathlist)
			return OpStatus::ERR_NO_MEMORY;

		return OpStatus::OK;
	}
#ifdef SVG_FULL_11
	else if(usedbydom && !m_pathlist->GetAsSynchronizedPathSegList())
	{
		SynchronizedPathSegList *new_list = OP_NEW(SynchronizedPathSegList, ());
		if (!new_list)
			return OpStatus::ERR_NO_MEMORY;

		// switch pathlist
		OP_DELETE(m_pathlist);
		m_pathlist = new_list;

#if defined COMPRESS_PATH_STRINGS
		if (m_compressed_str_rep)
		{
			// Here we pay the cost for compression, we have to unpack the string before
			// parsing it again into a more advance form of path segment lists.
			OpString str_rep_utf16;
			unsigned uncompressed_len = PathDecompress(m_compressed_str_rep,
													   m_compressed_str_rep_length,
													   NULL);
			uni_char* buf = str_rep_utf16.Reserve(uncompressed_len);
			if (!buf)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			PathDecompress(m_compressed_str_rep,
						   m_compressed_str_rep_length,
						   buf);

			OP_STATUS status = SVGAttributeParser::ParseToExistingPath(str_rep_utf16.CStr(), str_rep_utf16.Length(), this);
			if (OpStatus::IsSuccess(status))
			{
				// We are done with the string now, the syncronized
				// path seg list is used for generating the string if
				// it is needed.
				OP_DELETEA(m_compressed_str_rep);
				m_compressed_str_rep = NULL;
			}
			else
			{
				return status;
			}
		}
#endif // COMPRESS_PATH_STRINGS
	}
#endif // SVG_FULL_11

	return OpStatus::OK;
}

#ifdef SVG_PATH_EXPENSIVE_SYNC_TESTS
void
SynchronizedPathSegList::CheckSync() const
{
	int j = 0;
	for(UINT32 i=0;i<m_segments.GetCount();i++)
	{
		SVGCompoundSegment* comp_seg = m_segments.Get(i);
		OP_ASSERT(comp_seg);
		OP_ASSERT(comp_seg->member.list == this);
		OP_ASSERT(comp_seg->member.idx == i);
		OP_ASSERT(comp_seg->member.normidx == j);
		j += comp_seg->GetNormalizedCount();
	}
}
#endif // SVG_EXPENSIVE_SYNC_TESTS

/* virtual */ BOOL
OpBpath::InitializeAnimationValue(SVGAnimationValue& animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_OPBPATH;
	animation_value.reference.svg_path = this;
	return TRUE;
}

#ifdef SVG_FULL_11
/* virtual */ BOOL
SVGPathSegObject::IsEqual(const SVGObject &other) const
{
	if (other.Type() == SVGOBJECT_PATHSEG)
	{
		const SVGPathSegObject &other_pathseg = static_cast<const SVGPathSegObject &>(other);

		// Ignores flags on memberflags on SVGPathSegObject because
		// they should not affect the presentation of this path segment
		return seg == other_pathseg.seg;
	}
	else
	{
		return FALSE;
	}
}
#endif // SVG_FULL_11

#ifdef SVG_PATH_TO_VEGAPATH
#include "modules/libvega/vegapath.h"

#define VNUM(s) ((s).GetVegaNumber())

OP_STATUS ConvertSVGPathToVEGAPath(SVGPath* outline,
								   VEGA_FIX ofs_x, VEGA_FIX ofs_y,
								   VEGA_FIX flatness,
								   VEGAPath* voutline)
{
	OpBpath* path = static_cast<OpBpath*>(outline);

	UINT32 cmdcount = path->GetCount(TRUE);

	// No work to be done
	if (cmdcount == 0)
		return OpStatus::OK;

	PathSegListIterator* iter = path->GetPathIterator(TRUE); /* normalized */
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = OpStatus::OK;
	for (unsigned int i = 0; i < cmdcount && OpStatus::IsSuccess(status); i++)
	{
		const SVGPathSeg* cmd = iter->GetNext();

		switch(cmd->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
			status = voutline->moveTo(VNUM(cmd->x) + ofs_x,
									  VNUM(cmd->y) + ofs_y);
			break;
		case SVGPathSeg::SVGP_LINETO_ABS:
			status = voutline->lineTo(VNUM(cmd->x) + ofs_x,
									  VNUM(cmd->y) + ofs_y);
			break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			status = voutline->cubicBezierTo(VNUM(cmd->x) + ofs_x,
											 VNUM(cmd->y) + ofs_y,
											 VNUM(cmd->x1) + ofs_x,
											 VNUM(cmd->y1) + ofs_y,
											 VNUM(cmd->x2) + ofs_x,
											 VNUM(cmd->y2) + ofs_y,
											 flatness);
			break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
			status = voutline->quadraticBezierTo(VNUM(cmd->x) + ofs_x,
												 VNUM(cmd->y) + ofs_y,
												 VNUM(cmd->x1) + ofs_x,
												 VNUM(cmd->y1) + ofs_y,
												 flatness);
			break;
		case SVGPathSeg::SVGP_ARC_ABS:
			status = voutline->arcTo(VNUM(cmd->x) + ofs_x,
									 VNUM(cmd->y) + ofs_y,
									 VNUM(cmd->x1), VNUM(cmd->y1),
									 VNUM(cmd->x2),
									 cmd->info.large ? true : false,
									 cmd->info.sweep ? true : false,
									 flatness);
			break;
		case SVGPathSeg::SVGP_CLOSE:
			status = voutline->close(true);
			break;
		}
	}

	OP_DELETE(iter);

	return status;
}

#undef VNUM
#endif // SVG_PATH_TO_VEGAPATH

#endif // SVG_SUPPORT
