
build/Release/memoro.node: $(wildcard *.cc) $(wildcard *.h) Makefile binding.gyp
	../node_modules/node-gyp/bin/node-gyp.js rebuild --release --target=1.8.7 --arch=x64 --dist-url=https://atom.io/download/electron
