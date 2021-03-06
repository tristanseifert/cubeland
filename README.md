# cubeland
some sort of game with cubes (a shameless Minecraft clone)

![Demo screenshot](https://tseifert.me/public/gh-screenshots/hotlink-ok/cubeland-sphere.png)

## Requirements
- **sqlite3**: World storage format
- **libcurl**: Network access for working with servers and some future authentication APIs

### Included
The following dependencies are included as submodules:

- **[FastNoise2](https://github.com/Auburn/FastNoise2)**: Generates noise patterns used in terrain generation.
    - First, change into the library directory and check out its submodules: `cd libs/fastnoise2 && git submodule update --init --recursive`
    - Then, build it: `mkdir build && cd build && cmake .. && make -j16`
- **[glbinding v3](https://glbinding.org)**: C++ binding to OpenGL types and functions, autogenerated from GL spec. This is included in the libraries dir and needs to be built; to do this, run `cd libs/glbinding && mkdir build && cd build && cmake -DBUILD_SHARED_LIBS=false .. && cmake --build . --config Release -j16`
- **[ReactPhysics 3D](https://www.reactphysics3d.com)**: Game physics engine. Once cloned, it needs to be built using cmake: `cd libs/reactphysics3d && mkdir build && cd build && cmake .. && make -j16`
- **[LibreSSL](https://www.libressl.org)**: TLS encryption for multiplayer connections and miscellaneous crypto stuff. Follow the directions in its readme to build it in place; it will be linked against properly. Be sure to pass `--enable-shared=no` to the config step so we link statically to it.
- **[LibJPEG-Turbo](https://libjpeg-turbo.org)**: Encoding/decoding JPEG images for world screenshots, primarily. This is a little tricky to compile on macOS to produce fat binaries; see the notes below.

### macOS Note
To properly support macOS, including Apple Silicon, it's necessary to build fat (universal) binaries containing both an x86_64 and arm64e slice. When invoking CMake to generate the Makefiles, pass the `-DCMAKE_OSX_ARCHITECTURES="arm64;arm64e;x86_64"` switch to it. This holds true for both the main executable and _all_ libraries.

*** Note: *** Currently, the FastNoise2 library doesn't seem to support arm64 properly. There's some NEON support in it but it appears rather broken :(

##### LibJPEG-Turbo
The build scripts for this library do not support building fat binaries; you can only build x86_64 OR arm64e arch, but not both simultaneously. The solution to this is to simply build the library twice, then combine the resulting static archives. The build script assumes that these two binaries are in the `build_x86` and `build_arm64` directories. This is only for macOS; on all other platforms, you only need to build the x86 version in that directory.

## Resources
All images (as well as other resources) are stored in resource libraries. These can be generated with the `build_rsrc` tool. To generate the default resources library, run `cd tools && ./build_rsrc.py ../rsrc/textures ../build/default.rsrc`

## License
Cubeland is released under a 2-clause BSD license. See `LICENSE.md` for more.

## More Info
I wrote [a blog post](https://blraaz.me/graphics/2021/04/13/introducing-cubeland.html) about the project. There might be more coming in the future. Who knows?

