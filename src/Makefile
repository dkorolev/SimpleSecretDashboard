.PHONY: all clean update indent

LOGS_FILENAME="/var/log/current.jsonlines"

all: build/code
	[ -f ${LOGS_FILENAME} ] && tail -n +1 -f ${LOGS_FILENAME} | ./build/code || echo "Build successful."

build/code: build code.cc

build:
	mkdir -p build

clean:
	rm -rf build

build/%: %.cc
	g++ -std=c++11 -Wall -W -O0 -g -o $@ $< -pthread

update:
	git submodule update --init --recursive

indent:
	../../Bricks/KnowSheet/scripts/indent.sh
