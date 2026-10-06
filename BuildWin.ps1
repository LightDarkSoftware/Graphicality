windres .\resource.rc -O coff -o resource.o
g++ .\MainWindow.cpp .\Camera.cpp .\resource.o -o Graphicality.exe -lgdi32 -DUNICODE -D_UNICODE -mwindows
Exit
