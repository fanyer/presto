/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef _SVG_MOTIONPATH_H_
#define _SVG_MOTIONPATH_H_

#ifdef SVG_SUPPORT

#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGMatrix.h"

#include "modules/libvega/vegafixpoint.h"

class VEGAPath;
class SVGVector;
class SVGKeySpline;
struct FlattenedPathIndex;

class SVGMotionPath
{
public:
	SVGMotionPath();
	~SVGMotionPath();

	struct PositionDescriptor
	{
		PositionDescriptor();

		SVGNumber where;
		SVGNumber next_where;
		SVGCalcMode calcMode;
		SVGVector* keyTimes;
		SVGVector* keyPoints;
		SVGVector* keySplines;
		SVGRotate* rotate;
	};

	/**
	 * Set which OpBpath to use.
	 */
	OP_STATUS Set(OpBpath* path, SVGNumber path_length, BOOL account_moveto = FALSE);

	/**
	 * Set additional transform.
	 */
	void SetTransform(const SVGMatrix &matrix) { m_transform = matrix; }

	SVGMatrix GetMatrix() { return m_transform; }

	OP_STATUS GetCurrentTransformationValue(PositionDescriptor& pos,
											SVGMatrix &target);

	void SetFlatness(SVGNumber flatness) { m_flatness = flatness; }

	static SVGNumber CalculateKeySpline(SVGKeySpline* keyspline, SVGNumber flatness, SVGNumber q1);

	/**
	 * Get the path segment at a certain length
	 */
	unsigned int GetSegmentIndexAtLength(SVGNumber len);

	/**
	 * Distance is in pathlength coordinates
	 */
	void CalculateTransformAtDistance(SVGNumber distance, SVGRotate* rotate, SVGMatrix& target);
	void CalculateTransformAtDistance(SVGNumber distance, SVGMatrix& target)
	{
		SVGRotate rot(SVGROTATE_AUTO);
		CalculateTransformAtDistance(distance, &rot, target);
	}

	SVGNumber CalculateRotationAtDistance(SVGNumber distance);

	SVGNumber GetLength() { return m_vega_path_length; }

#ifdef SVG_FULL_11
	OP_STATUS Warp(const OpBpath* inpath, OpBpath** outpath,
				   SVGNumber startDist, SVGNumber expansionFactor, SVGNumber half_width);
#endif // SVG_FULL_11

	BOOL HasPath(SVGObject* obj) { return m_path == obj; }
	BOOL HasPathLength(SVGNumber path_length) { return m_path_length == path_length; }

private:
	OpBpath* m_path;
	SVGMatrix m_transform;

	VEGAPath* m_vega_path;
	SVGNumber m_vega_path_length;
	unsigned int m_vega_line_segments;
	unsigned int* m_vega_segment_indexes;
	SVGNumber* m_vega_segment_lengths;
	SVGNumber m_flatness;
	SVGNumber m_path_length;

	void Reset();

	/**
	 * Construct the VEGA path
	 */
	VEGAPath* ConstructVegaPath(BOOL account_moveto);

	/**
	 * Get the length of the path
	 */
	static SVGNumber GetSubLength(VEGAPath* path,
								  unsigned int start_idx, unsigned int end_idx);

	/**
	 * Map distance along path to index in flattened path
	 */
	BOOL DistanceToLineIndex(SVGNumber distance, FlattenedPathIndex& fpi);

#ifdef SVG_FULL_11
	/**
	 * Calculate factors used in path/glyph warping
	 */
	void CalculateWarpFactors(SVGNumber midpoint, SVGNumber half_width,
							  SVGNumber& left_sf, SVGNumber& right_sf);
#endif // SVG_FULL_11

	/**
	 * Calculate how far along the path we have come
	 */
	OP_STATUS CalculateCurrentDistanceAlongPath(PositionDescriptor& pos, SVGNumber& distance);

	/**
	 * Get command count
	 */
	SVGNumber GetSegmentLength(unsigned int idx);

	/**
	 * Get the accumulated length up to an interval
	 */
	SVGNumber GetAccumulatedSegmentLength(unsigned int idx);

	/**
	 * Calculate position according to key times
	 */
	SVGNumber CalculateKeyTimes(PositionDescriptor& pos);

	/**
	 * Calculate position according to key splines
	 */
	SVGNumber CalculateKeySplines(PositionDescriptor& pos, unsigned int whichInterval, SVGNumber& q1);
};

#endif // SVG_SUPPORT && !SVG_USE_DEFAULT_MOTION_PATH
#endif // _SVG_MOTIONPATH_H_

