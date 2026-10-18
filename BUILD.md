# Building

Use Visual Studio 2022 C++ tools and Windows SDK, CMake >=3.25 and Ninja. Configure the included vcpkg.json dependencies through vcpkg (fmt, spdlog, DirectXMath, DirectXTK, rapidcsv; CommonLib source is vendored). Enable only flat SE/AE.

With a standard configured compiler environment:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
    cmake --build build --parallel 4
    ctest --test-dir build --output-on-failure

scripts/build.ps1 additionally supports the author's existing portable MSVC/vcpkg dependency layout through -BuildDependencies. That folder must contain tools/msvc, tools/ninja, tools/python-packages/cmake/data/bin, tools/vcpkg, cmake/triplets, and vcpkg_installed. It is build tooling, not part of the mod runtime. A standard CMake/vcpkg build does not require that folder.

Compile Data/Scripts/Source using Caprica for Skyrim with SkyUI SDK and Skyrim/SKSE Papyrus headers on its import path and TESV_Papyrus_Flags.flg, output to Data/Scripts. Native stubs must be marked Hidden Native. No runtime quest polling is used.

Run python scripts/make_plugin.py to create the ESL-flagged MCM quest and SEQ. Run python scripts/package.py to validate and create runtime/source packages. Run scripts/audit_chance_calls.py against your installed executable/Address Library before runtime testing.

The precompiled CommonLib dependency is optional: CMake normally rebuilds it from the included vendor source. Restore-CommonLib.py extracts the matching library to build/vendor/CommonLibSSE-NG/CommonLibSSE.lib and verifies its SHA-256. Compiler/SDK, Caprica, SkyUI SDK and game Papyrus headers must be obtained separately.
