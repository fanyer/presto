
#ifndef STDLIB_DTOA_FLOITSCH_DOUBLE_FORMAT_H
#define STDLIB_DTOA_FLOITSCH_DOUBLE_FORMAT_H

#ifdef DTOA_FLOITSCH_DOUBLE_CONVERSION
extern char *stdlib_dtoa(double d, int mode, int ndigits, OpDoubleFormat::RoundingBias bias, int *decpt, int *sign, char **rve);
#define stdlib_freedtoa(x)
#define stdlib_dtoa_shutdown(x)
#endif // DTOA_FLOITSCH_DOUBLE_CONVERSION

#endif // STDLIB_DTOA_FLOITSCH_DOUBLE_FORMAT_H
