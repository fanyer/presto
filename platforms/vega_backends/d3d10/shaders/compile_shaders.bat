@set FXC="%DXSDK_DIR%\Utilities\bin\x86\fxc.exe"

@%FXC% /nologo /Zpc /Tvs_4_0 /Dop_main=vert2d /Evert2d /Fhvert2d.h vert2d.vsh
@%FXC% /nologo /Zpc /Tvs_4_0_level_9_1 /Dop_main=vert2dLevel9 /Evert2dLevel9 /Fhvert2d_level9.h vert2d.vsh
@%FXC% /nologo /Zpc /Tvs_4_0 /Dop_main=vert2dMultiTex /Evert2dMultiTex /Fhvert2dmultitex.h vert2dmultitex.vsh
@%FXC% /nologo /Zpc /Tvs_4_0_level_9_1 /Dop_main=vert2dMultiTexLevel9 /Evert2dMultiTexLevel9 /Fhvert2dmultitex_level9.h vert2dmultitex.vsh

@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=colorMatrix /EcolorMatrix /Fhcolormatrix.h colormatrix.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=colorMatrixLevel9 /EcolorMatrixLevel9 /Fhcolormatrix_level9.h colormatrix.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=componentTransfer /EcomponentTransfer /Fhcomponenttransfer.h componenttransfer.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=componentTransferLevel9 /EcomponentTransferLevel9 /Fhcomponenttransfer_level9.h componenttransfer.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=displacement /Edisplacement /Fhdisplacement.h displacement.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=displacementLevel9 /EdisplacementLevel9 /Fhdisplacement_level9.h displacement.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=lightingBump /ElightingBump /Fhlighting_bump.h lighting_bump.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=lightingBumpLevel9 /ElightingBumpLevel9 /Fhlighting_bump_level9.h lighting_bump.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=lightingDistant /ElightingDistant /Fhlighting_distant.h lighting_distant.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=lightingDistantLevel9 /ElightingDistantLevel9 /Fhlighting_distant_level9.h lighting_distant.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=lightingPoint /ElightingPoint /Fhlighting_point.h lighting_point.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=lightingPointLevel9 /ElightingPointLevel9 /Fhlighting_point_level9.h lighting_point.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=lightingSpot /ElightingSpot /Fhlighting_spot.h lighting_spot.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=lightingSpotLevel9 /ElightingSpotLevel9 /Fhlighting_spot_level9.h lighting_spot.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=linearRgbToSrgb /ElinearRgbToSrgb /Fhlinearrgb_to_srgb.h linearrgb_to_srgb.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=linearRgbToSrgbLevel9 /ElinearRgbToSrgbLevel9 /Fhlinearrgb_to_srgb_level9.h linearrgb_to_srgb.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=luminance /Eluminance /Fhluminance.h luminance.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=luminanceLevel9 /EluminanceLevel9 /Fhluminance_level9.h luminance.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=mergeArithmetic /EmergeArithmetic /Fhmerge_arithmetic.h merge_arithmetic.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=mergeArithmeticLevel9 /EmergeArithmeticLevel9 /Fhmerge_arithmetic_level9.h merge_arithmetic.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=mergeDarken /EmergeDarken /Fhmerge_darken.h merge_darken.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=mergeDarkenLevel9 /EmergeDarkenLevel9 /Fhmerge_darken_level9.h merge_darken.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=mergeLighten /EmergeLighten /Fhmerge_lighten.h merge_lighten.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=mergeLightenLevel9 /EmergeLightenLevel9 /Fhmerge_lighten_level9.h merge_lighten.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=mergeMultiply /EmergeMultiply /Fhmerge_multiply.h merge_multiply.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=mergeMultiplyLevel9 /EmergeMultiplyLevel9 /Fhmerge_multiply_level9.h merge_multiply.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=mergeScreen /EmergeScreen /Fhmerge_screen.h merge_screen.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=mergeScreenLevel9 /EmergeScreenLevel9 /Fhmerge_screen_level9.h merge_screen.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=morphologyDilate15 /EmorphologyDilate15 /Fhmorphology_dilate_15.h morphology_dilate_15.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=morphologyErode15 /EmorphologyErode15 /Fhmorphology_erode_15.h morphology_erode_15.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=srgbToLinearRgb /EsrgbToLinearRgb /Fhsrgb_to_linearrgb.h srgb_to_linearrgb.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=srgbToLinearRgbLevel9 /EsrgbToLinearRgbLevel9 /Fhsrgb_to_linearrgb_level9.h srgb_to_linearrgb.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=text2d /Etext2d /Fhtext2d.h text2d.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=text2dLevel9 /Etext2dLevel9 /Fhtext2d_level9.h text2d.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=text2dTexGen /Etext2dTexGen /Fhtext2dtexgen.h text2dtexgen.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=text2dTexGenLevel9 /Etext2dTexGenLevel9 /Fhtext2dtexgen_level9.h text2dtexgen.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=vector2dTexGenRadial /Evector2dTexGenRadial /Fhvector2dtexgenradial.h vector2dtexgenradial.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=vector2dTexGenRadialLevel9 /Evector2dTexGenRadialLevel9 /Fhvector2dtexgenradial_level9.h vector2dtexgenradial.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=vector2dTexGenRadialSimple /Evector2dTexGenRadialSimple /Fhvector2dtexgenradialsimple.h vector2dtexgenradialsimple.psh
@%FXC% /nologo /Zpc /Tps_4_0_level_9_1 /Dop_main=vector2dTexGenRadialSimpleLevel9 /Evector2dTexGenRadialSimpleLevel9 /Fhvector2dtexgenradialsimple_level9.h vector2dtexgenradialsimple.psh

@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=vector2d /Evector2d /Fhvector2d.h vector2d.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=vector2dTexGen /Evector2dTexGen /Fhvector2dtexgen.h vector2dtexgen.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=convolveGen16 /EconvolveGen16 /Fhconvolve_gen_16.h convolve_gen_16.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=convolveGen16Bias /EconvolveGen16Bias /Fhconvolve_gen_16_bias.h convolve_gen_16_bias.psh
@%FXC% /nologo /Zpc /Tps_4_0 /Dop_main=convolveGen25Bias /EconvolveGen25Bias /Fhconvolve_gen_25_bias.h convolve_gen_25_bias.psh

compile_shader_blends.bat
