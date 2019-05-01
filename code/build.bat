cls

@echo off

set CommonCompilerFlags=-Od -MTd -nologo /fp:fast -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4211 -wd4244 -wd4459 -wd4505 -wd4127 -wd4456 -FC -Z7
set CommonCompilerFlags= -DHANDMADE_INTERNAL=0 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 %CommonCompilerFlags%
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

del *.pdb > NUL 2> NUL

REM Asset file builder build
REM cl %CommonCompilerFlags% -DTRANSLATION_UNIT_INDEX=0 -D_CRT_SECURE_NO_WARNINGS=1 ..\handmade\code\test_asset_builder.cpp /link %CommonLinkerFlags%

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -suvsystem:windows,5.1 %CommonLinkerFlags

REM 64-bit build
REM Optimization switches /O2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% -O2 -DTRANSLATION_UNIT_INDEX=1 -I..\..\iaca-win64 -c ..\handmade\code\handmade_optimized.cpp -Fohandmade_optimized.obj -LD
cl %CommonCompilerFlags% -DTRANSLATION_UNIT_INDEX=0 -I..\..\iaca-win64 ..\handmade\code\handmade.cpp handmade_optimized.obj -Fmhandmade.map -LD /link -incremental:no -PDB:handmade_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender -EXPORT:DEBUGGameFrameEnd
del lock.tmp
cl %CommonCompilerFlags% -DTRANSLATION_UNIT_INDEX=2 ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
