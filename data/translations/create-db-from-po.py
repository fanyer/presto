#!/usr/bin/env python
# -*- coding: utf-8 -*-

import argparse, re, sys

from memory import *

def main():

	parser = argparse.ArgumentParser(description='Creates an SQLite database translations.db, with table strings')

	parser.add_argument('po_file', nargs='+', help='.po files to enter into a database')

	args = parser.parse_args()

	dictionary = dict()
	for name in args.po_file:
		po                 = create_paragraph_list( name )
		lang               = set_language_from_po( po[2] )
		dictionary[ lang ] = create_dict_from_po( po )

	create_sqlite3_db( dictionary )

	return None

if __name__ == "__main__":
        main()

