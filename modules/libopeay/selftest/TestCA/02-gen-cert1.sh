#!/bin/sh -v

openssl genrsa -out keys/key1.pem 2048
openssl req -new -subj "/CN=Libopeay test cert 1/O=Opera/ST=Oslo/C=NO" -key keys/key1.pem -sha1 -out reqs/req1.pem -text

openssl ca -batch -config openssl.cnf -in reqs/req1.pem -out certs/cert1.pem -days 3650
