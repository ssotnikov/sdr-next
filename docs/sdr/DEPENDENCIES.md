# Зависимости и правила поставки

Дата: 2026-11-07. ADR-007 сохраняется. Включение зависимости в эту таблицу не является лицензионным заключением о произвольной сборке.

| Компонент | Владелец/upstream | Версия | Использование/сборка | Лицензионный контроль |
|---|---|---|---|---|
| Собственное ядро, GUI, SDK | sdr-app | 0.1.0 | CMake, C++20 | MIT, LICENSE |
| Qt Core/Gui/Qml/Quick/QuickControls2/Multimedia | Qt Project | 6.10.0 | Динамический SDK; Linux-compatible сборка из исходников | Проверить LICENSES/third-party каждого фактически поставленного модуля, сохранить LGPL/permissive notices и требования замены библиотек |
| SoapySDR | pothosware/SoapySDR | ABI 0.8; проверен Radioconda 0.8.1 py312hd5eb7cc_5 | Только optional Device Provider модули: внешние headers через find_path, runtime по C ABI. Core/GUI не зависят от SDK/runtime | BSL-1.0 upstream, лицензии заголовков и фактической библиотеки сверять при упаковке |
| Аппаратные Soapy-модули | upstream RTL-SDR/HackRF/Airspy | Предоставленная пользователем версия | Внешние установленные драйверы, не копируются автоматически в пакет | Отдельная проверка лицензий каждого модуля/USB SDK; не считать GPL-драйверы частью MIT приложения |
| CMake | Kitware | 3.22 minimum, локальный Windows 3.31.6 | Build only | BSD-3-Clause; build tools не входят в runtime-пакет |
| Ninja | ninja-build / Kitware package | 1.13.0.git.kitware.jobserver-pipe-1 | Build only | Apache-2.0; исполняемые build tools не поставляются |
| Rust/Cargo | Rust project | 1.89.0 | Только bindings/example | MIT/Apache-2.0, std-only, без crates.io runtime-зависимостей |
| MinGW/GCC | GNU / mingw-w64 / winpthreads | 13.1.0, Qt tool package mingw1310_64 | Дополнительный Windows preview toolchain, не утверждённый MSVC gate | GCC GPL с runtime exception; mingw-w64/winpthreads собственные notices; runtime texts копируются из tool package, окончательный аудит отдельно |

В реальном Windows RX использованы SoapyHackRF 0.3.4 и libhackrf 2024.02.1 из Radioconda, firmware HackRF 2024.02.1/API 1.08/r9. Эти DLL и их транзитивные компоненты не включаются в preview; путь к установленному runtime задаётся пользователем. Qt deployment также добавляет QmlModels/WorkerScript/Meta, Network, OpenGL, Svg, QuickLayouts, QuickDialogs2 и выбранные style/implementation DLL: это зависимости фактической поставки, которые входят в финальный лицензионный аудит вместе с Qt SBOM. Сборочный QuickTest не поставляется.

## Обязательные действия

1. Перед добавлением компонента указать upstream, точную версию/commit, способ сборки, применимые лицензии и необходимость включения в runtime.
2. Проверять транзитивные зависимости и Qt deployment output, а не только этот список или vcpkg manifest.
3. Формировать инвентарь/SBOM и LICENSE/NOTICE для конкретного пакета. Не включать драйверы, Qt tools, debug output и непроверенные плагины простым рекурсивным копированием окружения разработчика.
4. Хранить build/install/download caches вне tracked-файлов. Непроверенные условия распространения блокируют окончательный G6.
5. При изменении зависимостей повторять аудит затронутых компонентов. Собственная MIT-лицензия не меняет лицензии Qt или внешних SDR-драйверов.

## Шрифты интерфейса

IBM Plex Sans (Regular, Medium, SemiBold) и IBM Plex Mono (Regular, Medium) — неизменённые TTF из пользовательского brand book, upstream IBM. SHA-256: assets/fonts/README.md. Лицензия SIL OFL 1.1: assets/fonts/OFL.txt; CMake install поставляет её в licenses/IBM-Plex. Точные версии доступны во встроенной metadata файлов.
