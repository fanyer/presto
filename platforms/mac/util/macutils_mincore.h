#ifndef MACUTILS_MINCORE_H_
#define MACUTILS_MINCORE_H_

#define BOOL NSBOOL
#import <objc/objc.h>
#undef BOOL

class OperaNSReleasePool
{
public:
	OperaNSReleasePool();
	~OperaNSReleasePool();
private:
	void* pool;
};

void SwizzleClassMethods(Class swizzle_class, SEL origSel, SEL newSel);

BOOL SetUniString(UniString& dest, void *source);
void SetNSString(void **dest, const UniString& source);

void ConvertFSRefToNSString(void **dest, const FSRef *fsref);
BOOL ConvertFSRefToUniString(UniString& dest, const FSRef *fsref);

void SetProcessName(const UniString &name);

double GetMonotonicMS();
BOOL IsDeadKey(uni_char c);
BOOL IsModifierUniKey(uni_char c);
UniChar OpKey2UniKey(uni_char c);

#ifdef _DEBUG
// Debug helper to print the execution time down to the millisecond
class MilliSecondTimer
{
public:
	MilliSecondTimer() { Microseconds((UnsignedWide*)&m_time); }
	~MilliSecondTimer() { Now(-1); }
	
	void Now(int i) { unsigned long long now_time; Microseconds((UnsignedWide*)&now_time); printf("Execute time @ %d: %llu\n", i, (now_time - m_time) / (unsigned long long)1000); m_time = now_time; }
	
	unsigned long long m_time;
};
#endif // _DEBUG


#endif //MACUTILS_MINCORE_H_
