/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
#include "core/pch.h"

#ifdef _PNG_SUPPORT_

#include "modules/zlib/zlib.h"
#include "modules/minpng/minpng.h"
#include "modules/minpng/png_int.h"

#ifndef MINPNG_NO_GAMMA_SUPPORT
static void _set_gamma(float gamma, minpng_state* s)
{
	int i;
	if (gamma == 0.0)
	{
		return;
	}
	s->gamma = ((float)1.0) / gamma;
	if ((float)op_fabs(s->gamma - (float)1.0) < (float)0.01)
	{
		s->gamma = 0.0;
		return;
	}
	s->gamma_table[0] = 0;
	s->gamma_table[255] = 255;
	for (i = 1; i < 255; i++)
		s->gamma_table[i] = (int)(op_pow((float)(i * (1 / 255.0)), (float)s->gamma) * 255);
}
#endif

#define NEW_IMAGE( X, Y, indexed ) _alloc_image(X,Y,indexed)

void *_alloc_image(int w, int h, BOOL indexed)
{
	// when indexed, add 8 bytes to handle "optimization" in _decode
	if(indexed)
		return OP_NEWA(UINT8, w * h + 8);
	return OP_NEWA(RGBAGroup, w * h);
}

static inline long _int_from_16bit(const unsigned char* data)
{
	return (data[0] << 8) | (data[1]);
}

/* Faster as inline, but somewhat larger. Not called in time-critical code anyway. */
static unsigned long _int_from_32bit(const unsigned char* data)
{
	return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
}

// FIXME: If this was moved to a callback we could save quite a lot of
// memory by not having a deinterlace buffer.
static inline void _draw_rectangle_rgba(RGBAGroup *dest, int span, int width, int height, RGBAGroup col)
{
	for (int y = 0; y < height; y++, dest += span)
		for (int x = 0; x < width; x++)
			dest[x]=col;
}

// FIXME: If this was moved to a callback we could save quite a lot of
// memory by not having a deinterlace buffer.
static inline void _draw_rectangle_indexed(UINT8 *dest, int span, int width, int height, UINT8 index)
{
	for (int y = 0; y < height; y++, dest += span)
		for (int x = 0; x < width; x++)
			dest[x]=index;
}

// width and height checked not to overflow when multipled by values
// smaller than 16 _or_ with eachother.
static int convert_to_rgba(RGBAGroup* w1, int type, int bpp, unsigned char* s,
						   long width, long height, minpng_palette* ct,
						   minpng_trns* trns)
{
	/* returns 1 if alpha channel, 0 if not */
	RGBAGroup* d1 = w1;
	int i;
	int yp, x;

	if (ct && trns)
		for (i = 0; i < 256; i++)
			ct->palette[i].a = trns->str[i];

	switch( type )
	{
		case 0:  /* 1,2,4,8 or 16 bit greyscale */
			{
				int tcol = -1;
				if (trns && trns->len >= 2)
					tcol = _int_from_16bit(trns->str);
#ifndef LIMITED_FOOTPRINT_DEVICE
				if (bpp == 8)
				{
					for (yp=0; yp < height; yp++)
					{
						s++; /* filter method. */
						for (x = 0; x < width; x++, s++)
						{
							d1[x].a = *s != tcol ? 255 : 0;
							d1[x].r = d1[x].g = d1[x].b = *s;
						}
						d1 += width;
					}
				}
				else
#endif
				if (bpp < 16)
				{
					/* Rather slow, but somewhat compact, routine for all bpp < 16
					 * It's not all _that_ slow either.
					 */
					unsigned int msk = (1 << bpp) - 1;
					/*	     unsigned int sft   = bpp==4 ? 2 : bpp-1; */
					for (yp = 0; yp < height; yp++)
					{
						s++;
						for (x = width; x > 0; s++)
						{
							for (i = 8-bpp; i >= 0 && x--; i -= bpp)
							{
								int c = ((*s) >> i) & msk;
								d1->r = d1->g = d1->b = (c * 255 / msk);
								d1->a = c == tcol ? 0 : 255;
								d1++;
							}
						}
					}
				}
				else
				{
					for (yp = 0; yp < height; yp++)
					{
						s++;
						for (x = 0; x < width; x++, s += 2)
						{
							int c = _int_from_16bit(s);
							d1[x].a = c != tcol ? 255 : 0;
							d1[x].r = d1[x].g = d1[x].b = c >> 8;
						}
						d1 += width;
					}
				}
				return tcol >= 0 ? 1 : 0;
			}

		case 2: /* 8 or 16 bit r,g,b */
			{
				int tr = -1, tg = -1, tb = -1;
				if (trns && trns->len >= 6)
				{
					tr = _int_from_16bit(trns->str);
					tg = _int_from_16bit(trns->str + 2);
					tb = _int_from_16bit(trns->str + 4);
				}
				if (bpp == 8)
				{
					for (yp = 0; yp < height; yp++)
					{
						s++;
						for (x = 0; x < width; x++, d1++)
						{
							int r = *(s++);
							int g = *(s++);
							int b = *(s++);
							d1->r = r;
							d1->g = g;
							d1->b = b;
							d1->a = ((r == tr) && (g == tg) && (b == tb)) ? 0 : 255;
						}
					}
				}
				else
				{
					for (yp = 0; yp < height; yp++)
					{
						s++;
						for (x = 0; x < width; x++, d1++)
						{
							int r = _int_from_16bit(s), g = _int_from_16bit(s + 2), b = _int_from_16bit(s + 4);
							s += 6;
							d1->r = r >> 8;
							d1->g = g >> 8;
							d1->b = b >> 8;
							d1->a = ((r == tr) && (g == tg) && (b == tb)) ? 0 : 255;
						}
					}
				}
				return tr < 0 ? 0 : 1;
			}

		case 3: /* 1,2,4,8 bit palette index. Alpha might be in palette */
			{
				unsigned int msk = (1 << bpp) - 1;
				for (yp = 0; yp < height; yp++)
				{
					s++;
					for (x = width; x > 0; s++)
					{
						for (i = 8 - bpp; i >= 0 && x--; i -= bpp)
						{
							unsigned char col_idx = ((*s) >> i) & msk;
							if (col_idx < ct->size)
								*(d1++) = ct->palette[col_idx];
							else
							{
								RGBAGroup black = { 0,0,0,255 };
								*(d1++) = black;
							}
						}
					}
				}
				return !!trns; /* Alpha channel if trns chunk. */
			}

		case 4: /* 8 or 16 bit grey,a */
			if (bpp == 8)
			{
				for (yp = 0; yp < height; yp++)
				{
					s++;
					for (x = 0; x < width; x++)
					{
						d1->r = d1->g = d1->b = s[0];
						(d1++)->a = s[1];
						s += 2;
					}
				}
			}
			else
			{
				for (yp = 0; yp < height; yp++)
				{
					s++;
					for (x = 0; x < width; x++)
					{
						d1->r = d1->g = d1->b = s[0];
						(d1++)->a = s[2];
						s += 4;
					}
				}
			}
			return 1;

		case 6: /* 8 or 16 bit r,g,b,a */
			if (bpp == 8)
			{
				for (yp = 0; yp < height; yp++)
				{
					s++;
					for (x = 0; x < width; x++)
					{
						d1->r = s[0];
						d1->g = s[1];
						d1->b = s[2];
						(d1++)->a = s[3];
						s += 4;
					}
				}
				return 1;
			}
			else
			{
				for (yp = 0; yp < height; yp++)
				{
					s++;
					for (x = 0; x < width; x++)
					{
						d1->r = s[0];
						d1->g = s[2];
						d1->b = s[4];
						(d1++)->a = s[6];
						s += 8;
					}
				}
			}
			return 1; /* always alpha channel */
	}
	return -1;
}

