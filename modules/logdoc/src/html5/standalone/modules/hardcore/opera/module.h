#ifndef MODULE_H
#define MODULE_H

class OperaInitInfo
{
};

class OperaModule
{
public:
	inline void * operator new (size_t size, OperaModule* m);
#if defined USE_CXX_EXCEPTIONS && defined _DEBUG
	void operator delete( void* loc, OperaModule* m ) { /* just to shut up the compiler */ }
#endif

	virtual ~OperaModule() {}

	virtual void InitL(const OperaInitInfo& info) = 0;
	virtual void Destroy() = 0;
};

#endif // MODULE_H
