@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
set path=d:\handmade\handmade\misc;%path%
set _NO_DEBUG_HEAP=1

d:
cd handmade
cls