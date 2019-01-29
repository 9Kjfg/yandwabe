cls

@echo off

set CommonCompilerFlags= -O2 -MTd -nologo /fp:fast -Gm- -GR- -EHa- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4211 -wd4244 -wd4459 -wd4505 -wd4127 -wd4456 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -DHANDMADE_WIN32=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -suvsystem:windows,5.1 %CommonLinkerFlags

REM 64-bit build
del *.pdb > NUL 2> NUL
REM Optimization switches /O2
echo WAITING FOR PDB > lock.tmp
cl %CommonCompilerFlags% ..\handmade\code\handmade.cpp -Fmhandmade.map -LD /link -incremental:no -PDB:handmade_%random%.pdb -EXPORT:GameGetSoundSamples -EXPORT:GameUpdateAndRender
del lock.tmp
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
