#ifndef OPERA_FT_MODULES_H
#define OPERA_FT_MODULES_H

#if defined FT_USE_TT_FONTS || defined FT_USE_CFF_FONTS
# define FT_USE_SFNT
#endif

#if defined FT_USE_CFF_FONTS || defined FT_USE_T1_FONTS
# define FT_USE_PSHINTER
#endif

#if defined FT_USE_T1_FONTS
# define FT_USE_PSAUX
#endif

#if defined FT_USE_SFNT || defined FT_USE_T1_FONTS
# define FT_USE_PSNAMES
#endif

#endif // OPERA_FT_MODULES_H
