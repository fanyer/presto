/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_PATH_H
#define SVG_PATH_H

class SVGNumber;

class SVGPath
{
public:
	virtual ~SVGPath() {}

	/**
	* Move to the specified coordinate. Doing this will create a new path segment and the old segment (if any) will be left open.
	* If you want the old path segment to be closed, then you need to call SVGPath::Close() before doing the moveto.
	*
	* @param x The x position
	* @param y The y position
	* @param relative TRUE if the given (x,y) are relative to the current position, FALSE if the values are absolute
	*/
	virtual OP_STATUS MoveTo(SVGNumber x, SVGNumber y, BOOL relative) = 0;

	/**
	* Line to the specified coordinate.
	*
	* @param x The end x position
	* @param y The end y position
	* @param relative TRUE if the given (x,y) are relative to the current position, FALSE if the values are absolute
	*/
	virtual OP_STATUS LineTo(SVGNumber x, SVGNumber y, BOOL relative) = 0;
	
	/**
	* Horizontal straight line to the specified x coordinate.
	*
	* @param x The x position
	* @param relative TRUE if the given x is relative to the current position, FALSE if the value is absolute
	*/
	virtual OP_STATUS HLineTo(SVGNumber x, BOOL relative) = 0;
	
	/**
	* Vertical straight line to the specified y coordinate.
	*
	* @param y The y position
	* @param relative TRUE if the given y is relative to the current position, FALSE if the value is absolute
	*/
	virtual OP_STATUS VLineTo(SVGNumber y, BOOL relative) = 0;

	/**
	* Cubic Bezier curve to.
	* 
	* If you pass smooth=TRUE then the first control point is assumed to be the reflection of the second control point on the previous command 
	* relative to the current point. If there is no previous command or if the previous command was not a cubic curve, assume the first control 
	* point is coincident with the current point.
	* 
	* @param cp1x The first controlpoint x coordinate
	* @param cp1y The first controlpoint y coordinate
	* @param cp2x The second controlpoint x coordinate
	* @param cp2y The second controlpoint y coordinate
	* @param endx The end x coordinate
	* @param endy The end y coordinate
	* @param smooth If TRUE then the first controlpoint will be calculated for you, you only need to pass the second controlpoint and endpoint
	* @param relative TRUE if the given coordinates are relative to the current position, FALSE if the values are absolute
	*/
	virtual OP_STATUS CubicCurveTo(	SVGNumber cp1x, SVGNumber cp1y,
									SVGNumber cp2x, SVGNumber cp2y, 
									SVGNumber endx, SVGNumber endy, 
									BOOL smooth, BOOL relative) = 0;

	/**
	* Quadratic Bezier curve to.
	* 
	* If you pass smooth=TRUE then the control point is assumed to be the reflection of the control point on the previous command relative to 
	* the current point. If there is no previous command or if the previous command was not a quadratic curve, assume the control point is coincident
	* with the current point.
	* 
	* @param cp1x The first controlpoint x coordinate
	* @param cp1y The first controlpoint y coordinate
	* @param endx The end x coordinate
	* @param endy The end y coordinate
	* @param smooth If TRUE then the first controlpoint will be calculated for you, you only need to pass the second controlpoint and endpoint
	* @param relative TRUE if the given coordinates are relative to the current position, FALSE if the values are absolute
	*/		
	virtual OP_STATUS QuadraticCurveTo( SVGNumber cp1x, SVGNumber cp1y, 
										SVGNumber endx, SVGNumber endy, 
										BOOL smooth, BOOL relative) = 0;

	/**
	 * Arc to.
	 *
	 * Add an elliptical arc from the current point to (x, y). The
	 * size and orientation of the ellipsi are defined by two radii
	 * (rx, ry) and an x-axis rotation.
	 *
	 * @param rx
	 * @param ry
	 * @param xrot
	 * @param large
	 * @param sweep
	 * @param x
	 * @param y
	 * @param relative
	 */
	virtual OP_STATUS ArcTo(SVGNumber rx, SVGNumber ry,
							SVGNumber xrot,
							BOOL large,
							BOOL sweep,
							SVGNumber x,
							SVGNumber y, BOOL relative) = 0;

	/**
	* Close this path segment. You can still add other commands after doing this, it just means this segment is closed.
	* Paths are not automatically closed for you, the default is to create open paths.
	*
	* Also note that a new (empty) curve segment is automatically created for you.
	* @see MoveTo
	*/
	virtual OP_STATUS Close() = 0;
};

#ifdef SVG_PATH_TO_VEGAPATH
#include "modules/libvega/vegafixpoint.h"

class VEGAPath;

/**
 * Convert an SVGPath to a VEGAPath.
 *
 * @param outline The SVGPath to convert
 * @param ofs_x X-offset to apply to the output
 * @param ofs_y Y-offset to apply to the output
 * @param flatness The flatness criteria to use
 * @param voutline The VEGAPath the result is appended to
 *
 * @return ERR_NO_MEMORY on OOM, otherwise OK. If OOM the output may
 * contain partial results.
 */
extern OP_STATUS ConvertSVGPathToVEGAPath(SVGPath* outline,
										  VEGA_FIX ofs_x, VEGA_FIX ofs_y,
										  VEGA_FIX flatness,
										  VEGAPath* voutline);
#endif // SVG_PATH_TO_VEGAPATH

#endif // SVG_PATH_H
