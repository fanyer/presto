#!/usr/bin/python

"""
multi_sjoin.py
Authored by Aaron Beaton (aaronb)
Created: 30/07/2012
Last updated: 31/07/2012
Description: Perform a batch sjoin on a specified selection of languages.

Two arguments must be provided:
1) The db number in the filename of the updated files.
2) A specific list of languages to process in the form of a space
separated list of language codes.

Example usage:
multi_sjoin.py 1043 -l da el fi
"""

import sys, argparse, subprocess

# The sub-directory where the updated PO files can be found
update_dir = 'updated_files/'

def main():

    parser = argparse.ArgumentParser(description='Run a batch sjoin on a specified set of languages.')
    parser.add_argument('db', type=int, help='database number in updated file name.')
    parser.add_argument('languages', nargs='+', help='list of language codes to process')
    args = parser.parse_args()

    # Join the languages specified
    langs = sorted(args.languages)
    db = args.db
    print 'Joining', len(langs), 'language(s):', ', '.join(langs)

    # lists for storing languages which succeed and fail
    success = []
    error = []

    for lang in langs:
        print lang + '...merging ' + lang + '_db' + str(db) + '.po into ' + lang + '/' + lang + '.po'
        try:
            # Make sure both files are available
            with open(update_dir + lang + '_db' + str(db) + '.po') as f:
                with open(lang + '/' + lang + '.po') as f: pass
        except IOError as e:
            error.append(lang)
            # Print error message in red
            print '\x1b[31mFAILED: File(s) missing.\x1b[0m'
            continue

        subprocess.call('./sjoin.pl ' + update_dir + lang + '_db' + str(db) + '.po ' + lang + '/' + lang + '.po ' + lang + '/' + lang + '_new.po', shell=True)

        try:
            # Make sure our new file is generated.
            with open(lang + '/' + lang + '_new.po') as f: pass
        except IOError as e:
            error.append(lang)
            # Print error message in red
            print '\x1b[31mFAILED: New PO file was not generated correctly.\x1b[0m'
            continue

        # Remove the old PO file and replace it with the new one.
        subprocess.call('rm ' + lang + '/' + lang + '.po', shell=True)
        subprocess.call('mv ' + lang + '/' + lang + '_new.po ' + lang + '/' + lang + '.po', shell=True)
        success.append(lang)

    # Print out a summary with all of the successes and failures
    print '----------'
    print 'Summary'
    print '----------'
    total = len(success) + len(error)
    print total, 'language(s) processed in total.'
    # Print successes in green
    print '\x1b[32m' + str(len(success)), 'language(s) successfully joined:', ', '.join(success), '\x1b[0m'
    # Print failures in red
    print '\x1b[31m' + str(len(error)), 'language(s) failed:', ', '.join(error), '\x1b[0m'

if __name__ == '__main__':
    main()