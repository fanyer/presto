@echo off

@set FXC="%DXSDK_DIR%\Utilities\bin\x86\fxc.exe"

:: mapping between defines and values in [st]wrap.shd - order in loops below must correspond to this mapping

Set /A si=0
Set /A ti=0

setlocal EnableDelayedExpansion
for %%S in (REPEAT CLAMP MIRROR) do (
  Set /A si+=1
  Set /A ti=0
  for %%T in (REPEAT CLAMP MIRROR) do (
    Set /A ti+=1
    call:compileShaderWithDefines %%S %%T !si! !ti! Vector2d vector2d.psh
    call:compileShaderWithDefines %%S %%T !si! !ti! Vector2dTexGen vector2dtexgen.psh
  )
)

Set /A si=0

setlocal EnableDelayedExpansion
for %%S in (REPEAT CLAMP) do (
  Set /A si+=1
  call:compileShaderWithDefines %%S %%S !si! !si! FilterMorphologyDilate morphology_dilate_15.psh
  call:compileShaderWithDefines %%S %%S !si! !si! FilterMorphologyErode morphology_erode_15.psh

  call:compileShaderWithDefines %%S %%S !si! !si! FilterConvolve16 convolve_gen_16.psh
  call:compileShaderWithDefines %%S %%S !si! !si! FilterConvolve16Bias convolve_gen_16_bias.psh
)

call:compileShaderWithDefines CLAMP CLAMP 2 2 FilterConvolve25Bias convolve_gen_25_bias.psh
call:compileShaderWithDefines_2_X REPEAT REPEAT 1 1 FilterConvolve25Bias convolve_gen_25_bias.psh

goto:eof

::-- <S define> <T define> <S value> <T value> <shader entry point> <shader file>
:compileShaderWithDefines
echo compiling %~5 with S-mode %~1 and T-mode %~2
@%FXC% /D SWRAP=%~3 /D TWRAP=%~4 /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=%~5 /E%~5 /Fh%~5_%~1_%~2_level9.h /Vn g_ps20_%~5_%~1_%~2 %~6
goto:eof

::-- <S define> <T define> <S value> <T value> <shader entry point> <shader file>
:compileShaderWithDefines_2_X
echo compiling %~5 with S-mode %~1 and T-mode %~2
@%FXC% /D SWRAP=%~3 /D TWRAP=%~4 /nologo /Zpc /Tps_4_0_level_9_3 /Dop_main=%~5 /E%~5 /Fh%~5_%~1_%~2_level9_3.h /Vn g_ps20_%~5_%~1_%~2 %~6
goto:eof
