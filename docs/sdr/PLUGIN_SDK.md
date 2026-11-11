# Plugin SDK v0.1

SDK: [C header](../../sdk/include/sdr/plugin.h), [Rust binding/example](../../sdk/rust/src/lib.rs). Нативные библиотеки доверенные; public ABI 1.0 не равен ABI внутренних C++ классов. Операционная система/архитектура DLL/SO должны совпадать с хостом.

## Сборка независимых примеров

Нужны CMake ≥3.22, Ninja и компилятор C++20. Для Windows x64 откройте Developer PowerShell for Visual Studio 2026. Для Linux x86-64 используйте GCC из [утверждённой матрицы](ADR-013-desktop-platforms.md). Для сборки расширений достаточно каталога `sdk/include` и исходников выбранного примера. Qt и библиотеки приложения не требуются.

Из корня репозитория:

```sh
cmake -S plugins/source-example -B build/example-source -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDR_SDK_INCLUDE="$PWD/sdk/include"
cmake --build build/example-source
cmake -S plugins/demod-example -B build/example-demod -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDR_SDK_INCLUDE="$PWD/sdk/include"
cmake --build build/example-demod
```

Каждая сборка создаёт соседние библиотеку (`.dll` или `.so`) и manifest (`source-example.json` или `demod-example.json`). При переносе копируйте пару файлов. Source выдаёт комплексный тон 1 кГц. Demodulator извлекает AM-огибающую при нулевом смещении и частоте аудио 48 кГц. Для прослушивания устойчивого тона с этим демодулятором подайте AM-модулированный IQ. При совместном запуске двух примеров Source имеет постоянную огибающую, поэтому звук Demodulator после переходного участка затихает.

## Rust

Crate `sdk/rust` предоставляет Rust-представления структур C ABI и собирает Source-пример без внешних crates. На Windows используйте target `x86_64-pc-windows-msvc` в том же Developer PowerShell. На Linux используйте `x86_64-unknown-linux-gnu`.

```sh
cargo test --locked --offline --manifest-path sdk/rust/Cargo.toml --target-dir build/rust
cargo build --locked --offline --release --manifest-path sdk/rust/Cargo.toml --target-dir build/rust
```

На Linux скопируйте `sdk/rust/rust-source.json` рядом с `build/rust/release/libsdr_plugin_api.so`. На Windows разместите копию manifest рядом с `build/rust/release/sdr_plugin_api.dll` и задайте в ней `"platform": "Windows"` и `"library": "sdr_plugin_api.dll"`.

Для проверки FFI передайте тестовому хосту три абсолютных пути:

```text
sdr_core_tests <source-library> <demod-library> <rust-library>
```

Тестовый хост создаёт, настраивает и вызывает экземпляры через ABI, проверяет обработку ошибки конфигурации и время жизни Rust-модуля. Тесты Cargo отдельно проверяют инициализацию Rust ABI и вызов Source. Результаты на обеих платформах приведены в [протоколе SDK](PLUGIN_SDK_VERIFICATION.md).

## Установка и отключение

1. Остановите RX и в панели расширений выберите «Добавить manifest…». Укажите соседний JSON доверенного модуля. Повторите для второго примера.
2. Выберите `Example tone source` как источник и демодулятор `org.sdr.example.demod` в списке режимов. Установите смещение 0 и запустите RX. Спектр показывает IQ от Source, аудиобуфер получает результат Demodulator.
3. Остановите RX, затем нажмите «Отключить расширения». Оба пункта исчезнут из списков. После этого библиотеки можно заменить и снова загрузить их manifest.

Хост отклоняет выгрузку во время RX. При отказе в регистрации проверьте JSON, ABI, архитектуру, ID и имя соседней библиотеки. Нативный код исполняется внутри процесса с правами приложения. Сбой нативного кода может завершить приложение.

## Manifest

Source-плагин v1.0 — источник IQ: один manifest регистрирует один источник, конфигурация содержит sample rate/center/offset. Этот ABI сохранён без изменений. Для полноценного аппаратного расширения используйте отдельный [Device Provider API](DEVICE_PLUGIN_API.md): enumeration, capabilities, gain/PPM/Bias-T/direct sampling и actual config. Модули HackRF/RTL-SDR/Airspy используют новый контракт; хост не содержит списка семейств. Внешние Soapy-драйверы и модули приложения — разные уровни расширения; подробности в ADR-017 и GLOSSARY.md.

Обязательные поля: id, name, version, api_major, api_minor, kind (`source` или `demodulator`), platform (`Windows`/`Linux`), arch (`x86_64`), license, library (только имя соседнего файла). Абсолютные пути и переходы в library не принимаются. Дубликаты id, несовместимые версии, неизвестный kind и другая платформа отклоняются до регистрации. `api_minor` — минимально требуемая minor версия, в v1.0 это 0. Версия плагина независима от версии ABI.

## Жизненный цикл

`sdr_plugin_init(major, minor, out_api)` получает занулённую структуру с установленным struct_size. При несовместимом major возвращает SDR_ERROR. Хост проверяет размер, версию, id, kind и обязательные callbacks до создания instance. Расширения minor не читаются без согласования размера; v1.0 не содержит необязательных расширений.

После create вызывается configure с source rate/center, output audio rate и VFO offset. Source получает read с заранее заданной вместимостью и возвращает число samples/flags. Demodulator получает borrowed IQ, borrowed выход audio и capacities; возвращённый count не превышает вместимость. Вход демодулятора — source IQ до DDC. Все возвращаемые значения должны быть finite.

Память instance выделяет и освобождает сам плагин. Host-owned вход/выход сохранять после вызова нельзя. Строка id должна жить до выгрузки библиотеки. В v1.0 нет фоновых потоков, удерживаемых callbacks, асинхронного process или самостоятельного GUI. Все callbacks одного instance последовательны и выполняются одним worker. Исключения C++/panic Rust должны завершаться внутри FFI и преобразовываться в код ошибки.

Source instance живёт в acquisition worker, demodulator instance — в DSP worker. Instance удерживает библиотеку shared ownership; destroy вызывается до фактической выгрузки. Для отключения используемого source/demodulator сначала stop RX. Ошибка init не оставляет зарегистрированный модуль. Нарушение контракта нативным модулем не изолируется хостом.

## Совместимость

Заголовок использует фиксированные целочисленные типы, C calling convention и opaque void* handles. STL, Qt-типы, C++ exceptions и Rust ABI через границу не передаются. Возможность будущего IPC не означает, что v1.0 уже является протоколом удалённого исполнения. Для новых callbacks потребуется согласованная эволюция ABI/struct_size и соответствующие тесты.
