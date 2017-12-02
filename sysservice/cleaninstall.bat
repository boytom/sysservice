@echo off
del C:\windows\system32\sys.bak.dll 
del C:\windows\system32\sys.dll 
del C:\windows\system32\sysservice.bak.exe
C:\windows\system32\sysservice.exe stop
C:\windows\system32\sysservice.exe delete
del C:\windows\system32\sysservice.exe
del C:\windows\system32\rich.bak.exe
reg delete "HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\sysupdate" /f
pause
@echo on
