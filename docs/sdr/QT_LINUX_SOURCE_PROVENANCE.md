# Исходники Qt для SDR Next 0.1 Linux Preview

Linux preview собран с динамическим Qt 6.10.0 на Ubuntu 22.04.5 x86-64, GCC 11.4.0. Superproject и модули `qtbase`, `qtdeclarative`, `qtmultimedia`, `qtshadertools` и `qtsvg` закреплены в [dependencies.lock.json](../../dependencies.lock.json). Исходники Qt перед сборкой сверены с lock. Изменения проекта в исходники Qt не вносились.

## Сборка SDK

В архиве исходников каталог `qt-source` содержит superproject и пять модулей. Понадобятся CMake, Ninja, GCC и заголовки X11/XCB, xkbcommon, Wayland, EGL/OpenGL, fontconfig и PulseAudio. Для XCB необходимы также соответствующие runtime-библиотеки. При использовании локального sysroot передайте его каталоги заголовков и библиотек через `CMAKE_INCLUDE_PATH` и `CMAKE_LIBRARY_PATH` и укажите его `PKG_CONFIG_PATH`. В эталонной сборке `PulseAudio_DIR` указывал на CMake config пакета `libpulse-dev` в sysroot.

В WSL запускайте конфигурацию с Linux-каталогами в `PATH`. Если локальный sysroot подставляет статические архивы вместо системных `libxcb.so`, `libxcb-render.so` и `libxcb-shm.so`, передайте пути к этим трём системным библиотекам через `XCB_XCB_LIBRARY`, `XCB_RENDER_LIBRARY` и `XCB_SHM_LIBRARY`. Эталонная сборка использовала файлы из `/usr/lib/x86_64-linux-gnu` для этих трёх входов.

```sh
cmake -S qt-source -B qt-build -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$PWD/qt-sdk" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DFEATURE_xcb=ON -DFEATURE_wayland_client=ON -DFEATURE_pulseaudio=ON \
  -DQT_FEATURE_ffmpeg=OFF -DQT_GENERATE_SBOM=ON
cmake --build qt-build --parallel 5
cmake --install qt-build
```

В эталонной сборке использованы release-флаги `-DNDEBUG -O2`. `local-ubuntu-build-packages.tsv` в архиве перечисляет пакеты, загруженные для локального sysroot, и их версии. Этот список дополняет системные зависимости Ubuntu. Итоговую конфигурацию можно сверить с `qt-config-summary.txt` в архиве и с Qt SPDX в бинарной поставке.

Сведения о коде X.Org из Ubuntu, статически вошедшем в XCB-компоненты Qt, приведены в `THIRD_PARTY_NOTICES.md` и [отчёте аудита](MVP_V0_1_VERIFICATION.md#предварительный-аудит-linux-preview). При публикации архивов нужно подтвердить предоставление соответствующих исходников для всей поставки. [Условия LGPL по документации Qt](https://www.qt.io/development/open-source-lgpl-obligations)
