#!/bin/sh -v

openssl ca -batch -config openssl.cnf -gencrl -crldays 3650 | \
	openssl crl -out crls/crl1.pem -text
