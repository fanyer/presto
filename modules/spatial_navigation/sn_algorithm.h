/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
**
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _SN_ALGORITHM_H_
#define _SN_ALGORITHM_H_

#ifdef SPATIAL_NAVIGATION_ALGORITHM

#ifndef INT32_MAX
# define INT32_MAX 0x7fffffff
#endif

#define DIR_RIGHT 0
#define DIR_UP 90
#define DIR_LEFT 180
#define DIR_DOWN 270
#define SN_START_DISTANCE INT32_MAX

#define SN_ELLIPSE_COEF 2
///< The coefficient determining the aspect ratio of the elipse in the rank algorithm.

#define SN_FUZZY_MARGIN 1500
/**< Amount of rank that an element can be worse than the best element
 and still be the best in the end when the logical tree is taken into
 account. */

/**
 * The OpSnAlgorithm class is used for giving a numerical rating for two
 * rectangles considering their size and position relative to each other.
 * This is used to find the best element to navigate to when doing spatial
 * navigation.
 * The Algorithm itself remembers the best rectangle found so far
 * 
 * For more on the algorithm currently used see ./documentation/index.html
 */
class OpSnAlgorithm
{
public:
	enum EvaluationResult {
		START_ELEMENT,		// Just for debugging using SnTrace.
		BEST_ELEMENT,
		BEST_ELEMENT_BUT_CLOSE,
		NOT_BEST_BUT_CLOSE,
		NOT_BEST,
		ALTERNATIVE_ELEMENT,
		NOT_IN_SECTOR,		// Just for debugging using SnTrace.
	};

	virtual	~OpSnAlgorithm() {}

	/**
	 * Inits the algorithm
	 * @param previousRect is the rectangle of the element we're navigating from
	 * @param direction The direction we're navigating
	 */
	virtual OP_STATUS			Init(const RECT& previousRect, INT32 direction, BOOL hasActiveLink, BOOL activeLinkHasLineBreak) = 0;

	/**
	 * Evaluates how a rectangle compares to the starting rect (given in Init)
	 * and all other rectangles tested so far
	 * @param linkRect the rectangle to evaluate.
	 * @return TRUE if this is the best rectangle evaluated so far, FALSE otherwise
	 */
	virtual EvaluationResult	EvaluateLink(const RECT& linkRect) = 0;

	virtual INT32				GetBestRectValue() = 0;

	virtual INT32				GetCurrentRectValue() = 0;

	virtual void				ForceBestRectValue(const RECT& linkRect) = 0;

	INT32					GetFuzzyMargin() { return SN_FUZZY_MARGIN; }
};

/**
 * This algorithm can be used if spatnav uses an input device with four direction
 * of movement - i.e. one for each of the directions up, down, left and right.
 */
class FourwayAlgorithm : public OpSnAlgorithm
{
public:
	FourwayAlgorithm();
	virtual OP_STATUS			Init(const RECT& previousRect, INT32 direction, BOOL hasActiveLink, BOOL activeLinkHasLineBreak);  ///< @overload OpSnAlgorithm
	virtual EvaluationResult	EvaluateLink(const RECT& linkRect);  ///< @overload OpSnAlgorithm
	virtual INT32				GetBestRectValue() { return bestRank.rank; }
	virtual INT32				GetCurrentRectValue() { return currentRank.rank; }
	virtual void				ForceBestRectValue(const RECT& linkRect);

	enum
	{
		LINK_INSIDE_ACTIVE = -10000,
		LINK_NOT_IN_SECTOR = -10001 // Large enough negative number that will otherwise not be returned by EvaluateLink
	};

	struct Rank {
		Rank(INT32 rank = SN_START_DISTANCE, INT32 sub_rank = SN_START_DISTANCE) : rank(rank), sub_rank(sub_rank) {}

		BOOL operator<(const Rank& rhs) const { return (rank < rhs.rank || (rank == rhs.rank && sub_rank < rhs.sub_rank)); }
		BOOL operator>(const Rank& rhs) const { return (rank > rhs.rank || (rank == rhs.rank && sub_rank > rhs.sub_rank)); }
		Rank operator+(const INT32 rhs) const { return Rank(rank + rhs, sub_rank); }
		Rank operator-(const INT32 rhs) const { return Rank(rank - rhs, sub_rank); }

		INT32 rank;
		INT32 sub_rank;
	};

private:
	Rank				Evaluate(const RECT& linkRect);
	BOOL				LinkInSearchSector(const OpRect& r0, const OpRect& r);

	BOOL				LinkInAltSearchSector(const OpRect& r0, const OpRect& r);

	Rank				LinkValue(const OpRect& r0, const OpRect& r);

	Rank				AltLinkValue(const OpRect& r0, const OpRect& r);

	Rank				bestRank;
	Rank				bestAlternative;
	Rank				currentRank;
	INT32				direction;
	BOOL				hasActiveLink;
	OpRect				transposed_previous_rect;
	BOOL				activeLinkHasLineBreak;
};

/**
 * This algorithm can be used for devices which use only a two directional
 * input device - i.e. up and down.
 */
#ifndef USE_4WAY_NAVIGATION
class TwowayAlgorithm : public OpSnAlgorithm
{
public:
	TwowayAlgorithm();
	virtual OP_STATUS			Init(const RECT& previousRect, INT32 direction, BOOL hasActiveLink, BOOL activeLinkHasLineBreak);  ///< @overload OpSnAlgorithm
	virtual EvaluationResult	EvaluateLink(const RECT& linkRect);  ///< @overload OpSnAlgorithm
	virtual INT32				GetBestRectValue() { return MAX(bestX, bestY); }
	virtual INT32				GetCurrentRectValue() { return 0; }
	virtual void				ForceBestRectValue(const RECT& linkRect);

private:
	RECT				lastRect;
	INT32				direction;
	BOOL				hasActiveLink;
	BOOL				activeLinkHasLineBreak;
	INT32				bestX;
	INT32				bestY;
};
#endif // !USE_4WAY_NAVIGATION


#endif // SPATIAL_NAVIGATION_ALGORITHM

#endif // _SN_ALGORITHM_H_
