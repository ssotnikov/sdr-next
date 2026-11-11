# Сборка и проверка

## Среда

C++20, CMake ≥3.22, Qt **6.10.0** Core/Gui/Qml/Quick/QuickControls2/Multimedia. Согласованная матрица — ADR-013. Qt задаётся через `CMAKE_PREFIX_PATH`, без машинных путей в tracked presets. Скачать SDK можно официальным Qt installer или aqtinstall; download/build caches находятся вне репозитория.

SoapySDR headers ABI 0.8 обнаруживаются через `CMAKE_PREFIX_PATH`/`SDR_SOAPY_INCLUDE_DIR` только при сборке optional модулей устройств. Runtime загружается внутри модулей по C ABI; драйверы устанавливаются отдельно. `SDR_ENABLE_SOAPY=OFF` или отсутствие SDK исключает три Soapy provider из сборки, сохраняя хост, software RX и SDK-пример. Core/GUI не содержат Soapy-кода и списка семейств. Для Windows-сборки используется MSVC из Visual Studio 2026. Локальная MinGW-сборка — дополнительное свидетельство, не замена MSVC gate.

## Ручная установка или дополнение MSVC

Выберите установленную Visual Studio 2026 или Build Tools 2026 в **Visual Studio Installer → Modify / Изменить**. Добавьте **Desktop development with C++**, инструменты **MSVC x64/x86** (v145) и Windows SDK. CMake и Ninja можно установить в составе C++ CMake tools for Windows.

Откройте **Developer PowerShell for VS 2026** для x64 и проверьте `cl /Bv`. Скрипт `scripts/build.ps1` находит установку 2026 через `vswhere` и загружает окружение самостоятельно. Официальный комплект Qt 6.10.0 для MSVC по-прежнему называется `msvc2022_64`. [Установка инструментов MSVC](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc?view=msvc-180)

Сборка расширений отдельно от приложения описана в [Plugin SDK Guide](sdr/PLUGIN_SDK.md).

## Headless (Qt не требуется)

```sh
cmake -S . -B build/core -DSDR_BUILD_GUI=OFF -DSDR_ENABLE_SOAPY=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release --parallel 4
ctest --test-dir build/core -C Release --output-on-failure
```

Тесты проверяют порядок/ограничение очереди, нормировку и bins FFT, контрольный AM/NFM/WFM тон, RIFF/CF32 round-trip, EOF/повреждение данных, загрузку/отклонение ABI и удержание библиотеки живым instance.

## Desktop

```sh
cmake -S . -B build/desktop -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.0/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop --config Release --parallel 4
ctest --test-dir build/desktop -C Release --output-on-failure
build/desktop/bin/sdr_app --smoke-test
```

В Windows используйте Developer PowerShell для Visual Studio 2026 и Qt msvc2022_64. У много-конфигурационного генератора binaries находятся в `bin/Release/`; у Ninja/Make — в `bin/`. При запуске из build-каталога Qt/компиляторные DLL должны быть доступны в PATH. Упаковка — отдельный шаг, SDK не заменяет deployed package.

`sdr_engine_tests` проверяет источник → DSP → запись → файловый RX, metadata count/complete, отклонение перенастройки при записи, duplicate plugin identity и отключение расширений. `--smoke-test` открывает GUI и выходит через 2 секунды; сам по себе не тестирует долгий RX/аудио.

`sdr_settings_tests` проверяет восстановление значений и сохранность повреждённого/неподдерживаемого JSON. `sdr_ui_tests` запускает настоящий Main.qml, RX synthetic, несколько кадров spectrum/waterfall, zoom, темы, RU/EN и перестановку панели. Он использует offscreen из Qt SDK; такой backend не заявляется частью Windows runtime-поставки.

Модули устройств собираются в `bin/devices`: отдельные HackRF/RTL-SDR/Airspy DLL/SO и manifest. Хост сканирует только `devices` рядом с executable. `sdr_device_tests` проверяет SDK и отрицательные ABI fixtures, `sdr_provider_engine_tests` — discovery, отсутствие модулей, локальные ошибки, actual config и stop/remove/re-add. `SDR_TEST_HACKRF=1` включает физический HackRF-сценарий QML; без устройства его не следует выдавать за выполненный.

