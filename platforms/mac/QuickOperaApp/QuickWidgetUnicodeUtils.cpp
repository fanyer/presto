#include "platforms/mac/QuickOperaApp/QuickWidgetUnicodeUtils.h"

int UniStrLen(const UniChar * str)
{
	int i;
	for (i = 0; ; i++)
		if (!str[i])
			return i;
}
