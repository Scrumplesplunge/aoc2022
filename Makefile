all: build/compiler

build:
	mkdir build

.PHONY:
cmake:
	(cd compiler && cmake -B ../build && ${MAKE} -C ../build)

build/compiler: cmake | build