Последняя сборка модулей: `build/windows-device-msvc2026`, MSVC 2026, 7/7 CTest. `cmake/MsvcDependencies.cmake` определяет фактический байтовый префикс локализованного `/showIncludes` для Ninja; тест `msvc-dependencies` изменяет header и проверяет пересборку зависимого объекта. В старом `build/windows-msvc2026` был обнаружен некорректный префикс и устаревшие объекты: для нового кода используется свежая папка сборки.

Переносимый пакет: `./scripts/package-windows.ps1 -QtRoot <Qt SDK> -BuildDirectory build/desktop -OutputDirectory dist/<new-name>`. Проверка с очищенным developer PATH: `./scripts/smoke-windows.ps1 -PackageDirectory dist/<new-name>`. `Start-SDR.cmd` явно выбирает Windows platform plugin; `qt.conf` закрепляет относительные пути. Внешний Soapy runtime задаётся `SDR_SOAPY_RUNTIME` (папка с SoapySDR.dll и зависимостями). Не устанавливайте `QT_QPA_PLATFORM=offscreen` для пользовательского пакета: он поставляет `windows`, а offscreen используется только в SDK-тестах.

Для пакета без аппаратных модулей добавьте `-WithoutDeviceModules`. Для проверки фактического software RX используйте `smoke-windows.ps1 -ReceiveSynthetic`: native Windows GUI обрабатывает минимум 2.4 M IQ и обновляет спектр, затем завершается; это отдельная проверка от длительной/audio приёмки. SDK device-пример помещается в `examples/device`, без автозагрузки.

### Портативный пакет Linux

Используйте Qt 6.10.0 из закреплённых исходников, собранный на Ubuntu 22.04 с платформенными плагинами XCB и Wayland. Для установки Qt runtime и QML нужен `patchelf` в PATH. Укажите каталог Qt SDK и, для сборки аппаратных модулей, префикс SDK SoapySDR. Выберите новую папку для установки.

```sh
cmake -S . -B build/linux-portable -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSDR_INSTALL_LINUX_PORTABLE=ON \
  '-DCMAKE_PREFIX_PATH=/opt/sdr-deps/qt-sdk;/opt/sdr-deps/installed/x64-linux-dynamic'
cmake --build build/linux-portable --parallel 4
ctest --test-dir build/linux-portable --output-on-failure
cmake --install build/linux-portable --prefix "$PWD/dist/sdr-next-0.1-preview-linux-x86_64"
tar -C dist -czf dist/sdr-next-0.1-preview-linux-x86_64.tar.gz sdr-next-0.1-preview-linux-x86_64
```

Распакуйте архив в новую папку. Запустите `bin/sdr_app` из распакованного дерева. Для быстрой проверки окна и синтетического IQ запустите `bin/sdr_app --smoke-rx` без путей Qt SDK в окружении. `bin/portable.txt` включает сохранение настроек в `bin/sdr-settings.json`. Модули устройств находятся в `bin/devices`, примеры SDK в `examples`. Для аппаратного RX установите SoapySDR ABI 0.8 и драйвер устройства в системе и проверьте USB-права. Перед распространением пакета проверьте его на утверждённых чистых системах и сверьте фактический состав с лицензиями.

Для запуска через X11 на Ubuntu 22.04 проверьте наличие системных пакетов `libxcb-cursor0` и `libxkbcommon-x11-0`. Они предоставляют `libxcb-cursor.so.0` и `libxkbcommon-x11.so.0`, которые загружает Qt XCB. При сборке самого Qt нужны также `libxcb-cursor-dev`, `libxkbcommon-x11-dev` и заголовки XCB. Если пакеты распакованы вручную в локальный sysroot, распакуйте вместе с `-dev` пакетом соответствующий runtime: без него ссылка `libxkbcommon-x11.so` в sysroot не разрешается при линковке. Полный перечень runtime-зависимостей определяется по итоговому архиву.

Для аудита сохраняйте SPDX из фактически использованного Qt SDK. Установка Linux-пакета копирует доступные файлы из `<Qt SDK>/sbom` в `licenses/qt-sbom`. Сверяйте пути из SPDX с составом итогового архива. Qt deployment может изменить RUNPATH, поэтому контрольные суммы архивируемых файлов вычисляйте после установки. Уведомления в `licenses/linux-qt` относятся к статическим входам XCB в эталонной сборке на Ubuntu 22.04.

