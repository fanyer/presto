IF EXIST %1ssleay32_x64.lib (
	IF EXIST %1libeay32_x64.lib (
		EXIT
	)
)

cd %2
perl Configure VC-WIN64A no-asm --prefix=./libs
call .\ms\do_win64a
nmake -f .\ms\nt.mak
copy /Y out32\ssleay32.lib %1\ssleay32_x64.lib
copy /Y out32\libeay32.lib %1\libeay32_x64.lib
