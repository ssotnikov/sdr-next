# Проверка получения зависимостей — G0 / 1.4

Дата: 2026-11-08. Относится к [ADR-023](ADR-023-pinned-dependency-acquisition.md) и OpenSpec `plan-mvp-v0-1`, задача 1.4. Реализованы pins, получение и проверка SDK на доступных Windows/Linux средах. В исходной проверке 2026-11-08 задача 1.4 оставалась открытой из-за требования MSVC 2022. После [уточнения матрицы](ADR-013-desktop-platforms.md) 2026-11-11 задача 1.4 подтверждена. Это не аппаратная, GUI или релизная приёмка.

## Среды и pins

- Windows 11 x86-64, Git 2.56.0.windows.2, Python 3.14.8; дополнительный Python venv использовался при первом запуске. MSVC 19.51.36260.0 / VS Professional 2026 18.11.12328.437; CMake приложения 3.31.6. `vswhere -all` обнаруживает только VS 2026, установленный VC toolset — только 14.51.36231. Исторические результаты MSVC 2022 в ADR-013 сохранены и не выдаются за новую чистую проверку.
- WSL Ubuntu 22.04.5 LTS x86-64, GCC 11.4.0, Git 2.34.1, Python 3.10.12, CMake приложения 3.22.1. Установлены недостающие системные prerequisites Ninja 1.10.1 и zip 3.0 через apt. Это Linux build-среда в WSL, не доказательство GUI/audio на физической Linux-системе.
- vcpkg baseline `6fdd23fa0a9479a9f9a1c48178696f60cf39255d`; bootstrap 2026-10-29 / `139f2e53438b1529e178f6295bc563f4e0de734f`. SDK producer использует CMake 4.4.4 / Ninja 1.13.2; Linux-инструменты дополнительно установлены в отдельный Python venv с точными версиями. Глобальный CMake приложения не заменялся.
- SoapySDR source 0.8.1, локальная packaging revision #1, исходный tag и SHA-512 сохранены. Qt superproject и все пять модулей сверены с `dependencies.lock.json`.

## Повтор после утверждения MSVC 2026 (2026-11-11)

Результаты первоначального получения Windows/MSVC 2026 и Ubuntu 22.04/GCC 11 из таблицы ниже входят в согласованную матрицу. После обновления bootstrap на Windows повторно сверены все Qt/vcpkg pins и установка SoapySDR 0.8.1#1 в существующем корне. vcpkg обнаружил `cl.exe` из Visual Studio 2026 (toolset 14.51.36231). В новой директории сборки независимый CMake consumer проверил загрузку SoapySDR, версию и ABI, CTest **1/1**. Тесты bootstrap прошли **4/4**. Повтор использовал ранее полученные исходники и установленный SDK.

## Результаты проверки 2026-11-08

| Проверка | Windows 11 / MSVC 2026 | Ubuntu 22.04 / GCC 11 |
|---|---|---|
| Получение Qt source subset и совпадение всех Git pins | Pass | Pass |
| Повторный запуск окончательного bootstrap, проверка кеша | Pass | Pass |
| Установка hardware feature, SoapySDR 0.8.1#1 | Pass | Pass |
| Независимый CMake consumer: импорт target, линковка, runtime release/ABI | 1/1 pass | 1/1 pass |
| Headless SDR Next с заголовками нового SDK; собраны HackRF/RTL-SDR/Airspy modules | 3/3 CTest pass | 2/2 CTest pass |
| Network-free проверки bootstrap | 4/4 pass | 4/4 pass |

Проверки bootstrap: выбор pinned commit вместо более нового remote HEAD и повторный запуск, отказ трогать чужую непустую папку, отказ менять remote другого репозитория, сохранение локальных изменений без reset/fetch. Дополнительно `fetch-dependencies.py --root .` отклонён с exit 1 без изменения проекта. В CI добавлен запуск этих проверок; удалённый CI в этой сессии не запускался.

