/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/src/vegarasterizer.h"
#include "modules/libvega/vegapath.h"

static const int VegaSampleSize[] = {
	1,
	2,
	4,
	8,
	16
};
static const unsigned int VegaSampleSizeShift[] = {
	0,
	1,
	2,
	3,
	4
};
static const unsigned int VegaAlphaShift[] = {
	8,
	6,
	4,
	2,
	0
};

typedef INT32 VEGA_SUBPIX;

#define VEGA_SUBPIX_PREC 12

// => VEGA_FIXTOINT(VEGA_FLOOR((v)))
#define VEGA_FLOORSUBPIX(v) ((v) >> VEGA_SUBPIX_PREC)
// => VEGA_FIXTOINT(v)
#define VEGA_ROUNDSUBPIX(v) (((v) + (1 << (VEGA_SUBPIX_PREC - 1))) >> VEGA_SUBPIX_PREC)

struct VEGASortedLineList
{
	VEGA_SUBPIX xpos;
	VEGA_SUBPIX slope;
	unsigned int currSample;
	unsigned int lastSample : 31;
	unsigned int isUp : 1;

	VEGASortedLineList* next;
};

struct VEGALineQueue
{
	void append(VEGALineQueue& vlq)
	{
		if (last)
			last->next = vlq.first;
		else
			first = vlq.first;

		last = vlq.last;
	}

	void enqueue(VEGASortedLineList* vsll)
	{
		vsll->next = first;
		first = vsll;

		if (last == NULL)
			last = vsll;
	}

	BOOL isEmpty() const { return first == NULL; }

	VEGASortedLineList* first;
	VEGASortedLineList* last;
};

struct VEGACoverageData
{
	int coverage; // How many of the scanlines 'subpixels' are covered
	int area; // Suitably calculated area
};

VEGARasterizer::VEGARasterizer() :
	raster_spans(NULL), span_count(0),
	sortedlines(NULL), allocSortedLines(0),
	maskbuffer(NULL),
	sleepingScanlines(NULL),
	line_bucket(NULL),
	width(0), height(0),
	consumer(NULL)
{}

VEGARasterizer::~VEGARasterizer()
{
	OP_DELETEA(maskbuffer);
	OP_DELETEA(sortedlines);
	OP_DELETEA(sleepingScanlines);
	if (line_bucket)
		OP_DELETEA((line_bucket-1));
	OP_DELETEA(raster_spans);
}

