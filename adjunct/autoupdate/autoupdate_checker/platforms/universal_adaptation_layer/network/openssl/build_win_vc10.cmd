IF EXIST %1ssleay32.lib (
	IF EXIST %1libeay32.lib (
		EXIT
	)
)

cd %2
call "%VS100COMNTOOLS%vsvars32.bat"
perl Configure VC-WIN32 no-asm --prefix=./libs
call ms\do_ms
nmake -f .\ms\nt.mak
copy /Y out32\ssleay32.lib %1
copy /Y out32\libeay32.lib %1