Consumer `tests/dependency-sdk` намеренно собирается вне основной CMake-сборки приложения и проверяет настоящий экспортированный SoapySDR target и вызовы C API. Upstream добавляет build-info suffix к версии, поэтому CMake `EXACT 0.8.1` не применим; release 0.8.1 и ABI 0.8 проверяются исполняемым тестом. Успешная компиляция headers сама по себе не считалась достаточной проверкой package fixup.

## Выявленные и устранённые проблемы

1. Upstream port 0.8.1#0 в baseline собирал библиотеку на Linux, но fixup искал отсутствующий `debug/cmake`. Локальный overlay #1 выбирает фактический Linux-путь `share/cmake/SoapySDR`, сохраняя Windows-путь `cmake`. До исправления установка завершалась BUILD_FAILED; после — успешно, consumer проходит на обеих ОС.
2. Унаследованный WSL PATH включал множество Windows-каталогов; CMake configure тратил минуты на поиск через 9P. Эти запущенные агентом Linux-проверки остановлены. Повтор с Linux-only PATH и исправленным port завершил установку SDK за 23 s. Глобальный PATH/конфигурация WSL не менялись.
3. После остановки проверок временные каталоги `/tmp/sdr-deps-*` оказались очищены. Итоговый Linux-кеш размещён в постоянном `/home/sergey/.cache/sdr-deps-g0-20261108`; Git source cache восстановлен из проверенного Windows-кеша локальным clone и повторно проверен по всем pins. Первоначальное получение исходников из upstream на Linux и Windows отражено в отдельных журналах; повтор из кеша не выдаётся за новый сетевой download.

## Воспроизводимые команды и артефакты

Общая инструкция — [Developer Guide](../DEVELOPER_GUIDE.md), включая получение из upstream в новую внешнюю постоянную папку. Итоговые roots:

- Windows: `D:/Tools/sdr-deps-g0-20261108`; SDK `installed/x64-windows`; source `qt-source`.
- Linux: `/home/sergey/.cache/sdr-deps-g0-20261108`; SDK `installed/x64-linux-dynamic`; source `qt-source`.

Linux producer после восстановления кеша:

```sh
env PATH=/home/sergey/.cache/sdr-g0-tools/bin:/usr/local/bin:/usr/bin:/bin \
  python3 scripts/fetch-dependencies.py \
  --root /home/sergey/.cache/sdr-deps-g0-20261108 --qt-source --hardware
```

Отдельный tools venv содержит `cmake==4.4.4 ninja==1.13.2`; при отсутствии подходящих инструментов штатный vcpkg загружает их по своим фиксированным metadata. Linux application/consumer построены системным CMake 3.22.1/Ninja 1.10.1 с `CMAKE_PREFIX_PATH=<root>/installed/x64-linux-dynamic`. Windows application/consumer — в DevShell MSVC 2026 с `CMAKE_PREFIX_PATH=<root>/installed/x64-windows`; для consumer DLL bin добавлен в PATH только процесса.

Сырые журналы локальны и не включаются в Git:

- `build/dependency-overlay-windows.log`, `build/dependency-persistent-linux-overlay.log`: окончательный SDK producer.
- `build/dependency-source-final-windows.log`, `build/dependency-source-repeat-{windows,linux}.log`: проверка source pins/повторного запуска; финальный Linux producer также проверил каждый pin.
- `build/dependency-build-{windows,linux}.log`, `build/dependency-consumer-{windows,linux}.log`: сборки и CTest.
- `build/dependency-bootstrap-tests-{windows,linux}.log`: четыре защитных сценария на каждой ОС.
- Ранние `dependency-acquisition-*`, `dependency-hardware-*`, `dependency-persistent-linux.log`: первоначальное получение и остановленные/неуспешные попытки, не pass.

## Границы результата

Получение исходников Qt не создаёт установленный Qt SDK. Новая сборка Qt на Ubuntu 22.04, чистая MSVC 2022 среда, Docker image после изменения, Windows 10/Ubuntu 24.04/Debian 12 и последующая GUI/audio/portable приёмка не считаются проверенными. Аппаратные драйверы не устанавливались, действующий процесс SDR Next не перезапускался. Проверки SDK не подтверждают совместимость реальных устройств. На дату исходного прогона основной MVP change оставался активным, прогресс был 27/54.
