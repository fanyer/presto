/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

/**
 * @file erasettings.h
 * Default ERA settings selected by the tweaks system.
 *
 * See module.tweaks for details.
 */

#ifndef ERASETTINGS_H
#define ERASETTINGS_H

// TWEAK_PREFS_ERA_PROFILE ===============================================

// Desktop profile -------------------------------------------------------
#if PREFS_ERA_PROFILE == 1
# define RM1_FXF 0
# define RM2_FXF 1
# define RM3_FXF 2
# define RM4_FXF 3
# define RM5_FXF 2
# define RM1_IRL 1
# define RM2_IRL 1
# define RM3_IRL 0
# define RM4_IRL 0
# define RM5_IRL 0
# define RM1_IMW 0
# define RM2_IMW 0
# define RM3_IMW 10
# define RM4_IMW 10
# define RM5_IMW 20
# define RM1_MCS 200
# define RM2_MCS 280
# define RM3_MCS 640
# define RM4_MCS INT_MAX

// ERA specification (default) -------------------------------------------
#elif PREFS_ERA_PROFILE == 0
# define RM1_FXF 0
# define RM2_FXF 0
# define RM3_FXF 1
# define RM4_FXF 2
# define RM5_FXF 2
# define RM1_IRL 1
# define RM2_IRL 1
# define RM3_IRL 1
# define RM4_IRL 0
# define RM5_IRL 0
# define RM1_IMW 0
# define RM2_IMW 0
# define RM3_IMW 30
# define RM4_IMW 0
# define RM5_IMW 20
# define RM1_MCS 200
# define RM2_MCS 250
# define RM3_MCS 500
# define RM4_MCS 800
#endif

// TWEAK_PREFS_ERA_FONT_SIZE =============================================

// Base font size 13 points ----------------------------------------------
#if PREFS_ERA_FONT_SIZE == 13
# define RM1_FSM 13
# define RM2_FSM 13
# define RM3_FSM 13
# define RM4_FSM 13
# define RM5_FSM 18
# define RM1_FME 15
# define RM2_FME 15
# define RM3_FME 15
# define RM4_FME 15
# define RM5_FME 18
# define RM1_FLA 17
# define RM2_FLA 17
# define RM3_FLA 17
# define RM4_FLA 17
# define RM5_FLA 21
# define RM1_FXL 19
# define RM2_FXL 19
# define RM3_FXL 19
# define RM4_FXL 19
# define RM5_FXL 24
# define RM1_FXX 21
# define RM2_FXX 21
# define RM3_FXX 21
# define RM4_FXX 21
# define RM5_FXX 32

// Base font size 9 points (default) -------------------------------------
#elif PREFS_ERA_FONT_SIZE == 9
# define RM1_FSM 9
# define RM2_FSM 9
# define RM3_FSM 9
# define RM4_FSM 9
# define RM5_FSM 18
# define RM1_FME 11
# define RM2_FME 11
# define RM3_FME 11
# define RM4_FME 11
# define RM5_FME 18
# define RM1_FLA 13
# define RM2_FLA 13
# define RM3_FLA 13
# define RM4_FLA 13
# define RM5_FLA 21
# define RM1_FXL 15
# define RM2_FXL 15
# define RM3_FXL 15
# define RM4_FXL 15
# define RM5_FXL 24
# define RM1_FXX 17
# define RM2_FXX 17
# define RM3_FXX 17
# define RM4_FXX 17
# define RM5_FXX 32
#endif

// TWEAK_PREFS_ERA_CONTENT_MAGIC (use tweak value directly) ==============

#define RM1_TTM PREFS_ERA_CONTENT_MAGIC
#define RM2_TTM PREFS_ERA_CONTENT_MAGIC
#define RM3_TTM 0
#define RM4_TTM 0
#define RM5_TTM 0

// TWEAK_PREFS_ERA_ALL_IMAGES_SSR ========================================

// Show all images in all modes ------------------------------------------
#ifdef SHOW_ALL_IMAGES_DEFAULT
# define RM1_ISI 2
# define RM2_ISI 2
# define RM3_ISI 2
# define RM4_ISI 2
# define RM5_ISI 2

// In SSR, show some images only (default) -------------------------------
#else
# define RM1_ISI 1
# define RM2_ISI 2
# define RM3_ISI 2
# define RM4_ISI 2
# define RM5_ISI 2
#endif

#endif // ERASETTINGS_H