Независимая сборка нового provider без Qt и Soapy: `cmake -S plugins/device-example -B build/device-sdk-independent -G Ninja -DCMAKE_BUILD_TYPE=Release`, затем `cmake --build build/device-sdk-independent`. Установите полученные manifest и библиотеку в applicationDir/devices и обновите список при остановленном RX. Подробный контракт — [Device Provider API](sdr/DEVICE_PLUGIN_API.md).

## Linux GUI в контейнере

```sh
docker build -t sdr-app-qt:6.10.0 -f ci/Dockerfile.qt .
docker run --rm -v "$PWD:/src" sdr-app-qt:6.10.0 sh -c 'cmake -S . -B build/linux-gui -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/linux-gui -j 4 && ctest --test-dir build/linux-gui --output-on-failure && xvfb-run -a build/linux-gui/bin/sdr_app --smoke-test'
```

PowerShell bind mount: `-v D:/Projects/sdr-app:/src`. Контейнер Ubuntu 24.04 проверяет сборку, программные сценарии и запуск под виртуальным X11/software. Он не подтверждает физические USB, Wayland, iGPU или слышимость звука. Для портативной Linux-поставки на Ubuntu 22.04 нужен отдельно проверенный Qt и runtime по ADR-013.

## Контрольные данные и измерения

```sh
build/core/bin/sdr_bench --rate 2400000 --seconds 1 --mode WFM --write-iq build/tone.cf32
build/desktop/bin/sdr_bench --module build/desktop/bin/devices/sdr_device_hackrf.dll --rate 2400000 --seconds 10 --mode WFM
```

Первый вызов создаёт контрольный IQ 1 kHz и metadata; генерация определена формулами в `core/src/source.cpp`, чужие IQ-наборы не включаются. Второй использует реальный HackRF. Для 10/20 MSPS повторите с соответствующим rate; сохраняйте stdout/stderr вместе с описанием среды. Измеряемый throughput включает генерацию или ожидание hardware; это не CPU-only benchmark DSP и не прохождение критериев GUI/audio/burn-in.

В Linux имя библиотеки — `sdr_device_hackrf.so`. Без `--device` выбирается первый экземпляр явно указанного модуля; `--device <raw-key>` выбирает конкретный. Прежний самостоятельный аргумент `--device soapy:...` заменён `--module`; сохранённые GUI ID мигрируют отдельно, только при однозначном совпадении.

Максимальная длительность headless прогона — 28800 секунд. Если он выполняется без GUI/аудиоустройства, его результат не заменяет согласованный восьмичасовой end-to-end тест.

## OpenSpec и коммиты

Рабочий change: `plan-mvp-v0-1`. Отмечать задачу только после полного выполнения её условий. Проверка: `openspec validate plan-mvp-v0-1 --strict`. Непроверенные ОС, драйверы и performance остаются blocked, даже если код собран. Архивирование — после реализации и фактической приёмки, не после создания файлов.

Соглашения коммитов/атрибуции — CONTRIBUTING.md и AGENTS.md. Generated binaries, локальные настройки, recordings и caches в Git не включаются.

## Фиксированное получение зависимостей (G0, задача 1.4)

Машиночитаемые pins: `dependencies.lock.json` и `vcpkg.json`; решение — [ADR-023](sdr/ADR-023-pinned-dependency-acquisition.md). `vcpkg-configuration.json` подключает локальный port SoapySDR 0.8.1#1: он исправляет Linux-путь CMake package, сохраняя архив upstream 0.8.1. Скрипт требует Python 3.10+, Git и сеть. Для `--hardware` дополнительно нужны C++ toolchain, CMake, Ninja и системные build prerequisites. Кеши и SDK размещаются **вне** checkout, в постоянной папке (не `/tmp` для WSL). Скрипт не устанавливает компилятор, драйверы устройств и готовый Qt SDK.

Windows x86-64, Developer PowerShell for VS 2026:

```powershell
python scripts/fetch-dependencies.py --root D:/Tools/sdr-deps --qt-source --hardware
./scripts/build.ps1 -QtRoot D:/Tools/Qt/6.10.0/msvc2022_64 -SoapySdkPrefix D:/Tools/sdr-deps/installed/x64-windows -BuildDirectory build/pinned-windows
```

`QtRoot` — уже собранный SDK ровно Qt 6.10.0 (официальный msvc2022_64 либо сборка из полученных исходников). Если CMake/Ninja установлены отдельно, добавьте их в PATH. Bootstrap и `build.ps1` выбирают MSVC 2026 через `vswhere`. Директория dependency root должна быть выделенной: скрипт не удаляет чужие данные и отклоняет изменённый checkout.

