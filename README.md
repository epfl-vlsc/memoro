# Memoro Visualizer

A tool to visualize Memoro profiling data.

# Build & Install

Run `npm update` to make sure all JS dependencies are present. 
You will then need to build the C++ data processing addon. 
To do this, you may first need to install `node-gyp` (`npm install -g node-gyp`)
Then, 

```bash
cd cpp
node-gyp build --release --target=1.7.9 --arch=x64 --dist-url=https://atom.io/download/electron
cd ..
```

The `target` is the electon version that must match that used to run the main app.
Adjust this if you need. 
Also adjust `arch` if you need, however I have not yet tested on non-\*nix x64 systems.
There is a Makefile as well that should work for most users. 

Obviously, you will need a C++ compiler installed for node-gyp to use. 

## Running

After building the C++ addon, the app can be run directly from the repo directory

```
electron .
```

## Installing

Use `electron-packager` to gather and export all code and assets into an application package. 

```bash
electron-packager . --overwrite --platform=<platform> --arch=x64 --electron-version=1.7.9 --icon=assets/icons/icon64.icns --prune=true --out=release-builds
```

Again, adjust the platform, arch, and electron version if necessary.

# Contributing

Contributions are more than welcome. 
Please submit a pull request or get in touch. 

