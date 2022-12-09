MODE=Release

all: build/compiler

build:
	mkdir build

.PHONY:
cmake:
	(cd compiler && cmake -B ../build -D CMAKE_BUILD_TYPE=${MODE} && ${MAKE} -C ../build)

build/compiler: cmake | build
