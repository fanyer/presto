#if !defined(MACOPDLL_H)
#define MACOPDLL_H

#if defined(USE_OPDLL)

#include "modules/pi/OpDLL.h"

class MacOpDLL : public OpDLL
{
public:
	MacOpDLL();
	~MacOpDLL();

	OP_STATUS Load(const uni_char *dll_name);

	BOOL IsLoaded() const { return loaded; }

	OP_STATUS Unload();

#ifdef DLL_NAME_LOOKUP_SUPPORT
	void *GetSymbolAddress(const char *symbol_name) const;
#else
	virtual void* GetSymbolAddress(int symbol_id) const;
#endif

#if defined(USE_BUNDLES)
	CFBundleRef GetHandle() const { return library; }
#endif

private:
	BOOL loaded;
	CFBundleRef library;
	OpString8 m_utf8_path;
	void* dll;
};

#endif

#endif
