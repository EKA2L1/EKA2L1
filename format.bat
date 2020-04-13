@echo off

call :DoFormatOn emu
call :DoFormatOn tests
call :DoFormatOn tools
call :DoFormatOn intests
call :DoFormatOn patch

EXIT /B %ERRORLEVEL%

:DoFormatOn
    echo Formatting src\%~1
    cd src\%~1
    for /f %%f in ('dir *.cpp *.h /b/s') do clang-format -i %%f
    cd ..\..
    exit /B 0