static void _defilter(minpng_buffer* ps, unsigned int linew, unsigned int height,
					  char type, char bpp, unsigned int* last_line, unsigned int* left,
					  unsigned char** line_start)
{
	unsigned char* row;
	unsigned int len = ps->size();
	unsigned int yp, xp;
	unsigned int bps;
	char upp = 1;

	/* -1 in an unsigned. This is ok here, and is used to indicate
	 * that not a single line was decoded. last_line=0 means that one
	 * line was decoded, namely line number 0.
	 */
	*last_line = (unsigned int)-1;

	*left = len;
	if (!(type & 1)) /* This generated 16 bytes smaller code than a switch. :-) */
	{
		if (type & 4)
			upp += 1;
 		if(type & 2)
			upp+=2;
	}
	bpp *= upp;                 /* multipy for units/pixel (rgba=4, grey=1, etc) */
	linew = (linew*bpp+7) >> 3; /* linew in bytes (round up to even bytes) */
	bps = (bpp + 7) >> 3;       /* bytes per sample up */

	if (len < (linew + 1) * MIN(height, 2))
		return;

	row = ps->ptr();

	if (len >= (linew + 1) * height) /* Whole image is in there. */
	{
		*last_line  = height - 1;
		*line_start = row + (height) * (linew + 1);
		*left -= (linew + 1) * (height);
	}
	else
	{
		height = len / (linew + 1);
		*last_line = len / (linew + 1) - 2;
		*line_start = row + (height - 1) * (linew + 1);
		*left -= (linew + 1) * (height - 1);
	}

	for (yp = 0; yp < height; yp++, row += linew + 1)
	{
		unsigned char *d = row + 1;
		switch (*row) // Defilter a single row of data.
		{
			case 0: /* No filter */
				break;

			case 1: /* Subtract left. */
				d += bps;
				for ( xp = bps; xp < linew; xp++, d++)
					*d = d[0] + d[-(int)bps];
				break;

			case 2: /* Subtract up. */
 				if (!yp)
					break; // Ignore if it's the first line...
				for (xp = 0; xp < linew; xp++)
					d[xp] += d[(int)xp - (int)linew - 1];
				break;

			case 3: /* Average left/up. */
				if (yp > 0)
					for (xp = 0; xp < linew; xp++, d++)
						*d += (d[-((int)linew + 1)] + (xp < bps ? 0 : d[-(int)bps])) >> 1;
				else
					for (xp = bps; xp < linew; xp++)
						d[xp] += d[(int)xp - (int)bps] >> 1;
				break;

			case 4: /* paeth (left up) */
				for (xp = 0 ; xp < linew; xp++, d++)
				{
					int a, q, c, pa, pb, pc;

					if (xp >= (unsigned)bps)
						a = d[-(int)bps];
					else
						a = 0;
					if (yp > 0)
					{
						q = d[-(int)(linew + 1)];
						if (xp >= (unsigned)bps)
							c = d[-(int)linew - 1 - (int)bps];
						else
							c = 0;
						pa= (q - c);
						if (pa < 0)
							pa = -pa;
					}
					else
						pa = c = q = 0;

					pb = (a - c);
					if (pb < 0)
						pb = -pb;

					pc = (a + q - (c << 1));
					if (pc < 0)
						pc = -pc;

					if (pa <= pb && pa <= pc)
						*d += a;
					else if (pb <= pc)
						*d += q;
					else
						*d += c;
				}
				break;
		}
		// Set filter to 0 for this row for the next pass, if any.
		// Sometimes rows are looked at twice with the current algorithm.
		*row = 0;
	}
	return;
}

