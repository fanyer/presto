/**
 * Dummy definitions of requisite plug-in wrapper symbols.
 */

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS
CoreComponent* g_opera;
OpComponentManager* g_component_manager;

extern "C"
void Debug_OpAssert(const char* expression, const char* file, int line) {}

int main(int argc, char** argv)
{
	return 0;
}
#endif // NO_CORE_COMPONENTS

