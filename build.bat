@echo Build test project and fuzzer (Windows)..
C:\msys64\mingw64\bin\gcc uP.c comms.c -o test.exe
C:\msys64\mingw64\bin\gcc fuzzer.c comms.c -o fuzzer.exe
