/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/spatial_navigation/sn_algorithm.h"
#include "modules/spatial_navigation/sn_util.h"

#ifdef SPATIAL_NAVIGATION_ALGORITHM

FourwayAlgorithm::FourwayAlgorithm() : bestRank(SN_START_DISTANCE), bestAlternative(SN_START_DISTANCE), direction(0)
{
}

OP_STATUS
FourwayAlgorithm::Init(const RECT& previousRect, INT32 direction, BOOL hasActiveLink, BOOL activeLinkHasLineBreak)
{
	this->activeLinkHasLineBreak = activeLinkHasLineBreak;
	this->direction = direction;
	this->hasActiveLink = hasActiveLink;
	transposed_previous_rect.Set(previousRect.left,
								 previousRect.top,
								 previousRect.right - previousRect.left,
								 previousRect.bottom - previousRect.top);
	SnUtil::Transpose(direction, transposed_previous_rect);
	return OpStatus::OK;
}

OpSnAlgorithm::EvaluationResult
FourwayAlgorithm::EvaluateLink(const RECT& linkRect)
{
	// Transpose to direction == DIR_DOWN
	OpRect transposed_link_rect(linkRect.left, linkRect.top, linkRect.right - linkRect.left, linkRect.bottom - linkRect.top);
	SnUtil::Transpose(direction, transposed_link_rect);

	OpSnAlgorithm::EvaluationResult result = NOT_IN_SECTOR;
	
	if (LinkInSearchSector(transposed_previous_rect, transposed_link_rect))
	{
		currentRank = LinkValue(transposed_previous_rect, transposed_link_rect);
		if (currentRank < bestRank - GetFuzzyMargin())
		{
			bestRank = currentRank;
			result = BEST_ELEMENT;
		}
		else if (currentRank < bestRank)
		{
			bestRank = currentRank;
			result = BEST_ELEMENT_BUT_CLOSE;
		}
		else if (currentRank < (bestRank.rank < 0 ? 0 : bestRank) + GetFuzzyMargin())
			result = NOT_BEST_BUT_CLOSE;
		else
			result = NOT_BEST;
	}
	else if (LinkInAltSearchSector(transposed_previous_rect, transposed_link_rect))
	{
		currentRank = AltLinkValue(transposed_previous_rect, transposed_link_rect);
		if (currentRank < bestAlternative)
		{
			bestAlternative = currentRank;
			result = ALTERNATIVE_ELEMENT;
		}
	}

	return result;
}

void
FourwayAlgorithm::ForceBestRectValue(const RECT& linkRect)
{
	// Transpose to direction == DIR_DOWN
	OpRect transposed_link_rect(linkRect.left, linkRect.top, linkRect.right - linkRect.left, linkRect.bottom - linkRect.top);
	
	SnUtil::Transpose(direction, transposed_link_rect);
	bestRank = LinkValue(transposed_previous_rect, transposed_link_rect);
}

BOOL
FourwayAlgorithm::LinkInSearchSector(const OpRect& r0, const OpRect& r)
{
	if (hasActiveLink && r.y < r0.y)
		return FALSE;

	// Do different things depending on if we have overlap.
	if (SnUtil::Intersect(r0.x,r0.x+r0.width,r.x,r.x+r.width) &&
		SnUtil::Intersect(r0.y,r0.y+r0.height,r.y,r.y+r.height))
	{
		if (r.y == r0.y && !SnUtil::IsInView(r.x,r.y,r.width,r.height,r0.x,r0.y,r0.width,r0.height))
			return FALSE;

		// Elements too far out to the sides are not in sector even if
		// we have overlap.
		INT32 mid_x = r.x + r.width/2;
		
		if ((r.x > r0.x || r.x+r.width < r0.x+r0.width) && 
			(mid_x < r0.x || mid_x > r0.x + r0.width))
			return FALSE;
	}
	else
	{
		if (r.y < r0.y+r0.height)
			return FALSE;

		INT32 xDistance;
		INT32 yDistance = r.y-(r0.y+r0.height);

		if (r.x+r.width <= r0.x)	// Link is to the left
			xDistance = r0.x-(r.x+r.width);
		else if (r.x >= r0.x+r0.width) // Link is to the right
			xDistance = r.x-(r0.x+r0.width);
		else
			return TRUE;

		if (direction == DIR_UP || direction == DIR_DOWN)
		{
			if (2*yDistance < xDistance)
				return FALSE;
		}
		else // DIR_LEFT || DIR_RIGHT
		{
			// Discriminate the corner case
			yDistance--;
			if (yDistance < 2*xDistance)
				return FALSE;
		}
	}
	return TRUE;
}

