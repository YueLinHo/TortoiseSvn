@echo off
SETLOCAL ENABLEDELAYEDEXPANSION
set starttime=%time%
set startdir=%cd%
rem
rem build script for TortoiseSVN
rem
@if "%VSINSTALLDIR%"=="" call "%VS80COMNTOOLS%\vsvars32.bat"
if "%TortoiseVars%"=="" call TortoiseVars.bat

set INCLUDE=%~dp0ext\svn-win32-libintl\inc;%INCLUDE%
set LIB=%~dp0ext\svn-win32-libintl\lib;%LIB%

if "%1"=="" (
  SET _RELEASE=ON
  SET _DEBUG=ON
  SET _DOCS=ON
  SET _SETUP=ON
)

:getparam
if "%1"=="release" SET _RELEASE=ON
if "%1"=="RELEASE" SET _RELEASE=ON
if "%1"=="debug" SET _DEBUG=ON
if "%1"=="DEBUG" SET _DEBUG=ON
if "%1"=="doc" SET _DOCS=ON
if "%1"=="DOC" SET _DOCS=ON
if "%1"=="setup" SET _SETUP=ON
if "%1"=="SETUP" SET _SETUP=ON
shift
if NOT "%1"=="" goto :getparam

rem patch apr-iconv
copy ext\apr-iconv\lib\iconv_module.c ext\apr-iconv\lib\iconv_module_original.c /Y
copy ext\apr-iconv_patch\lib\iconv_module.c ext\apr-iconv\lib /Y

rem OpenSSL
echo ================================================================================
echo building OpenSSL
cd ..\common\openssl
perl Configure VC-WIN32 -D_CRT_NONSTDC_NO_DEPRECATE -D_USE_32BIT_TIME_T > NUL
call ms\do_masm
call nmake -f ms\ntdll.mak
@echo off

rem Subversion
echo ================================================================================
echo building Subversion
cd %startdir%\ext\Subversion
rmdir /s /q build\win32\vcnet-vcproj
del build\win32\build_*.bat
echo 0.25.4> %startdir%\ext\neon\.version

:: Seems zlib can't be compiled with assembler on VS80, so patch the Subversion generator
:: to not use the assembler
copy %startdir%\gen_win.py %startdir%\ext\subversion\build\generator\gen_win.py /Y

call python gen-make.py -t vcproj --with-openssl=..\..\..\Common\openssl --with-zlib=..\..\..\Common\zlib --with-neon=..\neon --with-apr=..\apr --with-apr-util=..\apr-util --with-apr-iconv=..\apr-iconv --enable-nls --with-berkeley-db=..\berkeley-db\db4.3-win32 --enable-bdb-in-apr-util --vsnet-version=2003

:: now copy the VS80 project files and overwrite the generated ones
:: we do this until Subversion is able to generate VS80 project files correctly
copy %startdir%\ext\build\gen_uri_delims.vcproj %startdir%\ext\apr-util\uri\gen_uri_delims.vcproj /Y
copy %startdir%\ext\build\libapr.vcproj %startdir%\ext\apr\libapr.vcproj /Y
copy %startdir%\ext\build\libapriconv.vcproj %startdir%\ext\apr-iconv\libapriconv.vcproj /Y
copy %startdir%\ext\build\libapriconv_ccs_modules.vcproj %startdir%\ext\apr-iconv\ccs\libapriconv_ccs_modules.vcproj /Y
copy %startdir%\ext\build\libapriconv_ces_modules.vcproj %startdir%\ext\apr-iconv\ces\libapriconv_ces_modules.vcproj /Y
copy %startdir%\ext\build\libaprutil.vcproj %startdir%\ext\apr-util\libaprutil.vcproj /Y
copy %startdir%\ext\build\neon.vcproj %startdir%\ext\subversion\build\win32\neon.vcproj /Y
copy %startdir%\ext\build\svn_config.vcproj %startdir%\ext\subversion\build\win32\svn_config.vcproj /Y
copy %startdir%\ext\build\svn_locale.vcproj %startdir%\ext\subversion\build\win32\svn_locale.vcproj /Y
copy %startdir%\ext\build\xml.vcproj %startdir%\ext\apr-util\xml\expat\lib\xml.vcproj /Y
copy %startdir%\ext\build\zlib.vcproj %startdir%\ext\subversion\build\win32\zlib.vcproj /Y
copy %startdir%\ext\build\subversion_vcnet.sln %startdir%\ext\subversion\subversion_vcnet.sln /Y
copy %startdir%\ext\build\apr.hw %startdir%\ext\apr\include\apr.hw /Y
copy %startdir%\ext\build\neon.mak %startdir%\ext\neon\neon.mak /Y
copy /Y %startdir%\ext\build\svn\*.* build\win32\vcnet-vcproj

