@echo off
rem sign using an RSA key

call openssl sha1 -binary %1 >digest.bin
call openssl rsautl -inkey keys\turbo_config_key.priv -in digest.bin -sign -pkcs -out sign.bin
call openssl base64 -e -in sign.bin -out sign.b64

call perl scripts\createsig.pl // sign.b64 %1 %2

rem openssl rsautl -inkey keys\turbo_config_key.priv -in sign.bin -verify -pkcs -out digest_out.bin
call openssl rsautl -inkey keys\turbo_config_key.der -pubin -in sign.bin -verify -pkcs -out digest_out.bin
call fc digest_out.bin digest.bin
call del digest_out.bin digest.bin sign.bin sign.b64

