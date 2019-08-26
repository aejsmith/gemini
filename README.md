# Gemini

Gemini is a hobby 3D engine project.

It is designed exclusively for modern APIs - currently Vulkan only, but D3D12 support is planned later. It aims to be able to take advantage of the capabilities of those APIs (multithreading, multiple GPU queues, etc.), while also simplifying their usage to allow for fast prototyping of new features.

There is not much to see here yet. Currently, the most interesting parts are the graphics API abstraction, and the work-in-progress render graph which drives the rendering process (handles transient resource allocation, resource state transitions, etc.).

Only runs on Linux currently, but Windows support will be added later.

## License

Gemini is licensed under the [ISC license](https://github.com/aejsmith/gemini/blob/master/Documentation/License.md).

## Prerequisites

### Linux

You must also install the following requirements (including their development packages):

* SCons
* LLVM/clang
* SDL (2.x)
* Vulkan SDK

## Building

After cloning the repository you first need to clone submodules containing some external libraries:

    $ git submodule update --init

You can then build by running SCons:

    $ scons

And finally run the test game with:

    $ Build/Release/Test

A number of options can be passed to SCons to configure the build, which will be saved until the next time they are specified. The following options are supported:

* `GAME`: Selects the game to build, look in the Games directory for what's available. Defaults to `Test`.
* `BUILD`: Either `Debug`, `Sanitize`, `Release`. The built program will be in `Build/$BUILD`. `Debug` builds disable optimisation and include many more checks. `Santize` is equivalent to `Debug` but also builds with ASan and UBSan.

Example:

    $ scons BUILD=Debug
