@echo off
setlocal
cd /d "%~dp0"

:: CTIME Begin ::
call "tools/CTIME/ctime" -begin tools/CTIME/build.ctm

if not exist bin mkdir bin

:: Get cl.exe
where cl >nul 2>nul
if %errorlevel%==1 (
    echo Looking for 'vcvars64.bat'.. Recommended to run from the Developer Command Prompt.
    @call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

:: Unpack Arguments.
for %%a in (%*) do set "%%a=1"
if not "%release%" == "1" set debug=1

:: Set compiler flags
set compiler_flags_common=/nologo /std:c++14 /W4 /D_CRT_SECURE_NO_WARNINGS /EHsc-
set compiler_flags_debug=/Od /Zi /DBUILD_DEBUG=1
set compiler_flags_release=/O2 /Zi /arch:AVX /DBUILD_RELEASE=1

:: Set linker flags
set libs=user32.lib
set linker_flags_common=/incremental:no /opt:ref %libs%
set linker_flags_debug=/DEBUG


if "%debug%" == "1" (
    echo [Debug Build]
    set compiler_flags=%compiler_flags_common% %compiler_flags_debug%
    set linker_flags=%linker_flags_common% %linker_flags_debug%
)

if "%release%" == "1" (
    echo [Release Build]
    set compiler_flags=%compiler_flags_common% %compiler_flags_release%
    set linker_flags=%linker_flags_common%
)

pushd bin
call cl %compiler_flags% ../source/main.cpp /Fe:ImageProcessor.exe /link %linker_flags%
popd

:: CTIME End ::
call "tools/CTIME/ctime" -end tools/CTIME/build.ctm
