# Generate 2048 bit RSA key
openssl genrsa -f4 -des3 -out verify_key.priv 2048
openssl rsa  -pubout -inform PEM -in verify_key.priv -outform der -out verify_key.pub
openssl rsa  -pubin -pubout -inform der -in verify_key.pub -outform pem -out verify_key.b64
perl createinc.pl JS_VERIFY_KEY verify_key.pub
