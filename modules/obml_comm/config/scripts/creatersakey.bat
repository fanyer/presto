@echo off
rem Generate 2048 bit RSA key
call openssl genrsa -f4 -des3 -out ..\keys\turbo_config_key.priv 2048
call openssl rsa  -pubout -inform PEM -in ..\keys\turbo_config_key.priv -outform der -out ..\keys\turbo_config_key.pem
call openssl rsa  -pubin -pubout -inform der -in ..\keys\turbo_config_key.pem -outform pem -out ..\keys\turbo_config_key.der
call perl createinc.pl TURBO_CONFIG_KEY ..\keys\turbo_config_key.pem
