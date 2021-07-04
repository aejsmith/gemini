# Gemini

Gemini is a hobby 3D engine project. It is designed exclusively for modern APIs (currently Vulkan is supported), and aims to be able to take advantage of the capabilities of those APIs (multithreading, multiple GPU queues, pre-baked pipeline state objects, etc.).

<a href="https://raw.githubusercontent.com/aejsmith/gemini/master/Documentation/Screenshot.png"><img src="https://raw.githubusercontent.com/aejsmith/gemini/master/Documentation/ScreenshotSmall.png"></a>

More screenshots and videos in the [Screenshot Archive](/Documentation/Screenshots.md).

Features:

* Tile-based deferred lighting
* Physically-based shading
* Shadow mapping
* Render graph for automatically handling GPU synchronization and transient resource allocation
* Physics (using Bullet)
* Runtime object reflection/serialization, driven by code annotations used with a custom code generation tool
* glTF model importer
* Linux and Windows support

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

## License

Gemini is licensed under the [ISC license](/Documentation/License.md).
