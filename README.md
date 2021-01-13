# cubeland
some sort of game with cubes and shit

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

## Resources
All images (as well as other resources) are stored in resource libraries. These can be generated with the `build_rsrc` tool. To generate the default resources library, run `cd tools && ./build_rsrc.py ../rsrc/textures ../build/default.rsrc`