BOOL
FourwayAlgorithm::LinkInAltSearchSector(const OpRect& r0, const OpRect& r)
{
	if (r.y > r0.y && r.y+r.height > r0.y+r0.height)
		return TRUE;
	else
		return FALSE;
}

FourwayAlgorithm::Rank
FourwayAlgorithm::LinkValue(const OpRect& r0, const OpRect& r)
{
	OP_ASSERT(r0.width > 0);
	
	INT32 mid_x0 = r0.x + r0.width/2;

	INT32 yDistance = r.y  - (r0.y+r0.height);
	INT32 xDistance = 0;

	if (r.x+r.width < mid_x0)	// Link on left side.
	{
		xDistance = mid_x0 - (r.x+r.width);
	}
	else if (r.x > mid_x0) // Link on right side.
	{
		xDistance = r.x - mid_x0;
	}

	INT32 rank;
	INT32 sub_rank;


	if (yDistance == 0)
	{
		rank = 0;
		sub_rank = xDistance;
	}
	else if (yDistance < 0)
	{
		rank = yDistance;
		sub_rank = xDistance;
	}
	else  // yDistance > 0
	{
		sub_rank = INT32_MAX;

		if (direction == DIR_RIGHT || direction == DIR_LEFT)
		{
			rank = SnUtil::sqr(SN_ELLIPSE_COEF * xDistance) + SnUtil::sqr(yDistance);
			if (rank < SnUtil::sqr(r0.width*SN_ELLIPSE_COEF/2)) 
			{
				// Special handling of elements below and close the
				// current element.  Calculate the coefficient of the
				// ellipse that goes through the corner (xDistance,
				// yDistance) and use that coefficent to calculate the
				// rank. This gives a smooth transition.
				double yp = op_sqrt(SnUtil::sqr(r0.width)/4.0 - SnUtil::sqr(xDistance));
				double r = yDistance / yp;
				OP_ASSERT(r <= SN_ELLIPSE_COEF);
				rank = SnUtil::sqr(static_cast<INT32>(r * xDistance)) + SnUtil::sqr(yDistance);
				OP_ASSERT(rank < SnUtil::sqr(r0.width*SN_ELLIPSE_COEF/2));
				sub_rank = xDistance;
			}
		}
		else // DIR_UP || DIR_DOWN
		{
			rank = SnUtil::sqr(xDistance) + SnUtil::sqr(SN_ELLIPSE_COEF * yDistance);
			if (rank < SnUtil::sqr(r0.width/2))
			{
				// Special handling of elements below and close the
				// current element.  Calculate the coefficient of the
				// ellipse that goes through the corner (xDistance,
				// yDistance) and use that coefficent to calculate the
				// rank. This gives a smooth transition.
				double yp = op_sqrt(SnUtil::sqr(r0.width)/4.0 - SnUtil::sqr(xDistance));
				double radius = yDistance / yp;
				OP_ASSERT(radius < 1.0/SN_ELLIPSE_COEF);
				rank = SnUtil::sqr(static_cast<INT32>(radius * xDistance)) + SnUtil::sqr(SN_ELLIPSE_COEF * yDistance);
				OP_ASSERT(rank < SnUtil::sqr(r0.width/2));
				sub_rank = xDistance;
			}
		}
	}

	return FourwayAlgorithm::Rank(rank, sub_rank);
}

