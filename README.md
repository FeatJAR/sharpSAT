# sharpSAT

Instructions for building x64 static executables:

**Linux**

```
docker build -t sharpsat .
docker cp $(docker create sharpsat):/home/sharpSAT sharpSAT
```

**Windows**

* install [CMake](https://cmake.org/), [Visual Studio](https://visualstudio.microsoft.com/downloads/) with the C++ workload, and [vcpkg](https://github.com/microsoft/vcpkg)
* `vcpkg install gmp`
* `cmake . -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake`
* `msbuild ALL_BUILD.vcxproj` (binary+DLL in `Debug`)