static unsigned int _decode(minpng_state* s, PngRes* res, int width, int height)
{
	int last_line, left;
	unsigned char* line_start = 0;
	_defilter(&s->uncompressed_data, width, height, s->ihdr.type, s->ihdr.bpp,
			  (unsigned int*)&last_line, (unsigned int*)&left, &line_start);
	last_line++;
	res->image_data.data = 0;
	res->lines = last_line;

	if (last_line) /* At least one line decoded. */
	{
		res->image_data.data = NEW_IMAGE(width, last_line, s->output_indexed);
		res->nofree_image = 0;
		if (!res->image_data.data)
			return (unsigned int)-1;
		res->first_line = s->first_line;
		s->first_line += last_line;
		if(s->output_indexed)
		{
			int bpp = s->ihdr.bpp;
			res->has_alpha = FALSE;
			if(bpp == 8) // just copy indexed data
				for(int y=0;y<last_line;y++)
					op_memcpy(res->image_data.indexed + y*width, s->uncompressed_data.ptr() + y*(width+1) + 1, width);
			else  // unpack packed bytes
			{
				int per_byte = 8 / bpp;
				int packed_line_bytes = (width / per_byte) + ((width%per_byte) ? 1 : 0);
				for(int y=0;y<last_line;y++)
				{
					UINT8 *src = (UINT8*)(s->uncompressed_data.ptr() + y*(packed_line_bytes+1) + 1);
					UINT8 *dst = res->image_data.indexed + y*width;
					for(int x=0;x<packed_line_bytes;x++)
					{
						UINT8 pattern = ~((UINT8)(-1) >> bpp);
						int to_shift = 8 - bpp;
						/* Yes... we added some extra bytes while allocating res->image in order to
						 * allow some "overflow" here (<= 7 bytes) */
						for(int i=0;i<per_byte;i++)
						{
							*(dst++) = ((*src) & pattern) >> to_shift;
							pattern >>= bpp;
							to_shift -= bpp;
						}
						src++;
					}
				}
			}
		}
		else
		{
			res->has_alpha = convert_to_rgba(res->image_data.rgba, s->ihdr.type, s->ihdr.bpp,
											 s->uncompressed_data.ptr(), width, last_line,
											 s->ct, s->trns);
#ifndef MINPNG_NO_GAMMA_SUPPORT
			// if s->ihdr.type == 3, then has gamma already been adjusted
			if (!s->ignore_gamma && s->gamma != 0.0 && s->ihdr.type != 3)
			{
				int i;
				for (i = 0; i < width * last_line; i++)
				{
					res->image_data.rgba[i].r = s->gamma_table[res->image_data.rgba[i].r];
					res->image_data.rgba[i].g = s->gamma_table[res->image_data.rgba[i].g];
					res->image_data.rgba[i].b = s->gamma_table[res->image_data.rgba[i].b];
				}
			}
#endif
		}

		s->uncompressed_data.consume(line_start-s->uncompressed_data.ptr());
	}
	return last_line;
}

/* Only used once, thus inline and here. */
static inline minpng_palette* _palette_new(int size, unsigned char* data)
{
	minpng_palette* t = OP_NEW(minpng_palette, ());
	if (!t)
		return 0;
	int i;
	t->size = size;
	for (i = 0; i < size; i++)
	{
		t->palette[i].a = 255;
		t->palette[i].r = *(data++);
		t->palette[i].g = *(data++);
		t->palette[i].b = *(data++);
	}
	return t;
}

static inline PngRes::Result on_first_idat_found(minpng_state *s)
{
#ifndef MINPNG_NO_GAMMA_SUPPORT
	if(s->image_frame_data_pal && s->gamma != 0.0)
	{
		for(int i=s->pal_size-1;i>=0;i--)
		{
			s->image_frame_data_pal[i*3+0] = s->gamma_table[s->image_frame_data_pal[i*3+0]];
			s->image_frame_data_pal[i*3+1] = s->gamma_table[s->image_frame_data_pal[i*3+1]];
			s->image_frame_data_pal[i*3+2] = s->gamma_table[s->image_frame_data_pal[i*3+2]];
		}
	}
#endif
	if(s->ihdr.type == 3 && !s->image_frame_data_pal)
	{
		return PngRes::ILLEGAL_DATA;
	}
	OP_ASSERT(!s->output_indexed || (!s->trns && (s->ihdr.type == 3 ||
		(s->ihdr.type == 0 && s->ihdr.bpp <= 8))));
	if(s->output_indexed && s->ihdr.type == 0)
	{
		int indexes = 1 << s->ihdr.bpp;
		s->pal_size = indexes;
		s->image_frame_data_pal = OP_NEWA(UINT8, indexes*3);
		if (!s->image_frame_data_pal)
		{
			return PngRes::OOM_ERROR;
		}
#ifndef MINPNG_NO_GAMMA_SUPPORT
		if(s->gamma != 0.0)
		{
			for(int i=0;i<indexes;i++)
			{
				UINT8 val = (UINT8)(s->gamma_table[(255*i) / (indexes-1)]);
				s->image_frame_data_pal[i*3+0] = val;
				s->image_frame_data_pal[i*3+1] = val;
				s->image_frame_data_pal[i*3+2] = val;
			}
		}
		else
#endif
		{
			for(int i=0;i<indexes;i++)
			{
				UINT8 val = (UINT8)((i*255) / (indexes-1));
				s->image_frame_data_pal[i*3+0] = val;
				s->image_frame_data_pal[i*3+1] = val;
				s->image_frame_data_pal[i*3+2] = val;
			}
		}
	}
	else if(!s->output_indexed && s->ihdr.type == 3)
	{
		s->ct = _palette_new(s->pal_size,s->image_frame_data_pal);
		if(!s->ct)
		{
			return PngRes::OOM_ERROR;
		}
		OP_DELETEA(s->image_frame_data_pal);
		s->image_frame_data_pal = NULL;
	}
#ifdef APNG_SUPPORT
	if(!s->is_apng)
#endif
	{
		s->frame_index = 0;
	}

	s->first_idat_found = TRUE;
	return PngRes::OK;
}

