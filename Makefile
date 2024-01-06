

build: src/*.c
	gcc -o clox src/*.c

clean: clox
	rm clox
