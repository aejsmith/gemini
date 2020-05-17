# Gemini

Gemini is a hobby 3D engine project.

It is designed exclusively for modern APIs - currently Vulkan only, but D3D12 support may be added later. It aims to be able to take advantage of the capabilities of those APIs (multithreading, multiple GPU queues, etc.), while also simplifying their usage to allow for fast prototyping of new features.

There is not much to see here yet. Currently, the most interesting parts are the graphics API abstraction, and the render graph which drives the rendering process (handles transient resource allocation, resource state transitions, etc.).

Both Linux and Windows are supported.

## License

Gemini is licensed under the [ISC license](https://github.com/aejsmith/gemini/blob/master/Documentation/License.md).

## Prerequisites

### Linux

You must install the following requirements (including their development packages):

* SCons
* LLVM/clang
* SDL (2.x)
* Vulkan SDK

### Windows

You must install the following requirements:

* Visual Studio 2019
* SCons
* Vulkan SDK

## Building

After cloning the repository you first need to clone submodules containing some external libraries:

    $ git submodule update --init

You can then build by running SCons:

    $ scons

And finally run the test game with:

    $ Build/Release/Test

Options can be passed to SCons to configure the build, which will be saved until the next time they are specified. See `scons -h` for details.

Example:

    $ scons BUILD=Debug
