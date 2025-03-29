filename = grua

linux:
	g++ main.cpp -lglad -lglfw -o $(filename)
	sudo systemctl stop keyd
	./$(filename); sudo systemctl start keyd

win:
	i686-w64-mingw32-g++ main.cpp \
	-o $(filename).exe \
	-Linclude \
	-lglfw3dll \
	-lglfw3 \
	include/glad/glad.c \
	-static-libstdc++ \
	-static-libgcc \
	-static

	sudo systemctl stop keyd
	wine $(filename).exe; sudo systemctl start keyd
