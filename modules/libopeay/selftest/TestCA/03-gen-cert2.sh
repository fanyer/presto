#!/bin/sh -v

openssl genrsa -out keys/key2.pem 2048
openssl req -new -subj "/CN=Libopeay test cert 2/O=Opera/ST=Oslo/C=NO" -key keys/key2.pem -sha1 -out reqs/req2.pem -text

openssl ca -batch -config openssl.cnf -in reqs/req2.pem -out certs/cert2.pem -days 3650
