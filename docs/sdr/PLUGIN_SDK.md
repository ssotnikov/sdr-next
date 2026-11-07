# Plugin SDK v0.1

SDK: [C header](../../sdk/include/sdr/plugin.h), [Rust binding/example](../../sdk/rust/src/lib.rs). Нативные библиотеки доверенные; public ABI 1.0 не равен ABI внутренних C++ классов. Операционная система/архитектура DLL/SO должны совпадать с хостом.

## Сборка независимых примеров

```sh
cmake -S plugins/source-example -B build/example-source -DSDR_SDK_INCLUDE="$PWD/sdk/include"
cmake --build build/example-source --config Release
cmake -S plugins/demod-example -B build/example-demod -DSDR_SDK_INCLUDE="$PWD/sdk/include"
cmake --build build/example-demod --config Release
```

Source создаёт комплексный тон 1 kHz; Demodulator — учебный AM-envelope пример с нулевым offset, не замена встроенному канальному фильтру. Оба используют только SDK header, не внутренние заголовки приложения. CMake создаёт JSON manifest рядом с бинарником. В GUI остановите RX и вручную добавьте manifest.

Rust (без сторонних crates):

```sh
CARGO_TARGET_DIR=build/rust cargo test --manifest-path sdk/rust/Cargo.toml
CARGO_TARGET_DIR=build/rust cargo build --release --manifest-path sdk/rust/Cargo.toml
```

Для Windows/MSVC используйте штатный native Rust target и MSVC 2022. Дополнительный GNU target собирается с `--target x86_64-pc-windows-gnu` и соответствующим linker. У Rust-примера manifest-шаблон Linux; для Windows замените platform на `Windows`, library на `sdr_plugin_api.dll`, скопируйте manifest рядом с DLL.

Проверка реального вызова Rust из C++ host: `sdr_core_tests <source-library> <demod-library> <rust-library> [bad-library]`. Пути из собственной сборки, не посторонние DLL. Тест создаёт instance, вызывает read и уничтожает его до выгрузки библиотеки.

## Manifest

Source-плагин v1.0 — источник IQ, а не полный поставщик устройств: один manifest регистрирует один источник, конфигурация содержит sample rate/center/offset. В ABI нет перечисления устройств и полного capability/control API для gain, PPM, Bias-T и direct sampling. Встроенный Soapy-адаптер использует отдельный путь и сейчас перечисляет `hackrf`, `rtlsdr`, `airspy` явно. Внешние Soapy-драйверы и собственные Source-плагины — разные уровни расширения; подробности в ADR-016 и GLOSSARY.md.

Обязательные поля: id, name, version, api_major, api_minor, kind (`source` или `demodulator`), platform (`Windows`/`Linux`), arch (`x86_64`), license, library (только имя соседнего файла). Абсолютные пути и переходы в library не принимаются. Дубликаты id, несовместимые версии, неизвестный kind и другая платформа отклоняются до регистрации. `api_minor` — минимально требуемая minor версия, в v1.0 это 0. Версия плагина независима от версии ABI.

## Жизненный цикл

`sdr_plugin_init(major, minor, out_api)` получает занулённую структуру с установленным struct_size. При несовместимом major возвращает SDR_ERROR. Хост проверяет размер, версию, id, kind и обязательные callbacks до создания instance. Расширения minor не читаются без согласования размера; v1.0 не содержит необязательных расширений.

После create вызывается configure с source rate/center, output audio rate и VFO offset. Source получает read с заранее заданной вместимостью и возвращает число samples/flags. Demodulator получает borrowed IQ, borrowed выход audio и capacities; возвращённый count не превышает вместимость. Вход демодулятора — source IQ до DDC. Все возвращаемые значения должны быть finite.

Память instance выделяет и освобождает сам плагин. Host-owned вход/выход сохранять после вызова нельзя. Строка id должна жить до выгрузки библиотеки. В v1.0 нет фоновых потоков, удерживаемых callbacks, асинхронного process или самостоятельного GUI. Все callbacks одного instance последовательны и выполняются одним worker. Исключения C++/panic Rust должны завершаться внутри FFI и преобразовываться в код ошибки.

Source instance живёт в acquisition worker, demodulator instance — в DSP worker. Instance удерживает библиотеку shared ownership; destroy вызывается до фактической выгрузки. Для отключения используемого source/demodulator сначала stop RX. Ошибка init не оставляет зарегистрированный модуль. Нарушение контракта нативным модулем не изолируется хостом.

## Совместимость

Заголовок использует фиксированные целочисленные типы, C calling convention и opaque void* handles. STL, Qt-типы, C++ exceptions и Rust ABI через границу не передаются. Возможность будущего IPC не означает, что v1.0 уже является протоколом удалённого исполнения. Для новых callbacks потребуется согласованная эволюция ABI/struct_size и соответствующие тесты.
