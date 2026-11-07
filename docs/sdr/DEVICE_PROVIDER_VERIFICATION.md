# Проверка независимых модулей устройств

Дата: 2026-11-07. Изменение OpenSpec: `modular-device-providers`, ADR-017. Область: новый аппаратный контракт, загрузка, совместимость и Windows-поставка; полная приёмка MVP остаётся отдельной.

## Результаты

- Чистая сборка `build/windows-device-msvc2026`: VS 2026 18.11.12328.437, MSVC 19.51.36260.0, toolset 14.51.36231, SDK 10.0.26100.0, Qt 6.10.0 msvc2022_64. CTest **7/7 pass**, включая физический HackRF-сценарий при `SDR_TEST_HACKRF=1`.
- Отдельный QML прогон: **6 pass, 0 fail, 0 skip**. HackRF через `org.sdr.device.hackrf`: 7 372 800 IQ, 41 спектральный кадр, Fs 2.4 MSPS, gain 16; IQ gaps/overflows и audio buffer/backend underruns/drops — 0. Это короткая проверка, не восьмичасовая приёмка.
- CLI с явным `sdr_device_hackrf.dll`, WFM, 10 s: 24 000 000 IQ, 479 999 audio samples, IQ gaps 0, elapsed 10.0498 s. `acceptance=not_evaluated` сохранено.
- Headless Windows/MinGW с `SDR_BUILD_GUI=OFF`, `SDR_ENABLE_SOAPY=OFF`: **2/2 pass** (core, device-providers); хост/SDK работают без Qt и Soapy SDK.
- `plugins/device-example` независимо собран MSVC без Qt/Soapy в `build/device-sdk-independent`; C++ host успешно вызвал полученную DLL с одиннадцатью отрицательными ABI fixtures. Неизвестный приложению provider обнаружен Engine без пересборки хоста.
- Прежний Source/Demodulator C ABI не изменён: core tests вызывают native Source/Demod и Rust Source DLL, собранную для `x86_64-pc-windows-msvc`; pass.
- Оба пакета `dist/sdr-app-0.1-preview-device-modules-msvc2026` и `dist/sdr-app-0.1-preview-no-device-modules-msvc2026` выполнили `smoke-windows.ps1 -ReceiveSynthetic` с минимальным Windows PATH: native GUI обработал ≥2.4 M IQ и >20 спектральных кадров, exit 0. Первый содержит три пары manifest/DLL, второй не содержит папку devices. Soapy runtime не включён в пакеты.
- Найден и исправлен неверный байтовый префикс локализованного MSVC `/showIncludes`, из-за которого Ninja мог сохранять устаревшие объекты после изменения заголовков. Регрессия `msvc-dependencies` меняет header и проверяет результат повторной сборки.

Локальные логи находятся в ignored `build/`: device-final-build.log, device-final-tests.log, device-hackrf-qml.txt, device-headless-build.log, device-independent-build.log, device-package*.log. Логи/записи/серийные номера оборудования и бинарные артефакты не включаются в Git.

## Покрытие спецификации

| Требование и сценарий | Реализация | Свидетельство |
|---|---|---|
| Независимые модули: Новый поставщик | `sdk/include/sdr/device_plugin.h`, `app/engine.cpp` registry | `provider-engine`: SDK-only provider с неизвестным ID, два экземпляра; независимая сборка example |
| Независимые модули: Отдельные семейства | `plugins/devices/CMakeLists.txt`, entry/backend | Три отдельные DLL/manifest в build и пакете; в core/Engine нет семейств и Soapy |
| Работа без модулей: Пустая папка | `core/src/source.cpp`, Engine::scan | `provider-engine`: synthetic RX и file EOF без папки; native smoke пакета без модулей |
| Работа без модулей: Недоступная зависимость | DeviceModule, локальный catch Engine::scan | `provider-engine`: enumeration/dependency error и missing DLL рядом с исправным provider; полный пакет без runtime продолжает synthetic RX |
| Единый контракт: Возможности и применение | DeviceSource, controller/Main.qml | `device-providers`: per-instance caps/requested/actual/IQ; `provider-engine`: actual config; QML: отключённые настройки обнуляются, реальные HackRF параметры |
| Единый контракт: Ошибка конфигурации | DeviceSource/create и RAII backend | Отрицательные config/actual tests; неподдержанные параметры отклонены; код host уничтожает instance при ошибочном actual |
| Контролируемая загрузка: Повреждённый рядом с исправным | Engine::scan и DeviceModule | `provider-engine`: 8 локальных ошибок, включая manifest/ABI/path/platform/duplicate/dependency; валидный provider остаётся доступен; SDK tests проверяют 11 плохих библиотек |
| Жизненный цикл: Обновление во время RX | Engine stopped-only scan | `provider-engine`: отказ без разрушения текущего instance/RX |
| Жизненный цикл: Удаление модуля | shared module ownership, stop/join/destroy | `device-providers`: instance удерживает DLL; `provider-engine`: stop/remove/re-add/reopen |
| Совместимость: Отсутствует сохранённый источник | Main.qml::sourceIndex | QML: exact/legacy/ambiguous/missing ID; нет автоматической подмены; прежние native/Rust SDK вызовы pass |

## Проверка полноты и согласованности

Реализованы 6 требований и покрыты 10 сценариев этого изменения. Все 7 задач выполнены. Код следует ADR-017: семейства находятся только в модулях, единую границу задаёт C POD ABI, приложение использует возможности и actual параметры, экземпляр удерживает библиотеку. Прежний ABI и встроенные software-источники сохранены. Критических расхождений в проверенной области не обнаружено.

**Ограничения:** текущая Linux-сборка не проверена: запуск WSL зависал до выполнения команд сборки. Старые Linux результаты не относятся к текущей ревизии. Windows 10, полная аппаратная настройка/unplug/reopen, длительный RX, измерения SLO и чистые машины остаются gates родительского MVP. Реальные RTL-SDR/Airspy испытания отложены, статус experimental/unverified. Native crash isolation не реализована; доверенный модуль работает в процессе приложения. Не заявляется устойчивость к произвольному повреждению памяти или зависанию стороннего кода.

Полная приёмка MVP не завершена: родительский `plan-mvp-v0-1` сохраняет **27/54** выполненных задач и остаётся активным. Исходная baseline не изменялась; открытые решения не переводились в согласованные автоматически.
