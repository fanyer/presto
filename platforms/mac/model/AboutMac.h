#ifndef ABOUT_MAC
#define ABOUT_MAC

#include "adjunct/desktop_pi/AboutDesktop.h"

class MacOperaAbout : public AboutDesktop
{
public:
	MacOperaAbout(URL &url);

	/** Platform identifier and version */
	virtual OP_STATUS GetPlatform(OpString *platform, OpString *version);

	virtual OP_STATUS GetPreferencesFile(OpString *dest);
};


#endif // ABOUT_MAC
