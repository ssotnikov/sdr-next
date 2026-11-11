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

## Исходные пакеты Ubuntu для X.Org

Расширенный архив `sdr-next-0.1-preview-20261111-linux-qt-xorg-source.tar.gz` содержит исходники Qt и оригинальные исходные пакеты Ubuntu в каталоге `ubuntu-xorg-source/packages/`. Использованы версии `libx11 2:1.7.5-1ubuntu0.4`, `libxcb 1.14-3ubuntu3` и `xcb-util 0.4.0-1build2`. Файл `ubuntu-xorg-source/static-inputs.tsv` связывает восемь входных библиотек с dev-пакетами и их исходниками и содержит SHA-256 библиотек. [Ubuntu libxcb](https://launchpad.net/ubuntu/+source/libxcb/1.14-3ubuntu3)

Из каталога `ubuntu-xorg-source/packages/` проверьте и распакуйте поставленные исходники:

```sh
sha256sum -c ../SHA256SUMS
dpkg-source -x libx11_1.7.5-1ubuntu0.4.dsc
dpkg-source -x libxcb_1.14-3ubuntu3.dsc
dpkg-source -x xcb-util_0.4.0-1build2.dsc
```

Для повторного получения используйте `apt-get source --download-only libx11=2:1.7.5-1ubuntu0.4 libxcb=1.14-3ubuntu3 xcb-util=0.4.0-1build2` с подписанными источниками `deb-src` Ubuntu Jammy (`main`, `jammy-updates`, `jammy-security`). При распаковке `dpkg-source` может сообщить об отсутствии ключей отдельных авторов загрузок. При получении через APT проверены подписи индексов Ubuntu и контрольные суммы файлов по этим индексам.

Сборка Ubuntu-пакетов использует зависимости из их `debian/control`. В частности, `libxcb` генерирует код протоколов с помощью `xcb-proto`. Этот архив сохраняет исходные пакеты и патчи, использованные как основание для сверки. Полное восстановление окружения сборки Ubuntu-пакетов и повторная компиляция из архива пока не проверены.

## Уведомления в бинарном пакете

После `cmake --install` для Linux portable и до создания tar.gz запустите из корня SDR Next:

```sh
curl --fail --location \
  https://raw.githubusercontent.com/apache/tika/dbcbb368e0a913e1a74cca61347fd6ce24bc6d7e/NOTICE.txt \
  --output tika-NOTICE.txt
python3 scripts/collect-linux-qt-notices.py /path/to/staged /path/to/qt-source tika-NOTICE.txt
```

Используйте дерево исходников, из которого собран SDK. Скрипт рассчитан на формат tag/value SPDX закреплённой сборки Qt. Он проверяет контрольные суммы связанных SPDX, сверяет идентификаторы и выражения лицензий с исходными записями и проверяет SHA-256 файла Apache Tika. Результат появится в новом каталоге `licenses/qt-attributions/`. Для повторной упаковки используйте свежий staging.