rem the expat.h.in doesn't have the version information correctly set :(
copy %startdir%\ext\apr-util\xml\expat\lib\expat.h.in %startdir%\ext\apr-util\xml\expat\lib\expat.h.in_copy
copy %startdir%\expat.h.in %startdir%\ext\apr-util\xml\expat\lib\expat.h.in /Y
rem the neon tarball contains config.hw, but the source tag doesn't
copy %startdir%\neonconfig.hw %startdir%\ext\neon\config.hw /Y

if DEFINED _DEBUG (
  rem first, compile without any network/repository support
  echo building netless Subversion
  rmdir /s /q Debug\Subversion_netless
  ren subversion\svn_private_config.h  svn_private_config_copy.h
  ren subversion\svn_private_config.hw  svn_private_config_copy.hw
  copy %startdir%\svn_private_config.h subversion\svn_private_config.h
  copy %startdir%\svn_private_config.h subversion\svn_private_config.hw
  devenv subversion_vcnet.sln /useenv /rebuild debug /project "__ALL__"
  ren Debug\subversion subversion_netless
  echo building Subversion
  del subversion\svn_private_config.h
  del subversion\svn_private_config.hw
  ren subversion\svn_private_config_copy.h svn_private_config.h
  ren subversion\svn_private_config_copy.hw svn_private_config.hw
  devenv subversion_vcnet.sln /useenv /rebuild debug /project "__ALL__"
)

if DEFINED _RELEASE (
  rem first, compile without any network/repository support
  echo building netless Subversion
  rmdir /s /q Release\Subversion_netless
  ren subversion\svn_private_config.h  svn_private_config_copy.h
  ren subversion\svn_private_config.hw  svn_private_config_copy.hw
  copy %startdir%\svn_private_config.h subversion\svn_private_config.h
  copy %startdir%\svn_private_config.h subversion\svn_private_config.hw
  devenv subversion_vcnet.sln /useenv /rebuild release /project "__ALL__"
  ren Release\subversion subversion_netless
  echo building Subversion
  del subversion\svn_private_config.h
  del subversion\svn_private_config.hw
  ren subversion\svn_private_config_copy.h svn_private_config.h
  ren subversion\svn_private_config_copy.hw svn_private_config.hw
  devenv subversion_vcnet.sln /useenv /rebuild release /project "__ALL__"
)
cd %startdir%
copy ext\apr-iconv\lib\iconv_module_original.c ext\apr-iconv\lib\iconv_module.c /Y
del ext\apr-iconv\lib\iconv_module_original.c
copy %startdir%\ext\apr-util\xml\expat\lib\expat.h.in_copy %startdir%\ext\apr-util\xml\expat\lib\expat.h.in /Y
del %startdir%\ext\apr-util\xml\expat\lib\expat.h.in_copy

svn revert -R ext\apr-util
svn revert -R ext\apr
svn revert -R ext\apr-iconv
svn revert -R ext\Subversion
svn revert -R ext\neon

