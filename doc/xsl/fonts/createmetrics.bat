@echo off
set LIBDIR=D:\Development\SVN\TortoiseSVN\Tools\fop\lib\
set LOCAL_FOP_HOME=D:\Development\SVN\TortoiseSVN\Tools\fop\

set LOCALCLASSPATH=%LOCAL_FOP_HOME%build\fop.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LOCAL_FOP_HOME%build\fop-sandbox.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LOCAL_FOP_HOME%build\fop-hyph.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\xml-apis-1.3.04.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\xml-apis-ext-1.3.04.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\xercesImpl-2.7.1.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\xalan-2.7.0.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\serializer-2.7.0.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\batik-all-1.7.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\xmlgraphics-commons-1.4.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\avalon-framework-4.2.0.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\commons-io-1.3.1.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\commons-logging-1.0.4.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\jai_imageio.jar
set LOCALCLASSPATH=%LOCALCLASSPATH%;%LIBDIR%\fop-hyph.jar


set CMD="c:\Program Files (x86)\Java\jre6\bin\java" 
set CMD=%CMD% -cp %LOCALCLASSPATH%
set CMD=%CMD% org.apache.fop.fonts.apps.TTFReader
set FONT_DIR=C:\Windows\Fonts
%CMD% %FONT_DIR%\arial.ttf arial.xml
%CMD% %FONT_DIR%\arialbd.ttf arialbd.xml
%CMD% %FONT_DIR%\arialbi.ttf arialbi.xml
%CMD% %FONT_DIR%\ariali.ttf ariali.xml
%CMD% %FONT_DIR%\cour.ttf cour.xml
%CMD% %FONT_DIR%\courbd.ttf courbd.xml
%CMD% %FONT_DIR%\courbi.ttf courbi.xml
%CMD% %FONT_DIR%\couri.ttf couri.xml
%CMD% %FONT_DIR%\iqraa.ttf iqraa.xml
%CMD% %FONT_DIR%\msgothic.ttf msgothic.xml
%CMD% %FONT_DIR%\msmincho.ttf msmincho.xml
%CMD% %FONT_DIR%\msyh.ttf msyh.xml
%CMD% %FONT_DIR%\msyhbd.ttf msyhbd.xml
%CMD% %FONT_DIR%\simhei.ttf simhei.xml
%CMD% %FONT_DIR%\simsun.ttf simsun.xml
%CMD% %FONT_DIR%\times.ttf times.xml
%CMD% %FONT_DIR%\timesbd.ttf timesbd.xml
%CMD% %FONT_DIR%\timesbi.ttf timesbi.xml
%CMD% %FONT_DIR%\timesi.ttf timesi.xml

:: in case the ttf isn't available, try the ttc
%CMD% -ttcname "MS Gothic" %FONT_DIR%\msgothic.ttc msgothic.xml
%CMD% -ttcname "SimSun" %FONT_DIR%\simsun.ttc simsun.xml
