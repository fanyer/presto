/* Probe program for TWEAK_POSIX_USE_GMTOFF.
 *
 * See ../module.tweaks for a description of the tweak.  If this program
 * compiles without errors, reports a plausible version of your current
 * time-zone and subsequently claims to think your struct tm has a .tm_gmtoff
 * field, you can safely enable the tweak.
 *
 * Unless you know otherwise: if the time-zone reported on the "i.e." line of
 * the output mentions days or seconds, it isn't plausible; if it reports
 * minutes other than a multiple of 20, it probably isn't plausible; if it
 * reports more than twelve (well, maybe thirteen if you're in one of the
 * date-line's wiggles) hours, it stretches plausibility somewhat.
 *
 * If this program fails to compile, with errors about struct tm not having a
 * .tm_gmtoff member, then it hasn't got one !  Other compile errors should be
 * reported to the module owner, preferrably with a patch that fixes them
 * (portably).
 *
 * You may find .tm_gmtoff is missing if _BSD_SOURCE is not defined; while it
 * normally is with glibc, some situations (e.g. compiling with gcc -ansi) will
 * omit it.  Make sure you test this program with the same relevant compiler
 * options (e.g. -D_BSD_SOURCE or -ansi) as you use in delivery builds.  You may
 * also need to link with -lbsd-compat, if compiling with -D_BSD_SOURCE; again,
 * be sure to be consistent between this test program and delivery builds.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

void fragtime(int frag, int prior, int post, const char* unit) {
	if (frag || (prior && post))
		printf("%s%d %s%s",
			   prior ? (post ? ", " : " and ") : "",
			   frag,
			   unit,
			   frag == 1 ? "" : "s");
}

int main() {
	const time_t now = time(NULL);
	if (now + 1) {
		struct tm when;
		when.tm_gmtoff = ULONG_MAX / 3;
		if (localtime_r(&now, &when) == &when) {
			int ok = 1;
			printf("You appear to be %ld seconds East of UTC\ni.e. ", when.tm_gmtoff);
			if (when.tm_gmtoff) {
				long seconds = when.tm_gmtoff < 0 ? -when.tm_gmtoff : when.tm_gmtoff,
					minutes = seconds / 60, hours = minutes / 60, days = hours / 24;
				seconds -= minutes * 60;
				minutes -= hours * 60;
				hours -= days * 24;
				ok = !days && hours < 14 && hours >= 0;
				fragtime(days, 0, 1, "day");
				fragtime(hours, days, minutes || seconds, "hour");
				fragtime(minutes, days || hours, seconds, "minute");
				fragtime(seconds, days || hours || minutes, 0, "second");
				if (when.tm_gmtoff < 0)
					puts(" West of Greenwich");
				else
					puts(" East of Greenwich");
			} else
				  puts("you're on UTC");

			printf("and I%s think you have .tm_gmtoff in your struct tm %s\n",
				   ok ? "" : " don't", ok ? ":-)" : ":-(");
			return !ok;
		} else
			printf("Failed localtime_r() %d: %s\n", errno, strerror(errno));
	} else
		printf("Failed time() %d: %s\n", errno, strerror(errno));

	return 1;
}
