# Generate 2048 bit RSA key
openssl genrsa -f4 -des3 -out ../keys/turbo_config_key.priv 2048
openssl rsa  -pubout -inform PEM -in ../keys/turbo_config_key.priv -outform der -out ../keys/turbo_config_key.pem
openssl rsa  -pubin -pubout -inform der -in ../keys/turbo_config_key.pem -outform pem -out ../keys/turbo_config_key.der
perl createinc.pl TURBO_CONFIG_KEY ../keys/turbo_config_key.pem
