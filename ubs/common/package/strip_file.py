# Delete all the blank lines from the start and end
# of a file
#
# Usage: python input_filename output_filename
#
import sys

fr=open(sys.argv[1], 'r')
filetext = fr.read()
fr.close()

fw=open(sys.argv[2], 'w')
fw.write(filetext.strip() + "\n")
fw.close()
