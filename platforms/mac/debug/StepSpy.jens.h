#ifndef _STEPSPY_H_	// {
#define _STEPSPY_H_

typedef enum
{
	kStepSpyBuffer = 0,
	kStepSpyHandle,
	kStepSpyPointerToPointer,
	kStepSpyPointerToVoid,		// check if 4 bytes changed, if not, check if pointer valid. On quick version, just try pointer.
	kStepSpySpyKindJunk			// end marker
}SpyKind;

class StepSpy
{

public:

	StepSpy	*next;
	StepSpy	*prev;
	StepSpy	**owner;

	void	*start;
	long	value;				// used for pointers to pointers (later on)
	long	length;
	long	checksum;
	char	*id;

			StepSpy(StepSpy **theOwner);
			~StepSpy();

};

extern StepSpy	*gStepSpyList;

inline long GetCheckSum2(StepSpy *travel)
{
	long	*lp;
	long	l;
	long	newCheckSum;

	newCheckSum = 0;
	lp = (long *) travel->start;
	l = travel->length >> 1;

	while(l >= 32)
	{
		newCheckSum += lp[0];
		newCheckSum += lp[1];
		newCheckSum += lp[2];
		newCheckSum += lp[3];
		newCheckSum += lp[4];
		newCheckSum += lp[5];
		newCheckSum += lp[6];
		newCheckSum += lp[7];
		newCheckSum += lp[8];
		newCheckSum += lp[9];
		newCheckSum += lp[10];
		newCheckSum += lp[11];
		newCheckSum += lp[12];
		newCheckSum += lp[13];
		newCheckSum += lp[14];
		newCheckSum += lp[15];
		newCheckSum += lp[16];
		newCheckSum += lp[17];
		newCheckSum += lp[18];
		newCheckSum += lp[19];
		newCheckSum += lp[20];
		newCheckSum += lp[21];
		newCheckSum += lp[22];
		newCheckSum += lp[23];
		newCheckSum += lp[24];
		newCheckSum += lp[25];
		newCheckSum += lp[26];
		newCheckSum += lp[27];
		newCheckSum += lp[28];
		newCheckSum += lp[29];
		newCheckSum += lp[30];
		newCheckSum += lp[31];
		lp += 32;
		l -= 32;
	}

	while(l >= 16)
	{
		newCheckSum += lp[0];
		newCheckSum += lp[1];
		newCheckSum += lp[2];
		newCheckSum += lp[3];
		newCheckSum += lp[4];
		newCheckSum += lp[5];
		newCheckSum += lp[6];
		newCheckSum += lp[7];
		newCheckSum += lp[8];
		newCheckSum += lp[9];
		newCheckSum += lp[10];
		newCheckSum += lp[11];
		newCheckSum += lp[12];
		newCheckSum += lp[13];
		newCheckSum += lp[14];
		newCheckSum += lp[15];
		lp += 16;
		l -= 16;
	}

	while(l >= 8)
	{
		newCheckSum += lp[0];
		newCheckSum += lp[1];
		newCheckSum += lp[2];
		newCheckSum += lp[3];
		newCheckSum += lp[4];
		newCheckSum += lp[5];
		newCheckSum += lp[6];
		newCheckSum += lp[7];
		lp += 8;
		l -= 8;
	}

	while(l >= 4)
	{
		newCheckSum += lp[0];
		newCheckSum += lp[1];
		newCheckSum += lp[2];
		newCheckSum += lp[3];
		lp += 4;
		l -= 4;
	}

	while(l >= 2)
	{
		newCheckSum += lp[0];
		newCheckSum += lp[1];
		lp += 2;
		l -= 2;
	}
	while(l--)
	{
		newCheckSum += *lp++;
	}

	if(travel->length & 1)
	{
		newCheckSum += ((unsigned short *) lp)[0];
	}
	return(newCheckSum);
}

inline long GetCheckSum(StepSpy *spy)
{
	long			l, l2, value, value2;
	unsigned char	*p;
	long			*lp;

	l = spy->length;				// handle flag is in top bit.

	value = 0;
	value2 = 0;

	if(l < 0)						// if it's a handle
	{
		l = l & 0x7fffffff;
		p = (unsigned char *) *((long *) spy->start);
	}
	else
	{
		p = (unsigned char *) spy->start;
	}

	while(((long) p & 3) && l)
	{
		value = (value << 8) | *p++;
		l--;
	}

	l2 = l & 3;
	lp = (long *) p;
	l = l >> 2;

	while(l >= 16)
	{
		value += lp[0];
		value += lp[1];
		value += lp[2];
		value += lp[3];
		value += lp[4];
		value += lp[5];
		value += lp[6];
		value += lp[7];
		value += lp[8];
		value += lp[9];
		value += lp[10];
		value += lp[11];
		value += lp[12];
		value += lp[13];
		value += lp[14];
		value += lp[15];

		lp += 16;
		l -= 16;
	}

	if(l >= 8)
	{
		value += lp[0];
		value += lp[1];
		value += lp[2];
		value += lp[3];
		value += lp[4];
		value += lp[5];
		value += lp[6];
		value += lp[7];

		lp += 8;
		l -= 8;
	}

	if(l >= 4)
	{
		value += lp[0];
		value += lp[1];
		value += lp[2];
		value += lp[3];

		lp += 4;
		l -= 4;
	}

	if(l >= 2)
	{
		value += lp[0];
		value += lp[1];

		lp += 2;
		l -= 2;
	}

	while(l--)
	{
		value += *lp++;
	}

	p = (unsigned char *) lp;
	while(l2--)
	{
		value2 = (value2 << 8) | *p++;
	}
	return(value + value2);
}

//void AddStepSpy(void *start, long length, short itsHandle);
void AddStepSpy(void *start, long length, SpyKind spyKind, char *id);
//void AddStepSpy(void *start, long length, short isHandle, char *id);
void RemoveStepSpy(void *start);
void CheckStepSpy(const char *inWhere, long inID);
bool HasStepSpy(void *start);

#endif	// } _STEPSPY_H_
