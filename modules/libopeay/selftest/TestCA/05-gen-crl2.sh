#!/bin/sh -v

openssl ca -batch -config openssl.cnf -revoke certs/cert2.pem

openssl ca -batch -config openssl.cnf -gencrl -crldays 3650 | \
	openssl crl -out crls/crl2.pem -text