OP_STATUS VEGARasterizer::initialize(unsigned int w, unsigned int h)
{
	// largest value that can be stored in intervals. VEGAIntervalList
	// stores these in UINT16s, and uses lsb to differentiate between
	// start and end.
	const unsigned int max_val = USHRT_MAX >> 1;
	if (w > max_val || h > max_val)
		return OpStatus::ERR;

	UINT8* new_maskbuffer = maskbuffer;
	VEGALineQueue* new_sleepingScanlines = sleepingScanlines;
	VEGACoverageData* new_line_bucket = line_bucket;

	// Allocate the stuff needed for software
	if (!raster_spans)
	{
		raster_spans = OP_NEWA(VEGASpanInfo, MAX_RASTER_SPANS);
		if (!raster_spans)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (!maskbuffer || w != width)
	{
		new_maskbuffer = OP_NEWA_WH(UINT8, w, 2);
		if (!new_maskbuffer)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memset(new_maskbuffer, 0, w*sizeof(UINT8));
	}
	if (maskbuffer != new_maskbuffer)
	{
		OP_DELETEA(maskbuffer);
		maskbuffer = new_maskbuffer;
		maskscratch = maskbuffer + w;
	}

	if (!sleepingScanlines || !line_bucket || h != height || w != width)
	{
		/* Add a slop entry at both ends of the array, so as
		   to account for potential overruns due to rounding
		   errors when rasterizing. */
		new_line_bucket = OP_NEWA(VEGACoverageData, w+2);
		new_sleepingScanlines = OP_NEWA(VEGALineQueue, h);
		if (!new_sleepingScanlines || !new_line_bucket)
		{
			OP_DELETEA(new_line_bucket);
			OP_DELETEA(new_sleepingScanlines);
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memset(new_sleepingScanlines, 0, sizeof(VEGALineQueue)*h);
		op_memset(new_line_bucket, 0, sizeof(VEGACoverageData)*(w+2));
	}
	if (new_line_bucket != line_bucket)
	{
		if (line_bucket)
			OP_DELETEA((line_bucket-1));
		line_bucket = new_line_bucket+1;
	}
	if (new_sleepingScanlines != sleepingScanlines)
	{
		OP_DELETEA(sleepingScanlines);
		sleepingScanlines = new_sleepingScanlines;
	}

	width = w;
	height = h;
	return OpStatus::OK;
}

void VEGARasterizer::setQuality(unsigned int q)
{
	if (q >= ARRAY_SIZE(VegaSampleSize))
		q = ARRAY_SIZE(VegaSampleSize) - 1;

	quality = q;
}

void VEGARasterizer::rasterRect(unsigned x, unsigned y, unsigned w, unsigned h)
{
	VEGASpanInfo span;
	span.scanline = y;
	span.pos = x;
	span.length = w;
	span.mask = NULL;

	for (unsigned yp = 0; yp < h; ++yp)
	{
		emitSpan(span);

		span.scanline++;
	}

	flushSpans();
}

void VEGARasterizer::rasterRectMask(unsigned x, unsigned y, unsigned w, unsigned h,
											  const UINT8* mask, unsigned maskStride)
{
	VEGASpanInfo span;
	span.scanline = y;
	span.pos = x;
	span.length = w;
	span.mask = mask;

	for (unsigned yp = 0; yp < h; ++yp)
	{
		emitSpan(span);

		span.scanline++;
		span.mask += maskStride;
	}

	flushSpans();
}

struct VEGAIntervalList
{
	VEGAIntervalList() { reset(); }

	void reset()
	{
		num_entries = 1;
		entries[0].value = 0;
		entries[0].next = 0;
	}
	bool isEmpty() const { return num_entries == 1; }

	void addOne(unsigned cell, unsigned cursor, unsigned val)
	{
		while (entries[cursor].next && entries[entries[cursor].next].value < val)
			cursor = entries[cursor].next;

		entries[cell].next = entries[cursor].next;
		entries[cell].value = (UINT16)val;
		entries[cursor].next = (UINT16)cell;
	}
	void coalesce();
	bool isCoalesced() const { return num_entries > ARRAY_SIZE(entries); }

	void add(unsigned start, unsigned end);

	struct Entry
	{
		UINT16 value;
		UINT16 next;
	};
	Entry entries[1+32];
	unsigned num_entries;
};

void VEGAIntervalList::coalesce()
{
	UINT16 min_v = entries[entries[0].next].value;
	UINT16 max_v = entries[entries[0].value].value;

// 	entries[0].value = 2;
	entries[0].next = 1;
	entries[1].value = min_v;
	entries[1].next = 2;
	entries[2].value = max_v;
	entries[2].next = 0;

	num_entries++;
}

void VEGAIntervalList::add(unsigned start, unsigned end)
{
	OP_ASSERT(start <= end);

	start <<= 1;
	end = (end << 1) + 1;

	OP_ASSERT(start <= USHRT_MAX);
	OP_ASSERT(end   <= USHRT_MAX);

	if (num_entries >= ARRAY_SIZE(entries))
	{
		if (num_entries == ARRAY_SIZE(entries))
			// Ran out of space, coalesce and switch to union
			coalesce();

		if (start < entries[1].value)
			entries[1].value = start;
		if (end > entries[2].value)
			entries[2].value = end;

		return;
	}

	unsigned start_e = num_entries++;
	unsigned end_e = num_entries++;
	addOne(start_e, 0, start);
	addOne(end_e, start_e, end);

	if (entries[end_e].next == 0)
		entries[0].value = (UINT16)end_e;
}

/* Helper function to find the x point for a given y value and a line. */
static inline VEGA_FIX findXPointOnLine(VEGA_FIX *line, VEGA_FIX fix_y, VEGA_FIX slope)
{
	if (slope >= VEGA_INFINITY)
		return VEGA_FIXDIV2(line[VEGALINE_STARTX]+line[VEGALINE_ENDX]);

	return line[VEGALINE_STARTX] + VEGA_FIXMUL(fix_y-line[VEGALINE_STARTY],slope);
}

OP_STATUS VEGARasterizer::rasterize(const VEGAPath* path)
{
	// The number of lines might change when closing
	unsigned int numLines = path->getNumLines();

	maskbuffer_ptr = maskbuffer;

	if (allocSortedLines < numLines || !sortedlines)
	{
		OP_DELETEA(sortedlines);
		sortedlines = OP_NEWA(VEGASortedLineList, numLines);
		if (!sortedlines)
			return OpStatus::ERR_NO_MEMORY;
		allocSortedLines = numLines;
	}

	const int q_size = VegaSampleSize[quality];
	const int q_size_mask = q_size - 1;
	const unsigned q_size_shift = VegaSampleSizeShift[quality];

	// Set up the clipping values for the y axis
	unsigned int first_visible_sample_y = cliprect_sy << q_size_shift;
	unsigned int last_visible_sample_y = (cliprect_ey << q_size_shift) - 1;

	unsigned int first_active_y = cliprect_ey - 1;
	unsigned int last_active_y = cliprect_sy;

	VEGA_FIX centery_inc = VEGA_INTTOFIX(1) / q_size;
	VEGA_FIX centery_ofs = VEGA_FIXDIV2(centery_inc);
	VEGASortedLineList* line = sortedlines;

	// Sort the lines according to when they should be added (bucket sort)
	for (unsigned int cl = 0; cl < numLines; ++cl)
	{
		// Find all sub paths and at the same time find all top vertices
		VEGA_FIX* lineData = path->getNonWarpLine(cl);
		if (lineData && lineData[VEGALINE_STARTY] != lineData[VEGALINE_ENDY])
		{
			int sample = VEGA_FIXTOINT(lineData[VEGALINE_STARTY] * q_size);
			int lastSample = VEGA_FIXTOINT(lineData[VEGALINE_ENDY] * q_size);

			line->isUp = 0;

			if (lineData[VEGALINE_STARTY] > lineData[VEGALINE_ENDY])
			{
				int tmp = sample;
				sample = lastSample;
				lastSample = tmp;

				line->isUp = 1;
			}

			lastSample -= 1;

			if (sample < (int)first_visible_sample_y)
				sample = first_visible_sample_y;

			if ((unsigned int)sample <= last_visible_sample_y && lastSample >= sample)
			{
				line->currSample = sample;
				line->lastSample = lastSample;

				VEGA_FIX c = centery_inc * sample + centery_ofs;
				VEGA_FIX fix_dy = lineData[VEGALINE_ENDY] - lineData[VEGALINE_STARTY];
				VEGA_FIX fix_dx = lineData[VEGALINE_ENDX] - lineData[VEGALINE_STARTX];
				VEGA_FIX fix_slope = VEGA_FIXDIV(fix_dx, fix_dy);
				VEGA_FIX fix_xpos = findXPointOnLine(lineData, c, fix_slope) * q_size;

				line->xpos = VEGA_FIXTOSCALEDINT(fix_xpos, VEGA_SUBPIX_PREC);
				line->slope = VEGA_FIXTOSCALEDINT(fix_slope, VEGA_SUBPIX_PREC);

				sample >>= q_size_shift;
				lastSample >>= q_size_shift;

				if ((unsigned int)sample < first_active_y)
					first_active_y = sample;
				if ((unsigned int)lastSample > last_active_y)
					last_active_y = lastSample;

				// add to sleeping lines
				sleepingScanlines[sample].enqueue(line);

				++line;
			}
		}
	}

	if (last_active_y >= (unsigned)cliprect_ey)
		last_active_y = cliprect_ey-1;

	VEGALineQueue active_lines;
	active_lines.first = NULL;
	active_lines.last = NULL;

	VEGAIntervalList ilist;
	for (unsigned int sl = first_active_y; sl <= last_active_y; ++sl)
	{
		unsigned int sample = sl << q_size_shift;

		// Add the sleeping lines
		if (!sleepingScanlines[sl].isEmpty())
		{
			active_lines.append(sleepingScanlines[sl]);
			sleepingScanlines[sl].first = sleepingScanlines[sl].last = NULL;
		}

		if (active_lines.isEmpty())
			continue;

		// Add the active lines to the correct start pixels for this scanline
		VEGASortedLineList dummy;
		VEGASortedLineList* prev_line = &dummy;
		VEGASortedLineList* curr_line = dummy.next = active_lines.first;

		while (curr_line)
		{
			if (sample > curr_line->lastSample)
			{
				// Remove the lines which ended before the first subpixel of this scanline
				prev_line->next = curr_line->next;
				if (active_lines.last == curr_line)
					active_lines.last = prev_line;
				curr_line = curr_line->next;
				continue;
			}

			int next_line_sample = MIN((int)curr_line->lastSample + 1, (int)sample + q_size);

			int line_sample_height = next_line_sample - curr_line->currSample;
			curr_line->currSample = next_line_sample;

			VEGA_SUBPIX xpos_curr = curr_line->xpos;
			curr_line->xpos += curr_line->slope * line_sample_height;

			// Assume line goes left to right
			VEGA_SUBPIX xpos_min = xpos_curr;
			VEGA_SUBPIX xpos_max = curr_line->xpos - curr_line->slope;

			if (curr_line->slope < 0)
			{
				// Assumption failed - Line goes right to left
				VEGA_SUBPIX tmp = xpos_min;
				xpos_min = xpos_max;
				xpos_max = tmp;
			}

			// Round the first sample down to make sure the line is not added too late
			// (first sample might be bottom one, so the floating point precision is
			// important in such cases)
			int first_xsample = VEGA_FLOORSUBPIX(xpos_min);
			int first_p_xsample = first_xsample >> q_size_shift;
			int last_p_xsample;

			if (first_p_xsample >= cliprect_ex)
			{
				// first pixel >= cliprect end x => no coverage
				first_p_xsample = cliprect_ex - 1;
				last_p_xsample = cliprect_ex - 1;
			}
			else
			{
				int last_xsample = VEGA_ROUNDSUBPIX(xpos_max);
				last_p_xsample = last_xsample >> q_size_shift;

				int sublines_covered = line_sample_height;

				if (last_p_xsample < cliprect_sx)
				{
					// last pixel < cliprect start x
					// => coverage[cliprect_sx] += sublines_covered
					line_bucket[cliprect_sx].coverage += curr_line->isUp ? sublines_covered : -sublines_covered;

					first_p_xsample = cliprect_sx;
					last_p_xsample = cliprect_sx;
				}
				else if (first_p_xsample == last_p_xsample)
				{
					// The line stays within a single pixel
					int f_frac = first_xsample & q_size_mask;
					int l_frac = last_xsample & q_size_mask;

					int area = sublines_covered * (f_frac + l_frac) >> 1;

					// Making the assumption that if both samples fall
					// within the same pixel, that pixel cannot be clipped
					// due to the tests above (assert below to prove me
					// wrong =))
					OP_ASSERT(first_p_xsample >= cliprect_sx && first_p_xsample < cliprect_ex);
					line_bucket[first_p_xsample].coverage += curr_line->isUp ? sublines_covered : -sublines_covered;
					line_bucket[first_p_xsample].area += curr_line->isUp ? area : -area;
				}
				// The line segment either intersects the clipping rect, or is contained by it
				else
				{
					VEGA_SUBPIX xpos = xpos_curr;
					int line_coverage = curr_line->isUp ? 1 : -1;
					int sign = line_coverage >> 31;

					if (first_p_xsample < cliprect_sx || last_p_xsample >= cliprect_ex)
					{
						// Intersects clipping rect
						if (first_p_xsample < cliprect_sx)
							first_p_xsample = cliprect_sx;
						if (last_p_xsample >= cliprect_ex)
							last_p_xsample = cliprect_ex-1;

						while (sublines_covered-- > 0)
						{
							int xsamp = VEGA_ROUNDSUBPIX(xpos);
							int xpsamp = xsamp >> q_size_shift;

							if (xpsamp < cliprect_sx)
							{
								line_bucket[cliprect_sx].coverage += line_coverage;
							}
							else if (xpsamp < cliprect_ex)
							{
								int xfsamp = xsamp & q_size_mask;

								// Transfer sign
								xfsamp = (xfsamp ^ sign) - sign;

								OP_ASSERT(xpsamp >= cliprect_sx);
								line_bucket[xpsamp].coverage += line_coverage;
								line_bucket[xpsamp].area += xfsamp;
							}

							xpos += curr_line->slope;
						}
					}
					else
					{
						// Contained by clipping rect

						while (sublines_covered-- > 0)
						{
							int xsamp = VEGA_ROUNDSUBPIX(xpos);
							int xpsamp = xsamp >> q_size_shift;
							int xfsamp = xsamp & q_size_mask;

							OP_ASSERT(xpsamp >= cliprect_sx && xpsamp < cliprect_ex);

							// Transfer sign
							xfsamp = (xfsamp ^ sign) - sign;

							line_bucket[xpsamp].coverage += line_coverage;
							line_bucket[xpsamp].area += xfsamp;

							xpos += curr_line->slope;
						}
					}
				}
			}

			ilist.add(first_p_xsample, last_p_xsample);

			prev_line = curr_line;
			curr_line = curr_line->next;
		}

		active_lines.first = dummy.next;
		if (dummy.next == NULL)
			active_lines.last = NULL;

		if (!ilist.isEmpty())
			addSpans(sl, ilist);

		ilist.reset();
	}

	flushSpans();

	return OpStatus::OK;
}

unsigned VEGARasterizer::calculateArea(VEGA_FIX minx, VEGA_FIX miny, VEGA_FIX maxx, VEGA_FIX maxy)
{
	minx *= VegaSampleSize[quality];
	maxx *= VegaSampleSize[quality];

	int minx_samp = VEGA_FIXTOINT(VEGA_FLOOR(minx));
	int maxx_samp = VEGA_FIXTOINT(maxx);

	miny *= VegaSampleSize[quality];
	maxy *= VegaSampleSize[quality];

	int miny_samp = VEGA_FIXTOINT(miny);
	int maxy_samp = VEGA_FIXTOINT(maxy);

	int area = (maxx_samp - minx_samp) * (maxy_samp - miny_samp);

	area <<= VegaAlphaShift[quality];

	return area > 255 ? 255 : area;
}

static inline unsigned ScanForChange(VEGACoverageData* cd, unsigned max_len)
{
	VEGACoverageData* start_cd = cd;

	while (max_len > 0 && cd->coverage == 0 && cd->area == 0)
		cd++, max_len--;

	return cd - start_cd;
}

inline void VEGARasterizer::emitSpan(const VEGASpanInfo& span)
{
	raster_spans[span_count++] = span;

	if (span_count == MAX_RASTER_SPANS)
		flushSpans();
}

void VEGARasterizer::emitSpanAndFlush(VEGASpanInfo& span)
{
	if (span.length)
		raster_spans[span_count++] = span;

	flushSpans();

	span.pos = span.pos + span.length;
	span.mask = maskbuffer_ptr;
	span.length = 0;
}

static inline int ApplyFillRule(int c, bool xorFill)
{
	if (c < 0)
		c = -c;

	if (xorFill)
	{
		c = c & 0x1ff;
		if (c > 0x100)
			c = 0x1ff - c;
		else if (c > 0xff)
			c = 0xff;
	}
	else
	{
		if (c > 0xff)
			c = 0xff;
	}
	return c;
}

void VEGARasterizer::scanForSpans(unsigned line, int start, unsigned int length)
{
	VEGASpanInfo span;
	span.scanline = line;
	span.pos = start;
	span.length = 0;
	span.mask = maskbuffer_ptr;

	int acc_cov = 0;
	VEGACoverageData* cd = line_bucket + span.pos;

	const unsigned q_size_shift = VegaSampleSizeShift[quality];
	const unsigned q_alpha_shift = VegaAlphaShift[quality];

	while (length > 0)
	{
		acc_cov += cd->coverage;

		int c = (acc_cov << q_size_shift) - cd->area;
		c <<= q_alpha_shift;

		c = ApplyFillRule(c, xorFill);

		// Reset and consume this coverage data element
		cd->coverage = cd->area = 0;

		cd++;
		length--;

		unsigned len = ScanForChange(cd, length);

		if (span.length == 0 && c == 0 && acc_cov == 0)
		{
			// Trailing or leading transparency - skip
			span.pos += len + 1;

			cd += len;
			length -= len;
			continue;
		}

		if (len == 0)
		{
			if (remainingMaskBuffer() == 0)
				emitSpanAndFlush(span);

			*maskbuffer_ptr++ = (UINT8)c;
			span.length++;
			continue;
		}

		int next_c = (acc_cov << q_size_shift);
		next_c <<= q_alpha_shift;

		next_c = ApplyFillRule(next_c, xorFill);

		cd += len;
		length -= len;

		if (next_c != c)
		{
			if (remainingMaskBuffer() == 0)
				emitSpanAndFlush(span);

			*maskbuffer_ptr++ = (UINT8)c;
			span.length++;

			c = next_c;
		}
		else
		{
			len++; // Include the value that we started with
		}

		if (len < MIN_RASTER_SPAN_LENGTH || !(c == 0x00 || c == 0xff))
		{
			// We have a span that is deemed too short to warrant its
			// own span, or a continuous span that can not be
			// represented in the current model (not transparent or
			// opaque).

			// Will the span fit in what remains of the mask-buffer?
			unsigned remaining_mb = remainingMaskBuffer();
			if (remaining_mb < len)
			{
				// The span has to be split...
				unsigned int cnt = remaining_mb;
				while (cnt-- > 0)
					*maskbuffer_ptr++ = (UINT8)c;

				span.length += remaining_mb;

				emitSpanAndFlush(span);

				len -= remaining_mb;
			}

			unsigned int cnt = len;
			while (cnt-- > 0)
				*maskbuffer_ptr++ = (UINT8)c;

			span.length += len;
		}
		else
		{
			// We have a continuous span that is long enough to
			// warrant its own span

			// First emit what we currently have
			if (span.length)
			{
				emitSpan(span);

				// Advance to start of continuous span
				span.pos += span.length;
			}

			// If opaque, generate an opaque span of length <len>,
			// else just advance ('skip') <len>
			// (non-zero c implies opaque, see above)
			if (c)
			{
				// Setup opaque span
				span.mask = NULL;
				span.length = len;

				// len always > 0 here, so no need for empty-check
				emitSpan(span);
			}

			// Advance past the continuous span
			span.pos += len;

			// Reset mask pointer and length for next span
			span.mask = maskbuffer_ptr;
			span.length = 0;
		}
	}

	if (span.length)
		emitSpan(span);
}

struct VEGASpanState
{
	VEGASpanInfo span;
	int coverage;
};

inline void VEGARasterizer::emitMonotoneSpan(VEGASpanState& state)
{
	int c = state.coverage << VegaSampleSizeShift[quality];
	c <<= VegaAlphaShift[quality];

	c = ApplyFillRule(c, xorFill);
	if (c == 0)
		return;

	if (c == 0xff)
	{
		state.span.mask = NULL;
	}
	else
	{
		unsigned remaining_mb = remainingMaskBuffer();
		if (remaining_mb < state.span.length)
			flushSpans();

		state.span.mask = maskbuffer_ptr;

		unsigned int cnt = state.span.length;
		while (cnt-- > 0)
			*maskbuffer_ptr++ = (UINT8)c;
	}

	emitSpan(state.span);
}

inline void VEGARasterizer::emitMaskSpan(VEGASpanState& state)
{
	unsigned remaining_mb = remainingMaskBuffer();
	if (remaining_mb < state.span.length)
		flushSpans();

	state.span.mask = maskbuffer_ptr;

	const unsigned q_size_shift = VegaSampleSizeShift[quality];
	const unsigned q_alpha_shift = VegaAlphaShift[quality];

	// Generate maskbuffer content
	VEGACoverageData* cd = line_bucket + state.span.pos;
	unsigned cnt = state.span.length;
	if (xorFill)
	{
		while (cnt-- > 0)
		{
			state.coverage += cd->coverage;

			int c = (state.coverage << q_size_shift) - cd->area;
			c <<= q_alpha_shift;

			if (c < 0)
				c = -c;

			c = c & 0x1ff;
			if (c > 0x100)
				c = 0x1ff - c;
			else if (c > 0xff)
				c = 0xff;

			*maskbuffer_ptr++ = (UINT8)c;

			cd->coverage = cd->area = 0;
			cd++;
		}
	}
	else
	{
		while (cnt-- > 0)
		{
			state.coverage += cd->coverage;

			int c = (state.coverage << q_size_shift) - cd->area;
			c <<= q_alpha_shift;

			if (c < 0)
				c = -c;

			if (c > 0xff)
				c = 0xff;

			*maskbuffer_ptr++ = (UINT8)c;

			cd->coverage = cd->area = 0;
			cd++;
		}
	}

	emitSpan(state.span);
}

void VEGARasterizer::addSpans(unsigned int line, const VEGAIntervalList& ilist)
{
	if (ilist.isCoalesced())
	{
		int start = ilist.entries[1].value >> 1;
		unsigned length = (ilist.entries[2].value >> 1) + 1 - start;

		if (length < MIN_RASTER_SPAN_LENGTH)
		{
			VEGASpanState state;
			state.coverage = 0;
			state.span.scanline = line;
			state.span.pos = start;
			state.span.length = length;

			emitMaskSpan(state);
			return;
		}
		else
		{
			scanForSpans(line, start, length);
			return;
		}
	}

	VEGASpanState state;
	state.coverage = 0;
	state.span.scanline = line;

	unsigned cursor = ilist.entries[0].next;
	unsigned start = ilist.entries[cursor].value >> 1;

	while (true)
	{
		cursor = ilist.entries[cursor].next;

		// Find next interval end
		int cnt = 1;
		while (true)
		{
			unsigned int v = ilist.entries[cursor].value;

			cnt += (v & 1) != 0 ? -1 : 1;
			if (cnt == 0)
				break;

			cursor = ilist.entries[cursor].next;
		}

		unsigned end = (ilist.entries[cursor].value >> 1) + 1;

		// Span from start -> end
		state.span.pos = start;
		state.span.length = end - start;

		cursor = ilist.entries[cursor].next;
		if (!cursor)
		{
			// Nothing to attempt to merge with
			if (state.span.length > 1 ||
				line_bucket[state.span.pos].area ||
				state.coverage + line_bucket[state.span.pos].coverage)
			{
				// Not transparent - emit
				emitMaskSpan(state);
			}
			line_bucket[state.span.pos].coverage = 0;
			break;
		}

		start = ilist.entries[cursor].value >> 1;

		// Span from end -> start (iff start > end)
		if (start > end)
		{
			// Can the current (masked) span be merged with the following monotone span?
			if (state.span.length > 1 ||
				line_bucket[state.span.pos].area)
			{
				emitMaskSpan(state);
			}
			else
			{
				state.coverage += line_bucket[state.span.pos].coverage;
				line_bucket[state.span.pos].coverage = 0;

				end--;
			}

			state.span.pos = end;
			state.span.length = start - end;

			emitMonotoneSpan(state);
		}
		else
		{
			// The next interval is adjacent to the current - merge
			start = state.span.pos;
		}
	}
}

void VEGARasterizer::flushSpans()
{
	if (span_count == 0)
	{
		// Span state should be reset after flushing
		OP_ASSERT(maskbuffer_ptr == maskbuffer);
		return;
	}

	consumer->drawSpans(raster_spans, span_count);

	// Reset span state
	span_count = 0;
	maskbuffer_ptr = maskbuffer;
}

#endif // VEGA_SUPPORT
