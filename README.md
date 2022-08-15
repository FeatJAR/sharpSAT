# sharpSAT

Instructions for building x64 static executables:

**Linux**

```
docker build -t sharpsat .
docker cp $(docker create sharpsat):/home/sharpSAT sharpSAT
chmod +x sharpSAT
```

**Windows**

* install [CMake](https://cmake.org/), [Visual Studio](https://visualstudio.microsoft.com/downloads/) with the C++ workload, and [vcpkg](https://github.com/microsoft/vcpkg) (into `..`)
* `..\vcpkg\vcpkg install gmp`
* `cmake . -DCMAKE_TOOLCHAIN_FILE=..\vcpkg\scripts\buildsystems\vcpkg.cmake`
* `msbuild sharpSAT.sln /t:ALL_BUILD /p:Configuration=Release ` (binary+DLL in `Release`, `.lib` not needed)