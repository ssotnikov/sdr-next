# Архитектурные материалы SDR

Материалы перенесены 2026-11-06. Рабочая папка проекта уточнена пользователем: `D:/Projects/sdr-app`; документы и инструкции перенесены сюда из ранее использовавшейся папки OneDrive.

## Материалы

- [ADR-018: название программы SDR Next](ADR-018-product-name.md).
- [Подключение пользовательской иконки](PRODUCT_ICON_INTEGRATION.md).

- [План реализации MVP v0.1](MVP_V0_1_IMPLEMENTATION_PLAN.md) — предлагаемая последовательность G0–G6, критерии и зависимости от открытых решений; [изменение OpenSpec](../../openspec/changes/plan-mvp-v0-1/proposal.md).
- [Полные исходные требования пользователя](USER_REQUIREMENTS_ORIGINAL.md) — дословный текст заполненного опросника, 14 разделов.
- [Подтверждение 12 решений пользователем](ADR_USER_CONFIRMATION_ORIGINAL.md) — дословное сообщение о выборе решений.
- [SDR Architecture Baseline 1.0](SDR_Architecture_Baseline_1.0.md) — исходный файл без изменений, включая компоненты, зависимости, API-контракты, MVP и критерии приёмки.
- [12 ADR](ADR_REGISTER.md) — выдержка из раздела 2 спецификации.
- [Реестр 20 исходных открытых решений](OPEN_DECISIONS.md) — историческая выдержка и текущие статусы.
- [ADR-013: платформы](ADR-013-desktop-platforms.md), [ADR-014: технические контракты](ADR-014-mvp-implementation-contracts.md), [ADR-015: эталон и приёмка](ADR-015-reference-and-acceptance.md).
- [ADR-016: перенос испытаний RTL-SDR/Airspy](ADR-016-deferred-device-qualification.md), [термины подключения источников](GLOSSARY.md).
- [ADR-017: независимые модули устройств](ADR-017-device-provider-modules.md), [Device Provider API](DEVICE_PLUGIN_API.md), [проверка модульности](DEVICE_PROVIDER_VERIFICATION.md).
- [Протокол длительного RX](RX_ENDURANCE_PROTOCOL.md), [текущий восьмичасовой прогон](RX_ENDURANCE_RUN_2026-11-08.md).
- [Разбор первого прогона](RX_ENDURANCE_RETRY_ANALYSIS.md), [повторный тест с диагностикой](RX_ENDURANCE_RETRY_RUN_2026-11-08.md).
- [Текущий тест с защищённым управлением](RX_ENDURANCE_GUARDED_RUN_2026-11-08.md).
- [ADR-019: пробуждение RX worker](ADR-019-rx-worker-wakeup.md), [проверка изменения](RX_WORKER_WAKEUP_VERIFICATION.md).
- [Текущий длительный RX после замены polling](RX_ENDURANCE_EVENT_RUN_2026-11-08.md).
- [Итоги восьмичасового повтора после таймаута источника](RX_ENDURANCE_CABLE_RETRY_2026-11-08.md).
- [DSP API](DSP_API.md), [Plugin SDK](PLUGIN_SDK.md), [зависимости](DEPENDENCIES.md), [проверка реализации](MVP_V0_1_VERIFICATION.md).

## Происхождение и полнота

Исходные сообщения получены из чата «Выбор языка для SDR», conversationId `6aecfc51-aec4-83eb-8d11-38749743677a`. Это перенос материалов в файлы проекта; история сообщений не объединялась с текущим чатом.

Спецификация скопирована из `D:/Projects/sdr-app/docs/SDR_Architecture_Baseline_1.0.md`. SHA-256 оригинала и копии совпадает: `F57853F350221C6EC2A817CFB35BFAE8E33BFEC9977FCFED43110B576BE782FD`. Исходник оставлен на месте.

## Правила продолжения

Все этапы разработки проходят по OpenSpec согласно корневому `AGENTS.md`. Архитектурные документы сохраняются в `docs/sdr/`, артефакты OpenSpec — в структуре OpenSpec со ссылками на архитектурные документы. Пути `docs/adr/` и `docs/architecture/` в исходной спецификации являются прежним предложением структуры; для новых архитектурных файлов действует правило `docs/sdr/`.

Статус A у исходного ADR не делает содержащиеся в нём предложения R и открытые параметры O утверждёнными. Последующие решения фиксируются отдельными ADR, оригинальная baseline остаётся историческим документом. Эталон и количественные критерии теперь согласованы ADR-015, но их выполнение требует измерений. MVP change остаётся активным до завершения обязательных gates.

- [Адаптация brand book для Qt Quick](ADR-020-qml-brand-theme.md).

- [Проверка темы brand book](BRAND_THEME_VERIFICATION.md).

- [Фирменный заголовок и меню](ADR-021-branded-window-header.md).

- [Проверка заголовка и меню](WINDOW_HEADER_VERIFICATION.md).

- [План custom appearance profiles](ADR-022-custom-appearance-profiles.md), [контракт и этапы](APPEARANCE_PROFILES.md), [JSON Schema](appearance-resource-v1.schema.json), [примеры](examples/appearance/README.md). Реализация запланирована.

- [Предложения дополнений MVP по UI SDR#](SDRSHARP_MVP_FEATURE_REVIEW.md) — анализ, не утверждённое расширение.

- [Фиксированное получение зависимостей](ADR-023-pinned-dependency-acquisition.md), [протокол проверки](DEPENDENCY_ACQUISITION_VERIFICATION.md).
