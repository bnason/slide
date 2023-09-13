.PHONY: build build-debug run clean default install

default: build

build:
	meson build
	ninja -C build

build-debug:
	meson build --buildtype=debug
	ninja -C build

install: build
	ninja -C build install

run: build
	./build/slide

debug-run: build-debug
	./build/slide --log-level debug

test:
	meson test -C build --no-rebuild --verbose --suite slide
.PHONY: test

clean:
	rm -rf build