#ifdef EMBEDDED_ICC_SUPPORT
static inline PngRes::Result on_icc_profile_found(minpng_state* s, PngRes* res,
												  unsigned char* icc_data, int icc_data_len)
{
	minpng_zbuffer zbuf;

	PngRes::Result png_res = zbuf.init();
	if (png_res != PngRes::OK)
		return png_res;

	if (zbuf.append(icc_data_len, icc_data) != 0)
		return PngRes::OOM_ERROR;

	if (!s->buf)
	{
		s->buf = OP_NEWA(unsigned char, MINPNG_BUFSIZE);
		if (!s->buf)
		{
			return PngRes::OOM_ERROR;
		}
	}

	minpng_buffer decomp_icc_data;

	do
	{
		int res = zbuf.read(s->buf, MINPNG_BUFSIZE);
		if (res <= 0)
			return res == minpng_zbuffer::Oom ?
				PngRes::OOM_ERROR : PngRes::ILLEGAL_DATA;

		if (decomp_icc_data.append(res, s->buf) != 0)
			return PngRes::OOM_ERROR;

	} while (zbuf.size());

	// Put data in result
	res->icc_data = decomp_icc_data.ptr();
	res->icc_data_len = decomp_icc_data.size();

	// The result now owns the data
	decomp_icc_data.release();

#ifndef MINPNG_NO_GAMMA_SUPPORT
	// If gamma has been set, disable it
	s->gamma = 0.0;
#endif
	return PngRes::OK;
}
#endif

#define CONSUME(X) do { s->consumed += (X); data += (X); len -= (X); } while(0)

static const interlace_state adam7[8] =
{
	{0,8,8,  0,8,8},
	{0,8,8,  4,8,4},
	{4,8,4,  0,4,4},
	{0,4,4,  2,4,2},
	{2,4,2,  0,2,2},
	{0,2,2,  1,2,1},
	{1,2,1,  0,1,1}
};

