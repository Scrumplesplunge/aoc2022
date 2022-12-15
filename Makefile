.PHONY: all clean check

SOURCES = $(wildcard src/day[0-2][0-9].aoc)
PUZZLES = $(shell find puzzles -name '*.output')
OUTPUT_BASENAMES = $(subst /,.,${PUZZLES:puzzles/%=%})
OUTPUTS = ${OUTPUT_BASENAMES:%=build/%}
.PRECIOUS: ${OUTPUTS}

MODE=Release

all: build/compiler

check: build/tests
	@cat build/tests

build:
	mkdir build

clean:
	rm -rf build

.PHONY:
cmake:
	(cd compiler && cmake -B ../build -D CMAKE_BUILD_TYPE=${MODE} && ${MAKE} -C ../build)

build/compiler: cmake | build

build/day01.%.output: build/compiler src/day01.aoc puzzles/day01/%.input
	build/compiler src/day01.aoc <puzzles/day01/$*.input >$@.tmp && mv $@{.tmp,}
build/day01.%.verdict: puzzles/day01/%.output build/day01.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day02.%.output: build/compiler src/day02.aoc puzzles/day02/%.input
	build/compiler src/day02.aoc <puzzles/day02/$*.input >$@.tmp && mv $@{.tmp,}
build/day02.%.verdict: puzzles/day02/%.output build/day02.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day03.%.output: build/compiler src/day03.aoc puzzles/day03/%.input
	build/compiler src/day03.aoc <puzzles/day03/$*.input >$@.tmp && mv $@{.tmp,}
build/day03.%.verdict: puzzles/day03/%.output build/day03.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day04.%.output: build/compiler src/day04.aoc puzzles/day04/%.input
	build/compiler src/day04.aoc <puzzles/day04/$*.input >$@.tmp && mv $@{.tmp,}
build/day04.%.verdict: puzzles/day04/%.output build/day04.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day05.%.output: build/compiler src/day05.aoc puzzles/day05/%.input
	build/compiler src/day05.aoc <puzzles/day05/$*.input >$@.tmp && mv $@{.tmp,}
build/day05.%.verdict: puzzles/day05/%.output build/day05.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day06.%.output: build/compiler src/day06.aoc puzzles/day06/%.input
	build/compiler src/day06.aoc <puzzles/day06/$*.input >$@.tmp && mv $@{.tmp,}
build/day06.%.verdict: puzzles/day06/%.output build/day06.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day07.%.output: build/compiler src/day07.aoc puzzles/day07/%.input
	build/compiler src/day07.aoc <puzzles/day07/$*.input >$@.tmp && mv $@{.tmp,}
build/day07.%.verdict: puzzles/day07/%.output build/day07.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day08.%.output: build/compiler src/day08.aoc puzzles/day08/%.input
	ulimit -s 32768 && build/compiler src/day08.aoc <puzzles/day08/$*.input >$@.tmp && mv $@{.tmp,}
build/day08.%.verdict: puzzles/day08/%.output build/day08.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day09.%.output: build/compiler src/day09.aoc puzzles/day09/%.input
	ulimit -s 32768 && build/compiler src/day09.aoc <puzzles/day09/$*.input >$@.tmp && mv $@{.tmp,}
build/day09.%.verdict: puzzles/day09/%.output build/day09.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day10.%.output: build/compiler src/day10.aoc puzzles/day10/%.input
	build/compiler src/day10.aoc <puzzles/day10/$*.input >$@.tmp && mv $@{.tmp,}
build/day10.%.verdict: puzzles/day10/%.output build/day10.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day11.%.output: build/compiler src/day11.aoc puzzles/day11/%.input
	ulimit -s 65536 && build/compiler src/day11.aoc <puzzles/day11/$*.input >$@.tmp && mv $@{.tmp,}
build/day11.%.verdict: puzzles/day11/%.output build/day11.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day12.%.output: build/compiler src/day12.aoc puzzles/day12/%.input
	ulimit -s 32768 && build/compiler src/day12.aoc <puzzles/day12/$*.input >$@.tmp && mv $@{.tmp,}
