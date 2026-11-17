windres .\resource.rc -O coff -o resource.o
g++ -O3 -march=native -ffast-math .\MainWindow.cpp .\Camera.cpp .\resource.o -o Graphicality.exe -lgdi32 -DUNICODE -D_UNICODE -mwindows -static-libgcc -static-libstdc++ -static
Exit
