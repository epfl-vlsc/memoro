# Setting Up Memoro

Memoro consists of two parts - the compiler itself and the visualizer application. This guide will explain the various steps needed to set-up both of these tools.

## Compiler

Memoro was built using a fork of LLVM/Clang and in-order to use Memoro you have to use these forks. They can be found here:

[LLVM](https://github.com/epfl-vlsc/llvm)

[Clang](https://github.com/epfl-vlsc/clang)

[CompilerRT](https://github.com/epfl-vlsc/compiler-rt)

In order to set this up:

```bash
1. mkdir memoro_compiler
2. cd memoro_compiler
3. git clone -b memoro_80 https://github.com/epfl-vlsc/llvm.git
4. cd llvm/tools
5. git clone -b memoro_80 https://github.com/epfl-vlsc/clang.git
6. cd ../projects`
7. git clone -b memoro_80 https://github.com/epfl-vlsc/compiler-rt.git
8. cd ../../
9. mkdir build
10. cd build
11. cmake -G "Ninja" ../llvm
12. ninja
```

The above instructions are based off the main LLVM [page](https://releases.llvm.org/4.0.1/docs/GettingStarted.html). You need to have cmake installed for this to work.

There are other ways of doing this but this is a straightforward and tested way.

The above process will take some time to build. Once it is done, you can test your by running clang or clang++ in `build/bin` folder.

## Visualizer

The visualizer is an electron application that will come in handy to analyse your output from memoro. The visalizer can be set-up as follows:

```bash
1. git clone https://github.com/epfl-vlsc/memoro/
2. cd memoro
3. npm install
4. cd cpp
5. make
6. cd ../
7. npm start
```

**Note**: for the above to work you need to have NodeJS/npm installed on your computer. The current tested version is v8.16.1. There is a cpp interface which has to built using `node-gyp` (install this using `npm install -g node-gyp`). The makefile will do the necessary after that. 

## Using Memoro

Now that you have both Memoro and the visualizer set-up, you can refer to [here](use_case.md) for how to use Memoro.
