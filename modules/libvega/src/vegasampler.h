/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGASAMPLER_H
#define VEGASAMPLER_H

#include "modules/libvega/src/vegapixelformat.h"

#define VEGASamplerBilerpPM		VEGA_PIXEL_FORMAT_CLASS::SamplerBilerpPM
#define VEGASamplerLerpXPM		VEGA_PIXEL_FORMAT_CLASS::SamplerLerpXPM
#define VEGASamplerLerpYPM		VEGA_PIXEL_FORMAT_CLASS::SamplerLerpYPM
#define VEGASamplerNearestPM	VEGA_PIXEL_FORMAT_CLASS::SamplerNearestPM
#define VEGASamplerNearest		VEGA_PIXEL_FORMAT_CLASS::SamplerNearest
#define VEGASamplerNearestPM_1D	VEGA_PIXEL_FORMAT_CLASS::SamplerNearestPM_1D
#define VEGASamplerNearest_1D	VEGA_PIXEL_FORMAT_CLASS::SamplerNearest_1D

#define VEGA_INTTOSAMPLE(v)		((v) << VEGA_SAMPLER_FRACBITS)
#define VEGA_DBLTOSAMPLE(v)		((int)((v) * (1 << VEGA_SAMPLER_FRACBITS)))

#endif // VEGASAMPLER_H
