@rem
@rem Copyright 2015 the original author or authors.
@rem
@rem Licensed under the Apache License, Version 2.0
@rem
@echo off
setlocal
set DIRNAME=%~dp0
if "%DIRNAME%"=="" set DIRNAME=.
set APP_BASE_NAME=%~n0
set CLASSPATH=%DIRNAME%\gradle\wrapper\gradle-wrapper.jar
gradle %*
