#!/usr/bin/env python

import sys

max_literal_size=500   # characters


def usage(progname):
    'Returns usage message'
    return 'usage:  %s <file-to-split>' % progname


if len(sys.argv) != 2:
    sys.exit(usage(sys.argv[0]))
filename = sys.argv[1]

try:
    input = file(filename).read()
except IOError, msg:
    sys.exit(msg)

nfragments = 0
while input:
    fragment,input = input[:max_literal_size], input[max_literal_size:]
    frag_file = file('%s.%02d' % (filename, nfragments), 'w')
    nfragments += 1
    frag_file.write(fragment)
    frag_file.close()

print '''
// Start auto-generated code for reading input from files.
// Because pike selftests create literals which cannot be more than 509 bytes, which is far too little.
// * created by generate-readcode.py *

const UINT ninputs = %d;
char* input_arr[ninputs];
UINT input_len = 0;
''' % nfragments

for i in range(nfragments):
    print 'input_arr[%d] = #string "%s.%02d";' % (i, filename, i)

print '''
for (UINT i=0; i<ninputs; i++)
    input_len += op_strlen(input_arr[i]);
char* input = new char[input_len+1];
verify(input != NULL);
input[0] = '\\0';
for (UINT j=0; j<ninputs; j++)
    op_strcat(input, input_arr[j]);

// End auto-generated read code
'''