build/day12.%.verdict: puzzles/day12/%.output build/day12.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day13.%.output: build/compiler src/day13.aoc puzzles/day13/%.input
	build/compiler src/day13.aoc <puzzles/day13/$*.input >$@.tmp && mv $@{.tmp,}
build/day13.%.verdict: puzzles/day13/%.output build/day13.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day14.%.output: build/compiler src/day14.aoc puzzles/day14/%.input
	build/compiler src/day14.aoc <puzzles/day14/$*.input >$@.tmp && mv $@{.tmp,}
build/day14.%.verdict: puzzles/day14/%.output build/day14.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day15.%.output: build/compiler src/day15.aoc puzzles/day15/%.input
	build/compiler src/day15.aoc <puzzles/day15/$*.input >$@.tmp && mv $@{.tmp,}
build/day15.%.verdict: puzzles/day15/%.output build/day15.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day16.%.output: build/compiler src/day16.aoc puzzles/day16/%.input
	build/compiler src/day16.aoc <puzzles/day16/$*.input >$@.tmp && mv $@{.tmp,}
build/day16.%.verdict: puzzles/day16/%.output build/day16.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day17.%.output: build/compiler src/day17.aoc puzzles/day17/%.input
	build/compiler src/day17.aoc <puzzles/day17/$*.input >$@.tmp && mv $@{.tmp,}
build/day17.%.verdict: puzzles/day17/%.output build/day17.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day18.%.output: build/compiler src/day18.aoc puzzles/day18/%.input
	build/compiler src/day18.aoc <puzzles/day18/$*.input >$@.tmp && mv $@{.tmp,}
build/day18.%.verdict: puzzles/day18/%.output build/day18.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day19.%.output: build/compiler src/day19.aoc puzzles/day19/%.input
	build/compiler src/day19.aoc <puzzles/day19/$*.input >$@.tmp && mv $@{.tmp,}
build/day19.%.verdict: puzzles/day19/%.output build/day19.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day20.%.output: build/compiler src/day20.aoc puzzles/day20/%.input
	build/compiler src/day20.aoc <puzzles/day20/$*.input >$@.tmp && mv $@{.tmp,}
build/day20.%.verdict: puzzles/day20/%.output build/day20.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day21.%.output: build/compiler src/day21.aoc puzzles/day21/%.input
	build/compiler src/day21.aoc <puzzles/day21/$*.input >$@.tmp && mv $@{.tmp,}
build/day21.%.verdict: puzzles/day21/%.output build/day21.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day22.%.output: build/compiler src/day22.aoc puzzles/day22/%.input
	build/compiler src/day22.aoc <puzzles/day22/$*.input >$@.tmp && mv $@{.tmp,}
build/day22.%.verdict: puzzles/day22/%.output build/day22.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day23.%.output: build/compiler src/day23.aoc puzzles/day23/%.input
	build/compiler src/day23.aoc <puzzles/day23/$*.input >$@.tmp && mv $@{.tmp,}
build/day23.%.verdict: puzzles/day23/%.output build/day23.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day24.%.output: build/compiler src/day24.aoc puzzles/day24/%.input
	build/compiler src/day24.aoc <puzzles/day24/$*.input >$@.tmp && mv $@{.tmp,}
build/day24.%.verdict: puzzles/day24/%.output build/day24.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/day25.%.output: build/compiler src/day25.aoc puzzles/day25/%.input
	build/compiler src/day25.aoc <puzzles/day25/$*.input >$@.tmp && mv $@{.tmp,}
build/day25.%.verdict: puzzles/day25/%.output build/day25.%.output
	src/verdict.sh $^ >$@.tmp && mv $@{.tmp,}

build/tests: ${OUTPUTS:%.output=%.verdict}
	cat $(sort $^) >$@.tmp && mv $@{.tmp,}
