/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2206 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_AVERAGER_H
#define SVG_AVERAGER_H

#ifdef SVG_SUPPORT

class SVGAverager
{
private:
	unsigned int* m_samples;
	unsigned int m_size;
	unsigned int m_count;
	unsigned int m_next_idx;
public:
	SVGAverager() : m_samples(NULL), m_size(0), m_count(0), m_next_idx(0) {}
	~SVGAverager() { OP_DELETEA(m_samples); }

	OP_STATUS Create(unsigned int sample_size)
	{
		m_size = sample_size;
		m_samples = OP_NEWA(unsigned int, m_size);
		if (!m_samples)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}

	void AddSample(unsigned int sample)
	{
		if (m_size == 0)
			return;
		OP_ASSERT(sample > 0 && sample <= INT_MAX);
		m_samples[m_next_idx] = sample;
		m_next_idx = (m_next_idx + 1) % m_size;
		if (m_count < m_size)
			m_count++;
	}

	unsigned int GetAverage() const
	{
		if (m_size == 0 || m_count == 0)
			return 0;

		unsigned int sum = 0;
		for (unsigned int i=0; i < m_count; i++)
		{
			sum += m_samples[i];
		}
		return sum / m_count;
	}

	unsigned int GetMedian() const
	{
		OP_ASSERT(m_count < 30); // Need a better algorithm for this
								 // size of the problem
		if (m_size == 0 || m_count == 0)
			return 0;

		unsigned int* selection_array = OP_NEWA(unsigned int, m_count);
		if (!selection_array)
		{
			return 0;
		}

		op_memcpy(selection_array, m_samples, m_count*sizeof(*selection_array));

		// This is nonlinear but simpler than most linear
		// algorithms. Basically a selection sort that only runs
		// halfway.
		unsigned int median_index = (m_count - 1) / 2;

		for (unsigned int i = 0; i <= median_index; i++)
		{
			unsigned int min_index = i;
			unsigned int min_value = selection_array[i];
			for (unsigned int j = i+1; j < m_count; j++)
			{
				if (selection_array[j] < min_value)
				{
					min_index = j;
					min_value = selection_array[j];
				}
			}

			selection_array[min_index] = selection_array[i];
			selection_array[i] = min_value;
		}

		unsigned int median = selection_array[median_index];
		OP_DELETEA(selection_array);
		return median;
	}
};
#endif // SVG_SUPPORT
#endif // SVG_AVERAGER_H