PngRes::Result minpng_decode(PngFeeder* feed_me, PngRes* res)
{
	minpng_state* s;
	unsigned char* data;
	unsigned int len;

	if (!feed_me->state)
	{
		feed_me->state = s =  minpng_state::construct();
		if (!s)
			return PngRes::OOM_ERROR;
#ifndef MINPNG_NO_GAMMA_SUPPORT
		_set_gamma(((float)feed_me->screen_gamma) * (float)(1.0 / 2.2), s );
#endif
	}
	else
		s = (minpng_state*)feed_me->state;
	s->ignore_gamma = feed_me->ignore_gamma;

	res->lines = 0;
	res->draw_image = TRUE;
	if (s->consumed)
	{
		s->current_chunk.consume(s->consumed);
		s->consumed = 0;
	}

	// Skip extra malloc/switch/copy/free step.
	if (feed_me->len)
	{
		if (s->state == STATE_IDAT)
		{
			int cons = MIN(s->more_needed, feed_me->len);
			if (cons)
			{
				if (s->compressed_data.append(cons, feed_me->data))
				{
					return PngRes::OOM_ERROR;
				}
				feed_me->len -= cons;
				s->more_needed -= cons;
				feed_me->data += cons;
				if(!s->more_needed)
					s->state = STATE_CRC;
			}
		}
		/* Check again, data consumed above.*/
		if (feed_me->len)
		{
			if (s->current_chunk.append(feed_me->len, feed_me->data))
			{
				return PngRes::OOM_ERROR;
			}
			feed_me->len = 0;
		}
	}
	data = s->current_chunk.ptr();
	len = s->current_chunk.size();

	while (len)
	{
		switch (s->state)
		{
			case STATE_INITIAL: /* No header read yet. */
				if (len < 8 + 8 + 13 + 4 + 8)
				{ /* Header and IHDR size, and size of next chunk header. */
					return PngRes::NEED_MORE; /* return directly, no received data. */
				}

				if (data[0] != 137 || data[1] != 'P' || data[2] != 'N' || data[3] != 'G' ||
					data[4] != 13 || data[5] != 10 || data[6] != 26 || data[7]!=10)
				{
					return PngRes::ILLEGAL_DATA;
				}
				else
				{
					int id = _int_from_32bit(data + 12);
					unsigned char bpp, type;
					s->more_needed = _int_from_32bit(data + 8);
					if (id != 0x49484452 || s->more_needed != 13) {/* IHDR */
						return PngRes::ILLEGAL_DATA;
					}
					s->mainframe_x = _int_from_32bit(data + 16);
					s->mainframe_y = _int_from_32bit(data + 20);
					bpp = s->ihdr.bpp = data[24];
					type = s->ihdr.type = data[25];

					// Verify that there will be no overflows.

					// FIXME: This is not really true. (returning OOM
					// for images larger than ~2G). We limit to 2Gb to
					// avoid signed integer overflow in any routines.
					s->pixel_count = (float)s->mainframe_x * (float)s->mainframe_y * (float)4.0;
					if (s->pixel_count > 2.0e9 || (s->mainframe_x > (1 << 25)) || (s->mainframe_y > (1 << 25)))
					{
						return PngRes::OOM_ERROR;
					}

					s->rect.Set(0, 0, s->mainframe_x, s->mainframe_y);

					// Check bpp and type.
					// From the spec:
					//   Color    Allowed    Interpretation
					//    Type    Bit Depths
					//
					//    0       1,2,4,8,16  Each pixel is a grayscale sample.
					//
					//    2       8,16        Each pixel is an R,G,B triple.
					//
					//    3       1,2,4,8     Each pixel is a palette index;
					//                        a PLTE chunk must appear.
					//
					//    4       8,16        Each pixel is a grayscale sample,
					//                        followed by an alpha sample.
					//
					//    6       8,16        Each pixel is an R,G,B triple,
					//                        followed by an alpha sample.
					switch (type)
					{
						case 2: case 4:	case 6:
							if (bpp != 8 && bpp != 16)
							{
								return PngRes::ILLEGAL_DATA;
							}
							break;

						case 0: case 3:
							if (bpp != 8 && bpp != 4 && bpp != 2 && bpp != 1
								&& (type || bpp != 16 ))
							{
								return PngRes::ILLEGAL_DATA;
							}
#if defined(SUPPORT_INDEXED_OPBITMAP)
							if(bpp <= 8)
							{
								s->output_indexed = TRUE;
							}
#endif
							break;

						default:
							return PngRes::ILLEGAL_DATA;
					}
					// Check compression. Must be 0
					if (data[26] != 0)
					{
						return PngRes::ILLEGAL_DATA;
					}

					// Check filter. Must be 0.
					if (data[27] != 0)
					{
						return PngRes::ILLEGAL_DATA;
					}

					s->ihdr.interlace = data[28];
					CONSUME(13 + 4 + 8 + 8);
				}
				/* Fallthrough.. */

			case STATE_CHUNKHEADER: /* Reading generic chunk header. */
				if (len < 8)
					goto decode_received_data;

				{
					ChunkID id  = (ChunkID)_int_from_32bit(data + 4);
					s->more_needed = _int_from_32bit(data);
					CONSUME(8);
					switch (id)
					{
						case CHUNK_PLTE:
							s->state = STATE_PLTE;
							if (s->ct || s->image_frame_data_pal || s->ihdr.type != 3)
								s->state = STATE_IGNORE; // Already have one, or no needed.
							break;
						case CHUNK_IDAT:
							if(!s->first_idat_found)
							{
								PngRes::Result result = on_first_idat_found(s); // sets s->first_idat_found=TRUE
								if(result != PngRes::OK)
									return result;
							}
#ifdef APNG_SUPPORT
							if(s->is_apng && s->apng_seq == 0)
							{
								// This is the default-frame which should be ignored (no fcTL found before this IDAT)
								s->state = STATE_IGNORE;
							}
							else
#endif
							{
								s->state = STATE_IDAT;
							}
							break;
						case CHUNK_tRNS:
							s->state = s->first_idat_found ? STATE_IGNORE : STATE_tRNS;
							break;
#ifndef MINPNG_NO_GAMMA_SUPPORT
						case CHUNK_gAMA:
							s->state = STATE_gAMA;
							// Long enough?
							if (s->more_needed < 4)
								s->state = STATE_IGNORE;
#ifdef EMBEDDED_ICC_SUPPORT
							// If an ICC profile is present we use that instead
							if (s->first_iccp_found)
								s->state = STATE_IGNORE;
#endif // EMBEDDED_ICC_SUPPORT
							break;
#endif
#ifdef EMBEDDED_ICC_SUPPORT
						case CHUNK_iCCP:
							s->state = s->first_iccp_found ? STATE_IGNORE : STATE_iCCP;
							break;
#endif
#ifdef APNG_SUPPORT
						case CHUNK_fdAT:
							s->state = STATE_fdAT;
							break;
						case CHUNK_fcTL:
							s->state = STATE_fcTL;
							break;
						case CHUNK_acTL:
							s->state = s->is_apng ? STATE_IGNORE : STATE_acTL;
							break;
							/* Fallthrough */
#endif

						default:
							s->state = STATE_IGNORE;
							break;
					}
					break;
				}

#ifndef MINPNG_NO_GAMMA_SUPPORT
			case STATE_gAMA: /* gAMA */
				if (len < s->more_needed)
					goto decode_received_data;
				_set_gamma((float)(_int_from_32bit(data) / 100000.0) * (float)(feed_me->screen_gamma), s);
				CONSUME(s->more_needed);
				s->state = STATE_CRC;
				break;
#endif

#ifdef EMBEDDED_ICC_SUPPORT
			case STATE_iCCP: /* iCCP chunk */
			{
				if (len < s->more_needed)
					goto decode_received_data;

				unsigned char* icc_data = data;
				int icc_data_len = s->more_needed;
				while (*icc_data && icc_data_len != 0)
					icc_data++, icc_data_len--;

				if (icc_data_len > 2 && *(icc_data+1) == 0)
				{
					if (on_icc_profile_found(s, res, icc_data+2, icc_data_len-2) == PngRes::OOM_ERROR)
						return PngRes::OOM_ERROR;

					s->first_iccp_found = TRUE;
				}

				CONSUME(s->more_needed);
				s->state = STATE_CRC;
				break;
			}
#endif

			case STATE_PLTE:  /* PLTE chunk */
				if (len < s->more_needed)
					goto decode_received_data;

				// We only support at most 256 entries in the
				// palette (as per the specification, incidentally).
				s->pal_size = MIN(s->more_needed/3,256);
				s->image_frame_data_pal = OP_NEWA(UINT8, s->pal_size*3);
				if (!s->image_frame_data_pal)
				{
					return PngRes::OOM_ERROR;
				}
				op_memcpy(s->image_frame_data_pal,data,s->pal_size*3);
				CONSUME(s->more_needed);
				s->state = STATE_CRC;
				break;

#ifdef APNG_SUPPORT
			case STATE_acTL:
				if (len < s->more_needed)
					goto decode_received_data;
				s->is_apng = TRUE;
				s->num_frames = _int_from_32bit(data);
				if(!s->num_frames)
					return PngRes::ILLEGAL_DATA;
				s->num_iterations = _int_from_32bit(data+4);
				CONSUME(s->more_needed);
				s->state = STATE_CRC;
				break;

			case STATE_fcTL:
			{
				if (len < s->more_needed)
					goto decode_received_data;
				if(s->more_needed < 26)
					return PngRes::ILLEGAL_DATA;
				if(s->compressed_data.size()) // We're not finished with the previous frame, continue to decode that one instead
					goto decode_received_data;
				if(s->frame_index != s->last_finished_index) // frame_index is the index for the previous frame
					return PngRes::ILLEGAL_DATA; // The previous frame didn't contain all the data it was supposed to
				if(_int_from_32bit(data) != s->apng_seq++)
					return PngRes::ILLEGAL_DATA;

				unsigned int apng_rect_x = _int_from_32bit(data+12);
				unsigned int apng_rect_y = _int_from_32bit(data+16);
				unsigned int apng_rect_w = _int_from_32bit(data+4);
				unsigned int apng_rect_h = _int_from_32bit(data+8);
				s->pixel_count += (float)apng_rect_w * (float)apng_rect_h * (float)4.0;
				if (s->pixel_count > 2.0e9 || (apng_rect_w > (1 << 25)) || (apng_rect_h > (1 << 25)))
				{
					return PngRes::OOM_ERROR;
				}
				// Make sure the frame rect is inside the image (first check x/y agains max width/height to avoid overflows)
				if ((apng_rect_x > (1 << 25)) || (apng_rect_y > (1 << 25)) ||
					((apng_rect_x + apng_rect_w) > s->mainframe_x) || ((apng_rect_y+apng_rect_h) > s->mainframe_y))
					return PngRes::ILLEGAL_DATA;

				s->frame_index++;
				s->rect.Set(apng_rect_x, apng_rect_y, apng_rect_w, apng_rect_h);

				s->adam_7_state = 0;
				if(s->compressed_data.re_init())
					return PngRes::OOM_ERROR;

				int numerator = _int_from_16bit(data+20);
				int denom = _int_from_16bit(data+22);
				if(!denom)
				{
					// Spec says: If the denominator is 0, it is to be treated as if it were 100
					denom = 100;
				}
				if (!numerator)
				{
					// Spec says: If the value of the numerator is 0 the decoder should render the next frame as quickly as possible, though viewers may impose a reasonable lower bound.
					// Firefox 3.0b3 seems to implement a lower bound when numerator < 10 (demon 1000) but allow faster speed than the lower bound if numerator > 10. I'm not clear exactly
					// how and if we should do the same.
					s->duration = 10;
				}
				else
					s->duration = (100*numerator)/denom;
				if(data[24] == 1)
					s->disposal_method = DISPOSAL_METHOD_RESTORE_BACKGROUND;
				else if(data[24] == 2)
					s->disposal_method = DISPOSAL_METHOD_RESTORE_PREVIOUS;
				else
					s->disposal_method = DISPOSAL_METHOD_DO_NOT_DISPOSE;
				s->blend = data[25] != 0;

				CONSUME(s->more_needed);
				s->state = STATE_CRC;
				break;
			}
			case STATE_fdAT: // Sequence-nr + IDAT content

				// Though perhaps a bit vague,
				// https://wiki.mozilla.org/APNG_Specification seems to
				// strongly imply that an fdAT chunk must never appear before
				// the first IDAT chunk.
				//
				// We also perform some initialization upon seeing the first
				// IDAT (in on_first_idat_found()), which if not done triggers
				// CORE-34284 upon processing the first fdAT.
				if(!s->first_idat_found)
					return PngRes::ILLEGAL_DATA;

				if(s->more_needed < 4)
					return PngRes::ILLEGAL_DATA;
				if(s->frame_index == s->last_finished_index)
					return PngRes::ILLEGAL_DATA; // We havn't got any fcTL preceding this fdAT
				if (len < 4)
					goto decode_received_data;
				if(_int_from_32bit(data) != s->apng_seq++)
					return PngRes::ILLEGAL_DATA;
				s->more_needed -= 4;
				CONSUME(4);
				s->state = s->more_needed ? STATE_IDAT : STATE_CRC;
				break;

#endif // APNG_SUPPORT

			case STATE_IDAT:
				{
					int cons = MIN(s->more_needed,len);
					if (s->compressed_data.append(cons, data))
					{
						return PngRes::OOM_ERROR;
					}

					s->more_needed -= cons;
					CONSUME(cons);
					if (!s->more_needed)
					{
						s->state = STATE_CRC;
						break;
					}
					else
						goto decode_received_data;
				}

			case STATE_tRNS:
				if (len < s->more_needed)
					goto decode_received_data;
#if defined(SUPPORT_INDEXED_OPBITMAP)
				if(s->ihdr.type == 3 && s->more_needed == 1 && data[0] == 0)
				{
					s->has_transparent_col = TRUE;
					s->transparent_index = 0;
					CONSUME(1);
					s->state = STATE_CRC;
					break;
				}
				if(s->ihdr.type == 0 && s->image_frame_data_pal && s->more_needed == 2)
				{
					s->has_transparent_col = TRUE;
					s->transparent_index = data[1];
					CONSUME(1);
					s->state = STATE_CRC;
					break;
				}
				s->output_indexed = FALSE;
#endif
				s->trns = OP_NEW(minpng_trns, ()); /* simplifies code elsewhere. */
				if (!s->trns)
				{
					return PngRes::OOM_ERROR;
				}
				s->trns->len = len;
				// We only support at most 256 entries in the
				// transparent table (as per the
				// specification, incidentally).
				{
					int trnslen = MIN(256, s->more_needed);
					op_memcpy(s->trns->str, data, trnslen);
					op_memset(s->trns->str + trnslen, 255, 256 - trnslen);
					CONSUME(s->more_needed);
					s->state = STATE_CRC;
				}
				break;

			case STATE_IGNORE: /* Ignore chunk. */
				{
					int cons = MIN(s->more_needed, len);
					CONSUME(cons);
					s->more_needed -= cons;
					if (s->more_needed)
						goto decode_received_data;
					s->state = STATE_CRC;
				}
// 				break;

			case STATE_CRC: /* CRC */
				if (len < 4)
					goto decode_received_data;
				CONSUME(4);
				s->state = STATE_CHUNKHEADER;
				break;

		}
	}

decode_received_data:
	{
		unsigned int last_line;

		if (s->ihdr.type == 3 && !s->ct && !s->image_frame_data_pal)
			return PngRes::NEED_MORE;

		if (!s->compressed_data.size())
			return PngRes::NEED_MORE;

		OP_ASSERT(s->first_idat_found);

		if (!s->buf)
		{
			s->buf = OP_NEWA(unsigned char, MINPNG_BUFSIZE);
			if (!s->buf)
			{
				return PngRes::OOM_ERROR;
			}
		}
		int bytes = s->compressed_data.read(s->buf, MINPNG_BUFSIZE);
		if (bytes <= 0)
		{
			if (bytes == minpng_zbuffer::Oom)
			{
				return PngRes::OOM_ERROR;
			}
			else if (bytes == minpng_zbuffer::IllegalData)
			{
				return PngRes::ILLEGAL_DATA;
			}
			else if (bytes == minpng_zbuffer::Eof) // There was "junk" after end of zlib-stream in IDAT
			{
				return PngRes::ILLEGAL_DATA;
			}
			else
			{
				OP_ASSERT(bytes == 0);
				if(s->first_line == unsigned(s->rect.height))
				{ // only ending zlib-data left (termination-symbol, adler-checksum...)
#if !defined(APNG_SUPPORT)
					OP_ASSERT(FALSE);
#endif
					s->compressed_data.re_init();
					return PngRes::AGAIN;
				}
				// we're before the actual data in the zlib-stream - or can't process even a single symbol,
				// might be if input-data is max 2 bytes.
				return PngRes::NEED_MORE;
			}
		}
		else if (s->uncompressed_data.append(bytes, s->buf))
		{
			return PngRes::OOM_ERROR;
		}

		PngRes::Result more_res = s->compressed_data.size() > 0 ? PngRes::AGAIN : PngRes::NEED_MORE;
#ifdef APNG_SUPPORT
		if(s->state == STATE_fcTL)
			more_res = PngRes::AGAIN;
#endif
		/* --- fill in info in the result struct. --- */
		res->depth = s->ihdr.bpp;

		int upp = 1;
		if (!(s->ihdr.type & 1)) /* This generated 16 bytes smaller code than a switch. :-) */
		{
			if (s->ihdr.type & 4)
				upp += 1;
			if (s->ihdr.type & 2)
				upp += 2;
		}
		res->depth *= upp;

		res->mainframe_x = s->mainframe_x;
		res->mainframe_y = s->mainframe_y;

		res->rect = s->rect;
#ifdef APNG_SUPPORT
		res->num_iterations = s->num_iterations;
		res->duration = s->duration;
		res->disposal_method = s->disposal_method;
		res->blend = s->blend;
#endif
		res->num_frames = s->num_frames;
		res->frame_index = s->frame_index;

		res->image_frame_data_pal = s->image_frame_data_pal;
		res->has_transparent_col = s->has_transparent_col;
		res->transparent_index = s->transparent_index;
		res->pal_colors = s->pal_size;
		res->interlaced = !!s->ihdr.interlace;

		if (!s->ihdr.interlace)
		{
			/* FIXME: Here it's possible to do row-by-row
			 * writing. There is really no need for the image buffer.
			 */
			last_line = _decode(s, res, s->rect.width, s->rect.height - s->first_line);
			if (res->has_alpha == -1)
			{
				return PngRes::ILLEGAL_DATA;
			}
			if ((int)last_line == -1)
			{
				return PngRes::OOM_ERROR;
			}
			if (s->rect.height-s->first_line)
				return more_res;
#ifdef APNG_SUPPORT
			s->last_finished_index = s->frame_index;
			if(s->compressed_data.size()) // It seems like we where given to much data
				s->compressed_data.re_init();
			
			if(s->num_frames > s->frame_index+1)
			{
				s->first_line = 0;
				return more_res;
			}
#endif
			return PngRes::OK;
		}
		else
		{
			if (s->rect.width * s->rect.height > (int)(s->deinterlace_image_pixels))
			{
				if(s->deinterlace_image.data)
				{
					if(s->output_indexed)
						OP_DELETEA(s->deinterlace_image.indexed);
					else
						OP_DELETEA(s->deinterlace_image.rgba);
				}
				s->deinterlace_image.data = NEW_IMAGE(s->rect.width, s->rect.height, s->output_indexed); // FIXME: _could_ be>>1 on height.
				if (!s->deinterlace_image.data)
				{
					return PngRes::OOM_ERROR;
				}
				s->deinterlace_image_pixels = s->rect.width * s->rect.height;
			}

			res->first_line = 0;
			for (; s->adam_7_state < 7; s->adam_7_state++)
			{
 				int st = s->adam_7_state;
				unsigned int y0 = adam7[st].y0;
				unsigned int x0 = adam7[st].x0;
				unsigned int xw = adam7[st].xd;
				unsigned int yw = adam7[st].yd;
				unsigned int ys = adam7[st].ys;

				unsigned int xs = adam7[st].xs;
				unsigned int iwidth =(s->rect.width - x0 + xw - 1) / xw;
				unsigned int iheight=(s->rect.height - y0 + yw - 1) / yw;

				int old_first = res->first_line;
				int old_lines = res->lines;
				if (!iwidth || !iheight)
					continue;

				last_line = _decode(s, res, iwidth, iheight - s->first_line);
				if ((int)last_line == -1)
				{
					return PngRes::OOM_ERROR;
				}
				res->first_line = old_first;
				res->lines = old_lines;
				if (res->has_alpha == -1)
				{
					return PngRes::ILLEGAL_DATA;
				}
				if (last_line) // We have res->image_data... else NULL
				{
					unsigned int sy = 0;
					unsigned int row = y0 + (s->first_line - last_line) * yw;
					if(s->output_indexed)
					{
						UINT8 *src = res->image_data.indexed;
						UINT8 *dest;
						while (row < unsigned(s->rect.height) && sy < last_line)
						{
							unsigned int sx = 0;
							unsigned int yh = MIN(ys, s->rect.height - row);
							UINT8* dend;
							dest = s->deinterlace_image.indexed + row * s->rect.width + x0;
							dend = dest + s->rect.width - x0;
#ifdef APNG_SUPPORT
							if(s->is_apng)
							{
								while(dest < dend)
								{
									*dest = src[sx++];
									dest += xw;
								}
							}
							else
#endif
							{
								while (dest < dend)
								{
									/* FIXME: This method (or a similar one) could be
									** implemented in the 'res' struct, thus skipping the
									** need for the deinterlaced image buffer completely.
									*/
									_draw_rectangle_indexed(dest, s->rect.width, MIN((int)xs, dend-dest), yh, src[sx++]);
									dest += xw;
								}
							}
							sy++;
							row += yw;
							src += iwidth;
						}
						OP_DELETEA(res->image_data.indexed);
					}
					else
					{
						RGBAGroup *src = res->image_data.rgba;
						RGBAGroup *dest;
						while (row < unsigned(s->rect.height) && sy < last_line)
						{
							unsigned int sx = 0;
							unsigned int yh = MIN(ys, s->rect.height - row);
							RGBAGroup* dend;
							dest = s->deinterlace_image.rgba + row * s->rect.width + x0;
							dend = dest + s->rect.width - x0;
#ifdef APNG_SUPPORT
							if(s->is_apng)
							{
								while(dest < dend)
								{
									*dest = src[sx++];
									dest += xw;
								}
							}
							else
#endif
							{
								while (dest < dend)
								{
									/* FIXME: This method (or a similar one) could be
									** implemented in the 'res' struct, thus skipping the
									** need for the deinterlaced image buffer completely.
									*/
									_draw_rectangle_rgba(dest, s->rect.width, MIN((int)xs, dend-dest), yh, src[sx++]);
									dest += xw;
								}
							}
							sy++;
							row += yw;
							src += iwidth;
						}
						OP_DELETEA(res->image_data.rgba);
					}
					if (res->lines)
					{
						unsigned int num_lines = MAX(y0 + s->first_line * yw, res->first_line + res->lines);
						if (num_lines > unsigned(s->rect.height))
							num_lines = s->rect.height;
						res->first_line = MIN(res->first_line, y0 + (s->first_line - last_line) * yw);
						res->lines = num_lines-res->first_line;
					}
					else
					{
						res->first_line = y0 + (s->first_line - last_line) * yw;
						res->lines = MIN((y0 + s->first_line * yw), unsigned(res->rect.height)) - res->first_line;
					}
				}
				res->nofree_image = 1;
				if(s->output_indexed)
				{
					OP_ASSERT(res->image_frame_data_pal);
					res->image_data.indexed = s->deinterlace_image.indexed + res->first_line * res->rect.width;
				}
				else
				{
					OP_ASSERT(!res->image_frame_data_pal);
					res->image_data.rgba = s->deinterlace_image.rgba + res->first_line * res->rect.width;
				}
				if (s->first_line < iheight)
				{
#ifdef APNG_SUPPORT
					if(s->is_apng)
					{
						res->draw_image = FALSE;
						minpng_clear_result(res);
					}
#endif
					return more_res;
				}
				else
				{
					s->first_line = 0;
				}
			}
#ifdef APNG_SUPPORT
			if(s->is_apng)
			{
				s->last_finished_index = s->frame_index;
				if(s->compressed_data.size()) // It seems like we where given to much data
					s->compressed_data.re_init();

				OP_ASSERT(res->nofree_image && res->draw_image);
				res->image_data.data = s->deinterlace_image.data;
				res->first_line = 0;
				res->lines = res->rect.height;
				if(res->frame_index < s->num_frames - 1)
					return more_res;
			}
#endif
			return PngRes::OK;
		}
	}
}

