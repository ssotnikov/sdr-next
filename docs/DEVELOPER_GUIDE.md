# Сборка и проверка

## Среда

C++20, CMake ≥3.22, Qt **6.10.0** Core/Gui/Qml/Quick/QuickControls2/Multimedia. Согласованная матрица — ADR-013. Qt задаётся через `CMAKE_PREFIX_PATH`, без машинных путей в tracked presets. Скачать SDK можно официальным Qt installer или aqtinstall; download/build caches находятся вне репозитория.

SoapySDR headers ABI 0.8 обнаруживаются через `CMAKE_PREFIX_PATH`/`SDR_SOAPY_INCLUDE_DIR`. Runtime загружается по C ABI во время работы; драйверы устанавливаются отдельно. `SDR_ENABLE_SOAPY=OFF` исключает аппаратный адаптер. Для MSVC runtime/SDK требуется согласованный MSVC 2022. Локальная MinGW-сборка — дополнительное свидетельство, не замена MSVC gate.

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

В Windows используйте Developer PowerShell MSVC 2022 и Qt msvc2022_64. У много-конфигурационного генератора binaries находятся в `bin/Release/`; у Ninja/Make — в `bin/`. При запуске из build-каталога Qt/компиляторные DLL должны быть доступны в PATH. Упаковка — отдельный шаг, SDK не заменяет deployed package.

`sdr_engine_tests` проверяет источник → DSP → запись → файловый RX, metadata count/complete, отклонение перенастройки при записи, duplicate plugin identity и отключение расширений. `--smoke-test` открывает GUI и выходит через 2 секунды; сам по себе не тестирует долгий RX/аудио.

`sdr_settings_tests` проверяет восстановление значений и сохранность повреждённого/неподдерживаемого JSON. `sdr_ui_tests` запускает настоящий Main.qml, RX synthetic, несколько кадров spectrum/waterfall, zoom, темы, RU/EN и перестановку панели. Он использует offscreen из Qt SDK; такой backend не заявляется частью Windows runtime-поставки.

Переносимый пакет: `./scripts/package-windows.ps1 -QtRoot <Qt SDK> -BuildDirectory build/desktop -OutputDirectory dist/<new-name>`. Проверка с очищенным developer PATH: `./scripts/smoke-windows.ps1 -PackageDirectory dist/<new-name>`. `Start-SDR.cmd` явно выбирает Windows platform plugin; `qt.conf` закрепляет относительные пути. Внешний Soapy runtime задаётся `SDR_SOAPY_RUNTIME` (папка с SoapySDR.dll и зависимостями). Не устанавливайте `QT_QPA_PLATFORM=offscreen` для пользовательского пакета: он поставляет `windows`, а offscreen используется только в SDK-тестах.

## Linux GUI в контейнере

```sh
docker build -t sdr-app-qt:6.10.0 -f ci/Dockerfile.qt .
docker run --rm -v "$PWD:/src" sdr-app-qt:6.10.0 sh -c 'cmake -S . -B build/linux-gui -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/linux-gui -j 4 && ctest --test-dir build/linux-gui --output-on-failure && xvfb-run -a build/linux-gui/bin/sdr_app --smoke-test'
```

PowerShell bind mount: `-v D:/Projects/sdr-app:/src`. Контейнер Ubuntu 24.04 проверяет сборку, программные сценарии и запуск под виртуальным X11/software. Он не подтверждает физические USB, Wayland, iGPU или слышимость звука. Для портативной Linux-поставки на Ubuntu 22.04 нужен отдельно проверенный Qt и runtime по ADR-013.

## Контрольные данные и измерения

```sh
build/core/bin/sdr_bench --rate 2400000 --seconds 1 --mode WFM --write-iq build/tone.cf32
build/desktop/bin/sdr_bench --device soapy:driver=hackrf --rate 2400000 --seconds 10 --mode WFM
```

Первый вызов создаёт контрольный IQ 1 kHz и metadata; генерация определена формулами в `core/src/source.cpp`, чужие IQ-наборы не включаются. Второй использует реальный HackRF. Для 10/20 MSPS повторите с соответствующим rate; сохраняйте stdout/stderr вместе с описанием среды. Измеряемый throughput включает генерацию или ожидание hardware; это не CPU-only benchmark DSP и не прохождение критериев GUI/audio/burn-in.

Максимальная длительность headless прогона — 28800 секунд. Если он выполняется без GUI/аудиоустройства, его результат не заменяет согласованный восьмичасовой end-to-end тест.

## OpenSpec и коммиты

Рабочий change: `plan-mvp-v0-1`. Отмечать задачу только после полного выполнения её условий. Проверка: `openspec validate plan-mvp-v0-1 --strict`. Непроверенные ОС, драйверы и performance остаются blocked, даже если код собран. Архивирование — после реализации и фактической приёмки, не после создания файлов.

Соглашения коммитов/атрибуции — CONTRIBUTING.md и AGENTS.md. Generated binaries, локальные настройки, recordings и caches в Git не включаются.