FourwayAlgorithm::Rank
FourwayAlgorithm::AltLinkValue(const OpRect& r0, const OpRect& r)
{
	INT32 yDistance = r.y + r.height - r0.y;
	INT32 xDistance = 0;

	if (r.x+r.width <= r0.x)	// Link is to the left
	{
		xDistance = r0.x - (r.x+r.width);
	}
	else if (r.x >= r0.x+r0.width) // Link is to the right
	{
		xDistance = r.x - (r0.x+r0.width);
	}

	INT32 sub_rank = INT32_MAX;
	
	const INT32 multiplier = 10000;

	if (yDistance > 0)
		sub_rank = xDistance * multiplier / yDistance;
	
	INT32 rank = 1;
	
	if (sub_rank < multiplier)
		rank = 0;

	return FourwayAlgorithm::Rank(rank, sub_rank);
}

#ifndef USE_4WAY_NAVIGATION
// Twoway

TwowayAlgorithm::TwowayAlgorithm() : bestX(0), bestY(0)
{
}

OP_STATUS
TwowayAlgorithm::Init(const RECT& previousRect, INT32 direction, BOOL hasActiveLink, BOOL activeLinkHasLineBreak)
{
	if (direction != DIR_UP && direction != DIR_DOWN)
		return OpStatus::ERR;

	lastRect.left = previousRect.left;
	lastRect.right = previousRect.right;
	lastRect.top = previousRect.top;
	lastRect.bottom = previousRect.bottom;

	bestY = (direction == DIR_UP) ? -SN_START_DISTANCE : SN_START_DISTANCE;
	bestX = (direction == DIR_UP) ? -SN_START_DISTANCE : SN_START_DISTANCE;
	this->direction = direction;
	this->hasActiveLink = hasActiveLink;
	this->activeLinkHasLineBreak = activeLinkHasLineBreak;
	return OpStatus::OK;
}

OpSnAlgorithm::EvaluationResult
TwowayAlgorithm::EvaluateLink(const RECT& linkRect)
{
	// First check if the link is in the correct direction
	if (direction == DIR_DOWN && linkRect.top < lastRect.top ||
		direction == DIR_UP && linkRect.top > lastRect.top)
		return NOT_BEST;
	if (linkRect.top == lastRect.top)
	{
		if (direction == DIR_DOWN && linkRect.left < lastRect.left ||
			direction == DIR_UP && linkRect.left > lastRect.left)
			return NOT_BEST;
	}
	if (activeLinkHasLineBreak)
	{
		if (direction == DIR_DOWN)
		{
			if (linkRect.top < (lastRect.top+lastRect.bottom)/2)
				return NOT_BEST;
		}
		else	// DIR_UP
		{
			if (linkRect.bottom > (lastRect.top+lastRect.bottom)/2)
				return NOT_BEST;
		}
	}

	// Link is in the correct direction, now let's see if it is the best so far
	if (direction == DIR_DOWN && linkRect.top < bestY ||
		direction == DIR_UP && linkRect.top > bestY)
	{
		bestY = linkRect.top;
		bestX = linkRect.left;
		return BEST_ELEMENT;
	}
	else if (linkRect.top == bestY)
	{
		if (direction == DIR_DOWN && linkRect.left < bestX ||
			direction == DIR_UP && linkRect.left > bestX)
		{
			bestX = linkRect.left;
			return BEST_ELEMENT;
		}
	}
	return NOT_BEST;
}

void
TwowayAlgorithm::ForceBestRectValue(const RECT& linkRect)
{
	bestX = linkRect.left;
	bestY = linkRect.top;
}

#endif // !USE_4WAY_NAVIGATION
#endif // SPATIAL_NAVIGATION_ALGORITHM
