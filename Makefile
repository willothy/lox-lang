
.PHONY: run build clean

run: build
	./clox

build: src/*.c
	gcc -o clox src/*.c

clean:
	rm ./clox
