#ifndef QUICK_VERSION_H
#define QUICK_VERSION_H

/******************************************************************
 *
 * Only change the few entries included in this section
 *
 * Note: All comments in this file MUST BE in the old c style format 
 *
 */

#define VER_NUM_FORCE_STR	"9.80"

#define VER_NUM_MAJOR	12
#define VER_NUM_MINOR	15
#define VER_YEAR		2012

/* The VER_BETA define needs to be defined until right before final release.
   For the final release build comment out the line below */
/*#define VER_BETA*/

#if defined(VER_BETA)
/**
* This is the text to call the version used in places like opera:about
* It should follow the following convention:
*  - "internal": string used before we release any alpha or beta versions, and for labs releases
*  - "alpha" or "alpha [1-9]": used for alpha releases, stays until beta release
*  - "beta" or "beta [1-9]": used for beta releases, stays until final release
*  - empty string for final versions
*
* Alpha and Beta releases must include the number of the alpha/beta (e.g. "beta" "alpha 1" "beta 2" etc)
*/
# define VER_BETA_NAME		UNI_L("internal")

/* This VER_BETA_STR is required by the old update checker and should match the
   number used in VER_BETA_NAME above or be 1 if there is no number above.
   Once the new Autoupdate system is running this can be removed */
# define VER_BETA_STR "1"
#endif /* VER_BETA */

#define OP_PRODUCT_NAME "desktop"

/*
 *	End of section to change version data
 *
 ******************************************************************/






/******************************************************************
 *
 * DO NOT change anything below this line!!!!!
 *
 */

#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(x) #x

/* Other defines created from the main ones above. */
#define VER_NUM_STR			XSTRINGIFY(VER_NUM_MAJOR.VER_NUM_MINOR)
#define VER_YEAR_STR		XSTRINGIFY(VER_YEAR)
#define VER_NUM_INT_STR		XSTRINGIFY(VER_NUM_MAJOR) XSTRINGIFY(VER_NUM_MINOR)
#define VER_NUM_STR_UNI		UNI_L(XSTRINGIFY(VER_NUM_MAJOR.VER_NUM_MINOR))

/* Mac definition to use in it's resource file, since this is all the preprocessor can handle */
#define VER_FULL_NUM		VER_NUM_MAJOR.VER_NUM_MINOR.VER_BUILD_NUMBER
#define VER_SHORT_NUM		VER_NUM_MAJOR.VER_NUM_MINOR

/* Ugly crap that the mac preprocessor for mac happens to actually understand to 
   concatenate all the parts of the version and build number together */
#define DUMMY_PLIST(a) a
#define VER_BUNDLE DUMMY_PLIST(VER_NUM_MAJOR)DUMMY_PLIST(VER_NUM_MINOR)DUMMY_PLIST(VER_BUILD_NUMBER)		/* CFBundleVersion for Mac Info.plist, cannot ever decrease */

/* From hardcore :P */
#define VER_NUM				VER_NUM_MAJOR										/* From hardcore :P */
#define VER_NUM_INT_STR_UNI	UNI_L(XSTRINGIFY(VER_NUM_MAJOR.VER_NUM_MINOR))		/* From hardcore :P */

/* Default bundle ID for Mac which is currently overridden for widget runtime */
#define BUNDLE_ID com.operasoftware.Opera

#endif /* !QUICK_VERSION_H */
