inline bool operator == (
		OpPoint	inPointOne,
		OpPoint	inPointTwo)
{
	return(	inPointOne.x == inPointTwo.x &&
			inPointOne.y == inPointTwo.y);
}

inline bool operator != (
		OpPoint	inPointOne,
		OpPoint	inPointTwo)
{
	return(	inPointOne.x != inPointTwo.x ||
			inPointOne.y != inPointTwo.y);
}

inline bool operator == (
		OpPoint	inPointOne,
		Point	inPointTwo)
{
	return(	inPointOne.x == inPointTwo.h &&
			inPointOne.y == inPointTwo.v);
}


inline bool operator != (
		OpPoint	inPointOne,
		Point	inPointTwo)
{
	return(	inPointOne.x != inPointTwo.h ||
			inPointOne.y != inPointTwo.v);
}

inline bool operator == (
		Point	inPointOne,
		OpPoint	inPointTwo)
{
	return(	inPointOne.h == inPointTwo.x &&
			inPointOne.v == inPointTwo.y);
}

inline bool operator != (
		Point	inPointOne,
		OpPoint	inPointTwo)
{
	return(	inPointOne.h != inPointTwo.x ||
			inPointOne.v != inPointTwo.y);
}

