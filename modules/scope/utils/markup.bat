rem Script for doing fixating markup on the grammar and copying it to kaleidoscope

cd ..\documentation
..\utils\markup.py
del scope-specs/*
mkdir scope-specs
copy fixed\* scope-specs
copy ..\..\coredoc\coredoc.css scope-specs
cd scope-specs
del .*
del Doxyfile
del *.bak
cd ..

rem Now scope-specs contains everything that should be on kaleidoscope
ssh kaleidoscope.vlab.osa "rm -rf /var/www/html/scope-specs"
scp -r scope-specs kaleidoscope.vlab.osa:/var/www/html
ssh kaleidoscope.vlab.osa "chgrp -R www-data /var/www/html/scope-specs; chmod -R g+r /var/www/html/scope-specs; chmod g+x /var/www/html/scope-specs"

cd ..\utils