Ubuntu 22.04/GCC 11 (системные prerequisites устанавливаются администратором):

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git python3 curl zip unzip tar pkg-config
env PATH=/usr/local/bin:/usr/bin:/bin python3 scripts/fetch-dependencies.py --root /opt/sdr-deps --qt-source --hardware
```

Выберите доступную для записи внешнюю папку вместо `/opt/sdr-deps`, если нет прав. Команда задаёт PATH только для своего процесса: в WSL это исключает медленный поиск Linux-программ среди унаследованных Windows-каталогов и случайный выбор Windows-инструментов. Глобальные настройки WSL не меняются. При нестандартной установке добавьте каталоги ваших Linux-инструментов в этот PATH. Linux-профиль — `x64-linux-dynamic`, поскольку port SoapySDR не поддерживает статическую сборку. После получения SDK Soapy находится в `installed/<triplet>/include`; runtime — отдельно в `bin` (Windows) или `lib` (Linux). Приложение динамически загружает SoapySDR из provider; драйверы SoapyHackRF/RTLSDR/Airspy нужно устанавливать отдельно, этот bootstrap их не получает и не подменяет действующее окружение RX.

### Qt 6.10.0 из исходников

`--qt-source` получает superproject и пять модулей ровно по commit/gitlinks из lock. Это **исходники**, не установленный Qt. Сборочные и графические prerequisites зависят от ОС; их отсутствие должно быть устранено до configure Qt. На Ubuntu потребуются, в частности, dev-пакеты OpenGL/EGL/X11/XCB/xkbcommon/fontconfig и аудио backend. Wayland и другие дополнительные backend требуют своих dev-пакетов и отдельной квалификации. Наличие source tree не означает прохождения этих проверок.

Пример out-of-source сборки (в Developer PowerShell MSVC либо Linux shell; замените пути):

```sh
cmake -S /opt/sdr-deps/qt-source -B /opt/sdr-deps/qt-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/sdr-deps/qt-sdk -DBUILD_SHARED_LIBS=ON -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DQT_FEATURE_ffmpeg=OFF
cmake --build /opt/sdr-deps/qt-build --parallel 2
cmake --install /opt/sdr-deps/qt-build
cmake -S . -B build/pinned-linux -G Ninja -DCMAKE_BUILD_TYPE=Release '-DCMAKE_PREFIX_PATH=/opt/sdr-deps/qt-sdk;/opt/sdr-deps/installed/x64-linux-dynamic'
cmake --build build/pinned-linux --parallel 2
ctest --test-dir build/pinned-linux --output-on-failure
```

Для общей Linux-поставки Qt собирается на Ubuntu 22.04 по ADR-013. Текущий CI-контейнер Ubuntu 24.04 с готовым Qt 6.10.0 остаётся дополнительной средой тестов; он не заменяет эту portable-сборку. В контейнере SoapySDR теперь получается через тот же manifest/baseline, без `libsoapysdr-dev` из apt.

Проверка получения и ограничения: [DEPENDENCY_ACQUISITION_VERIFICATION.md](sdr/DEPENDENCY_ACQUISITION_VERIFICATION.md). Обновление pins выполняется отдельным изменением с проверкой состава зависимостей; совпадение версии API 0.8 не разрешает автоматически обновлять runtime или аппаратные драйверы.

### Проверка полученного SDK

Независимый consumer в `tests/dependency-sdk` проверяет не только заголовки, но и импортированный CMake target, линковку, загрузку библиотеки, release 0.8.1 и ABI 0.8:

```sh
cmake -S tests/dependency-sdk -B build/dependency-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/sdr-deps/installed/x64-linux-dynamic
cmake --build build/dependency-sdk
ctest --test-dir build/dependency-sdk --output-on-failure
```

На Windows выполните команды в Developer PowerShell, замените prefix на `installed/x64-windows` и добавьте его `bin` в PATH для загрузки DLL тестом. Upstream добавляет build-info к версии библиотеки, поэтому consumer использует `find_package(SoapySDR 0.8.1 CONFIG REQUIRED)` и отдельно проверяет release/ABI через C API, вместо несовместимого с суффиксом `EXACT 0.8.1`.
