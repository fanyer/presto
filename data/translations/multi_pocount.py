#!/usr/bin/python

"""
multi_pocount.py
Authored by Aaron Beaton (aaronb)
Created: 30/07/2012
Last updated: 1/08/2012
Description: Perform a batch pocount on a specified selection of languages.

There are four ways to use the script:
1) ./multi_pocount.py
Checks all main PO files in the translations repo.

2) ./multi_pocount.py -d updated_files
Checks all files with a .po extension in the specified directory.

3) ./multi_pocount.py -l da el fi zu
Checks the main PO files in the translations repo for the specified
languages.

4) ./multi_pocount.py -v /Users/aaronbeaton/Repos/volunteers/svn
Checks all main PO files in the specified volunteer repo.
"""

import sys, argparse, subprocess, glob

def main():

    parser = argparse.ArgumentParser(description='Run pocount over a specified set of languages.')
    # Create a group to ensure the three options are mutually exclusive
    group = parser.add_mutually_exclusive_group()
    group.add_argument('-d', dest='directory', help='Check all languages in the specified directory within the translations repo.')
    group.add_argument('-l', dest='languages', nargs='+', help='Check a list of languages in the translations repo.')
    group.add_argument('-v', dest='volunteers', help='Check all languages in the specified volunteer repo.')
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

    # Sort the file list alphabetically, and put all the items into a single, space separated string
    file_list.sort()
    files = ' '.join(file_list)

    # Run the pocount
    subprocess.call('pocount --csv ' + files, shell=True)

if __name__ == '__main__':
    main()  