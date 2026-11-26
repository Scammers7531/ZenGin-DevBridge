[🇵🇱 Polski](THIRD_PARTY_NOTICES.md) | [🇬🇧 English](THIRD_PARTY_NOTICES.en.md)

# Third-party notices

The DevBridge DLL statically links the Union API and its enabled support libraries. Their copyright and license texts are reproduced in `LICENSES/`. The source submodules are pinned to the commits recorded below.

| Component | Version / revision | License | Notes |
| --- | --- | --- | --- |
| Union API | `167c5573653a8d89c1fee01865b1ac4502935e3f` | BSD 3-Clause | Static API implementation and hook support. |
| Gothic API | `6e554b0acb199c9544037fb7136e07fad44a2fc4` | BSD 3-Clause | Gothic II: Night of the Raven API headers and version glue. |
| Microsoft Detours | 4.0.1, `e4bfd6b03e50de46b47abfbd1e46b384f0c5f833` | MIT | Included through Union API. |
| libogg | 1.3.5, `e1774cd77f471443541596e09078e78fdc342e4f` | Xiph BSD-style license | Included through Union API. |
| libvorbis | `bb4047de4c05712bf1fd49b9584c360b8e4e0adf` | Xiph BSD-style license | Included through Union API. |
| zlib | 1.3.1, `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf` | zlib license | Included through Union API. |
| nlohmann/json | 3.11.3, single header | MIT | `src/json.hpp`; SPDX copyright and license identifiers remain in the header. |

The `LICENSES/` directory contains the license texts needed with source and binary distributions. The binary also imports the Windows system libraries `KERNEL32.dll` and `dbghelp.dll`; these are not bundled with DevBridge.

## Union plugin template review

The Patrix9999 Union plugin template repository has no published `LICENSE` file on the reviewed default branch. DevBridge does not redistribute that template's project scaffold, build files, entry-point source, or template assets. The source in this repository uses its own CMake build and a small Gothic II: Night of the Raven wrapper written for this project. The template itself is not included in source submodules or the release archive.

License names and their required texts in `LICENSES/` are preserved verbatim; this description does not replace them.
