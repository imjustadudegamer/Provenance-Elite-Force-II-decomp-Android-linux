# Third-party components

Vendored under `app/src/main/cpp/code/thirdparty/` and wired by
`app/src/main/cpp/cmake/libraries/`.

| Component | Licence |
|---|---|
| SDL2 (2.30.9, 2.32.8) | zlib |
| OpenAL Soft (`openal-soft-1.24.3`) | LGPL-2.1 |
| libmad | GPL-2.0 |
| libogg (`1.3.6`), libvorbis (`1.3.7`) | BSD-3-Clause |
| opus (`1.5.2`), opusfile (`0.12`) | BSD-3-Clause |
| libjpeg (`jpeg-9f`) | IJG |
| zlib (`1.3.1`) | zlib |
| curl (`8.15.0`) | curl |
| GameSpy SDK (`code/gamespy/`) | GameSpy SDK licence |
| libc++ (`app/thirdparty/libcxx-16k/`) | Apache-2.0 with LLVM exception |

The 16 KB-page-aligned `libc++_shared.so` in `app/thirdparty/libcxx-16k/` comes
from NDK r27; its symbol audit is in `app/thirdparty/libcxx-16k/PROVENANCE.md`.