void minpng_clear_result(PngRes* res)
{
	if (!res->nofree_image)
	{
		if(res->image_frame_data_pal)
			OP_DELETEA(res->image_data.indexed);
		else
			OP_DELETEA(res->image_data.rgba);
	}
#ifdef EMBEDDED_ICC_SUPPORT
	OP_DELETEA(res->icc_data);
#endif // EMBEDDED_ICC_SUPPORT
	minpng_init_result(res);
}

void minpng_clear_feeder(PngFeeder* res)
{
	OP_DELETE((minpng_state*)res->state);
	res->state = NULL;
}

void minpng_init_result(PngRes* res)
{
	op_memset(res, 0, sizeof(PngRes));
}

void minpng_init_feeder(PngFeeder* res)
{
	op_memset(res, 0, sizeof(PngFeeder));
	res->screen_gamma = 2.2;
}


minpng_state::~minpng_state()
{
	OP_DELETE(ct);
	OP_DELETE(trns);
	if(output_indexed)
		OP_DELETEA(deinterlace_image.indexed);
	else
		OP_DELETEA(deinterlace_image.rgba);
	OP_DELETEA(buf);
	OP_DELETEA(image_frame_data_pal);
}

minpng_state* minpng_state::construct()
{
	minpng_state* state = OP_NEW(minpng_state, ());
	if (state == NULL)
		return NULL;
	PngRes::Result res = state->init();
	if (res == PngRes::OK) // Take care of different errors later.
		return state;
	OP_DELETE(state);
	return NULL;
}

PngRes::Result minpng_state::init()
{
	return compressed_data.init();
}

minpng_state::minpng_state()
{
	more_needed = consumed = first_line = 0;
	state = adam_7_state = 0;
	pal_size = 0;
	output_indexed = has_transparent_col = FALSE;
	transparent_index = 0;
	first_idat_found = FALSE;
	image_frame_data_pal = NULL;
	ct = NULL;
	trns = NULL;
	buf = NULL;
	deinterlace_image_pixels = 0;
#ifdef APNG_SUPPORT
	is_apng = FALSE;
	apng_seq = 0;
	last_finished_index = -1;
#endif
#ifdef EMBEDDED_ICC_SUPPORT
	first_iccp_found = FALSE;
#endif
	frame_index = -1;
	num_frames = 1;
	pixel_count = 0;
	mainframe_x = 0;
	mainframe_y = 0;
#if !defined(MINPNG_NO_GAMMA_SUPPORT)
	gamma = 0.0;
#endif
	deinterlace_image.data = NULL;
	op_memset(&ihdr, 0, sizeof(ihdr));
}

#endif // _PNG_SUPPORT_
