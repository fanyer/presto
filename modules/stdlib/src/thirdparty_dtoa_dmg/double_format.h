/****************************************************************
 *
 * The author of this software is David M. Gay.
 *
 * Copyright (c) 1991, 1996 by Lucent Technologies.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 ***************************************************************/

#ifndef STDLIB_DTOA_DMG_G_FMT_H
#define STDLIB_DTOA_DMG_G_FMT_H

#ifdef DTOA_DAVID_M_GAY
extern char *stdlib_dtoa(double d, int mode, int ndigits, OpDoubleFormat::RoundingBias bias, int *decpt, int *sign, char **rve);
extern void stdlib_freedtoa(char*);
extern void stdlib_dtoa_shutdown(int final);
#endif // DTOA_DAVID_M_GAY

#endif // STDLIB_DTOA_DMG_G_FMT_H
