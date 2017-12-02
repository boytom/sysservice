@echo off
mc -U sys.mc
rc -r sys.rc
link -dll -noentry -out:sys.dll /MACHINE:x86 sys.res
@echo on