@echo off
rem TortoiseSVN
echo ================================================================================
echo copying files
cd %startdir%
if DEFINED _DEBUG (
  if EXIST bin\debug\iconv rmdir /S /Q bin\debug\iconv > NUL
  mkdir bin\debug\iconv > NUL
  copy .\ext\apr-iconv\Debug\iconv\*.so bin\debug\iconv > NUL
  if EXIST bin\debug\bin rmdir /S /Q bin\debug\bin > NUL
  mkdir bin\debug\bin > NUL
  copy ..\Common\openssl\out32dll\*.dll bin\debug\bin /Y > NUL
  copy .\ext\svn-win32-libintl\bin\intl3_svn.dll bin\debug\bin /Y > NUL
  copy .\ext\Subversion\db4-win32\bin\libdb43d.dll bin\debug\bin /Y > NUL
  copy .\ext\apr\Debug\libapr.dll bin\Debug\bin /Y > NUL 
  copy .\ext\apr-util\Debug\libaprutil.dll bin\Debug\bin /Y > NUL 
  copy .\ext\apr-iconv\Debug\libapriconv.dll bin\Debug\bin /Y > NUL 
)
if DEFINED _RELEASE (
  if EXIST bin\release\iconv rmdir /S /Q bin\release\iconv > NUL
  mkdir bin\release\iconv > NUL
  copy .\ext\apr-iconv\Release\iconv\*.so bin\release\iconv > NUL
  if EXIST bin\release\bin rmdir /S /Q bin\release\bin > NUL
  mkdir bin\release\bin > NUL
  copy ..\Common\openssl\out32dll\*.dll bin\release\bin /Y > NUL
  copy .\ext\svn-win32-libintl\bin\intl3_svn.dll bin\release\bin /Y > NUL
  copy .\ext\Subversion\db4-win32\bin\libdb43.dll bin\release\bin /Y > NUL
  copy .\ext\apr\Release\libapr.dll bin\Release\bin /Y > NUL 
  copy .\ext\apr-util\Release\libaprutil.dll bin\Release\bin /Y > NUL 
  copy .\ext\apr-iconv\Release\libapriconv.dll bin\Release\bin /Y > NUL 
)
echo ================================================================================
echo building TortoiseSVN
cd src
rem Build SubWCRev twice to include its own version info
copy /y version.none version.h
if NOT EXIST ..\bin\release\bin\SubWCRev.exe devenv TortoiseSVN.sln /rebuild release /project SubWCRev

:: now we can create all files with version numbers in them
cd ..
call version.bat
cd src
devenv TortoiseSVN.sln /rebuild release /project SubWCRev
if DEFINED _RELEASE (
  devenv TortoiseSVN.sln /rebuild release
  copy TortoiseSVNSetup\include\autolist.txt ..\bin\release\bin
)
if DEFINED _DEBUG (
  devenv TortoiseSVN.sln /rebuild debug
  copy TortoiseSVNSetup\include\autolist.txt ..\bin\debug\bin
)

echo ================================================================================
echo building Scintilla
cd Utils\scintilla\win32
nmake -f scintilla.mak
svn revert ..\src\LexCaml.cxx
copy ..\bin\SciLexer.dll ..\..\..\..\bin\debug\bin /Y > NUL
copy ..\bin\SciLexer.dll ..\..\..\..\bin\release\bin /Y > NUL
del ..\bin\*.dll > NUL
del ..\bin\*.exp > NUL
del ..\bin\*.lib > NUL
del ..\bin\*.pdb > NUL
del *.obj > NUL
del *.res > NUL
del *.pdb > NUL

cd ..\..\..\..\Languages
call Make_Pot.bat
cd ..
@echo off

echo ================================================================================
cd doc
if DEFINED _DOCS (
  echo generating docs
  call GenDoc.bat > NUL
)
@echo off

echo ================================================================================
cd ..\src\TortoiseSVNSetup
@echo off
if DEFINED _SETUP (
  echo building installers

  call MakeMsi.bat
  cd ..\..\Languages
  call Make_Installer.bat
  cd ..\src\TortoiseSVNSetup
)

:end
cd %startdir%
echo started at: %starttime%
echo now it is : %time%