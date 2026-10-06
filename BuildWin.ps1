windres .\resource.rc -O coff -o resource.o
gcc .\MainWindow.c .\Camera.c .\resource.o -o Graphicality.exe -lgdi32 -mwindows
Exit