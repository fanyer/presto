// Settings file for Erik Dahlström
#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#define _NO_PRAGMAMSG		/* should really go into the prefix headers. */

#define MESSAGE_FOR_AU(a)	/* nothing */
#define MESSAGE_FOR_AM(a)	/* nothing */
#define MESSAGE_FOR_ED(a)	MESSAGE:a
#define MESSAGE_FOR_JB(a)	/* nothing */
#define MESSAGE_FOR_RM(a)	/* nothing */

#ifdef _ED_DEBUG

#define SUBROUTINE(a)			Subroutine ed_check(a);	::DeadBeefOnStack();

#define CHECKSTEPSPY(a, b)		CheckStepSpy(a, b);
#define ADDSTEPSPY(a, b, c, d)	AddStepSpy(a, b, c, d);
#define REMOVESTEPSPY(a)		RemoveStepSpy(a);
#define MACSBUGHC(a, b)			DebugStr("\p;hc;g");

#include "platforms/mac/debug/Subroutine.h"
#include "platforms/mac/debug/StepSpy.jens.h"

#endif	// _ED_DEBUG

#define I_UNDERSTAND

#endif	// _SETTINGS_H_
