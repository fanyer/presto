#!/usr/bin/python

"""
multi_validator.py
Authored by Aaron Beaton (aaronb)
Created: 27/07/2012
Last updated: 1/08/2012
Description: Perform a batch validation of PO files.

The validation includes:
1. msgfmt -c (a gettext tool)
2. format_check.py (a custom script in the translations repo)

There are four ways to use the script:
1) ./multi_validator.py
Validates all main PO files in the translations repo.

2) ./multi_validator.py -d updated_files
Validates all files with a .po extension in the specified directory.

3) ./multi_validator.py -l da el fi zu
Validates the main PO files in the translations repo for the specified
languages.

4) ./multi_validator.py -v /Users/aaronbeaton/Repos/volunteers/svn
Validates all main PO files in the specified volunteer repo.
"""

import sys, argparse, subprocess, glob

def main():

    parser = argparse.ArgumentParser(description='Run msgfmt -c and format_check.py to validate PO files.')
    # Create a group to ensure the three options are mutually exclusive
    group = parser.add_mutually_exclusive_group()
    group.add_argument('-d', dest='directory', help='Validate all languages in the specified directory within the translations repo.')
    group.add_argument('-l', dest='languages', nargs='+', help='Validate a list of language codes in the translations repo.')
    group.add_argument('-v', dest='volunteers', help='Validate all languages in the specified volunteer repo.')
    args = parser.parse_args()

    file_list = []

    if args.directory:
        # If a directory is specified, grab all files in that directory with a .po extension
        directory = args.directory.strip('/')
        for files in glob.glob(directory + '/*.po'):
            file_list.append(files)

    elif args.languages:
        # If a language list is specified, grab the main PO files for those languages
        for lang in args.languages:
            for files in glob.glob(lang + '/' + lang +  '.po'):
                file_list.append(files)

    elif args.volunteers:
        # If a volunteer repo path is specified, grab all the main PO files from there
        volunteer_dir = args.volunteers.rstrip('/')
        for files in glob.glob(volunteer_dir + '/translations/??/locale/??.po'):
            file_list.append(files)

        for files in glob.glob(volunteer_dir + '/translations/??-??/locale/??-??.po'):
            file_list.append(files)

    else:
        # If nothing is specified, grab the main PO files for all languages in the translations repo
        for files in glob.glob('??/??.po'):
            file_list.append(files)

        for files in glob.glob('??-??/??-??.po'):
            file_list.append(files)

    # Sort the file list alphabetically, so it prints out nicely
    file_list.sort()

    for f in file_list:
        print f
        # Run the validators for each file
        subprocess.call("msgfmt -c " + f, shell=True)
        subprocess.call("./format_check.py " + f, shell=True)

if __name__ == '__main__':
    main()  