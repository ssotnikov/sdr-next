# ADR-023 — Фиксированное получение зависимостей

Дата: 2026-11-08. Статус: принято в рамках технической делегации пользователя; выполнение задачи 1.4 проверяется отдельно.

## Решение

- Сохранить baseline vcpkg `6fdd23fa0a9479a9f9a1c48178696f60cf39255d`. Один commit задаёт registry и bootstrap metadata инструмента. В manifest добавить override SoapySDR **0.8.1#1** (локальный packaging overlay), чтобы минимальное ограничение версии не допускало незаметного обновления.
- `dependencies.lock.json` фиксирует этот commit, Qt **6.10.0** superproject `077347cc6d198053fb61cc0841c5c0c60a7deeb1` и gitlinks qtbase/qtdeclarative/qtmultimedia/qtshadertools/qtsvg. Это commit, на который указывает upstream tag v6.10.0; ветка или имя тега не используются как изменяемый lock.
- Общий `scripts/fetch-dependencies.py` использует Python 3.10+ и Git, получает исходники в отдельную внешнюю папку, проверяет Git HEAD и Qt gitlinks. Существующие изменённые checkout не очищаются и не сбрасываются. Bootstrap vcpkg и установка hardware feature включаются явно; метрики vcpkg отключаются.
- Windows — `x64-windows`, Linux — `x64-linux-dynamic`: SoapySDR port не поддерживает static linkage. Оба профиля предоставляют отдельный SDK-prefix, который передаётся CMake; runtime драйверов и аппаратура не устанавливаются. На Windows bootstrap предпочитает VS 2022; при его отсутствии использует уже установленный VS 2026 с явным сообщением о дополнительном, не релизном профиле.
- Получение Qt source subset не создаёт готовый SDK. Его сборка и системные prerequisites документируются отдельно; готовый Windows SDK допускается только версии 6.10.0. CMake уже требует эту версию EXACT. Portable Linux Qt собирается на Ubuntu 22.04 согласно ADR-013; получение исходников не доказывает завершение этой сборки.
- Linux CI получает SoapySDR через тот же baseline/manifest, а не через изменяемую версию libsoapysdr-dev из apt. Системные build prerequisites остаются частью платформенного окружения и не объявляются герметичным образом ОС.
- `vcpkg-configuration.json` регистрирует `cmake/vcpkg-ports/soapysdr`. Upstream port 0.8.1#0 из baseline успешно собирает библиотеку на Linux, но fixup ищет отсутствующий `debug/cmake`; upstream SoapySDR устанавливает конфигурацию в `share/cmake/SoapySDR`. Локальная ревизия #1 выбирает CONFIG_PATH по платформе, сохраняя source tag и SHA-512 архива. Исходный MIT copyright рецепта сохранён рядом; библиотека остаётся BSL-1.0. Обновлять весь baseline ради этого исправления не требуется.

## Границы

SoapySDR — optional build dependency независимых providers (ADR-017), не обязательная зависимость core/GUI. Source/Demodulator SDK принадлежит репозиторию; Rust example std-only и использует свой Cargo.lock. Нет установки native hardware SDK, SoapyRemote или новых DSP-библиотек. Версии ОС, компиляторы, приёмка и критерии ADR-013/015 не меняются.

## Альтернативы и ограничения

Системный SoapySDR удобен для экспериментов, но не фиксирует одинаковую версию на двух ОС. Qt через vcpkg изменил бы выбранный отдельный SDK и состав зависимости; оставлен отдельный pinned source path. Копирование Radioconda не обеспечивает чистой воспроизводимой установки и переносит неаудированные драйверные DLL, поэтому не является bootstrap.

Git commit обеспечивает проверку содержимого Git-объектов, а не независимую подпись поставщика. Сетевой доступ и upstream availability необходимы при первом получении. Скрипт не устанавливает компиляторы, не выполняет финальный лицензионный аудит и не подтверждает побайтово одинаковые бинарные сборки.

## Проверка

[Протокол получения](DEPENDENCY_ACQUISITION_VERIFICATION.md), [инструкция](../DEVELOPER_GUIDE.md). Задача OpenSpec: `plan-mvp-v0-1`, 1.4. Не архивировать весь MVP по результату одного этапа.

## Источники

- [vcpkg versioning](https://learn.microsoft.com/en-us/vcpkg/users/versioning).
- [SoapySDR port в выбранном baseline](https://github.com/microsoft/vcpkg/tree/6fdd23fa0a9479a9f9a1c48178696f60cf39255d/ports/soapysdr).
- [Qt superproject commit](https://github.com/qt/qt5/tree/077347cc6d198053fb61cc0841c5c0c60a7deeb1).
