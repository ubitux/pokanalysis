default: build build/MakeFile
	cd build && make

build:
	mkdir build

build/MakeFile: build
	cd build && cmake ..

clean:
	make -C build clean

distclean: clean
	rm -rf build
	rm -f tags
