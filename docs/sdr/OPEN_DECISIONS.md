# Открытые решения — не согласовано

Дословная выдержка из [SDR Architecture Baseline 1.0](SDR_Architecture_Baseline_1.0.md).

## 12. Список открытых архитектурных решений (не утверждать автоматически)

| ID | Вопрос | Когда закрывать | Риск незакрытого решения |
|---|---|---|---|
| O-001 | Точная версия Qt, поддержка Windows 10 и Linux runtime matrix | до G0 | deploy/toolchain divergence |
| O-002 | SoapySDR основной адаптер vs резервные native SDK | до G1 | дублирование аппаратного слоя |
| O-003 | Формат IQ в памяти: CF32, packed SC16, lifetime blocks | до G1 | копии/ABI-несовместимость |
| O-004 | FFT backend, SIMD, liquid-dsp build без FFTW | до G2 | лицензия/CPU regression |
| O-005 | Audio backend и pipeline rates | до G2 | underruns/latency |
| O-006 | Hardware matrix: конкретные RTL/HackRF/Airspy модели, sample rates, OS | до G1/G6 | неподтверждённая совместимость |
| O-007 | Точная функциональность limited docking в 0.1 | до G3 | uncontrolled GUI scope |
| O-008 | Raw IQ datatype, sidecar schema, диск/разрывы/восстановление | до G5 | непереносимые записи |
| O-009 | Конкретная семантика `process()`, config и thread-affinity Plugin ABI | до G4 | ABI churn |
| O-010 | Политика plugin unload при потоках и обработка rollback ошибок | до G4 | crashes |
| O-011 | Минимальная конфигурация reference laptop, версии OS/driver | до performance baseline | нет воспроизводимых результатов |
| O-012 | Числовые критерии FPS, CPU, RAM, latency, drop и uptime | до G6 | нет объективного acceptance |
| O-013 | Проектная реализация CPU rendering fallback | до G3 | неподдерживаемые GPU |
| O-014 | Поддержка Linux Wayland/X11, HiDPI и USB permissions | до G6 | различия среды |
| O-015 | Набор локализаций в MVP (RU/EN) и правила i18n | до G3 | UX scope |
| O-016 | Подписывание/канал обновлений Windows/Linux (P1) | после MVP | supply-chain risk |
| O-017 | Политика цифровых прав / каталог и sandbox plugins | после MVP | trust boundaries |
| O-018 | Время и объем Android/ARM64 | roadmap | platform constraints |
| O-019 | Exact portable distribution and driver prerequisites | до G6 | неверные обещания portable |
| O-020 | Формат ошибок/телеметрия/Crash report privacy | до G3/G6 | неконсистентная диагностика |

Не выбранные решения остаются открытыми, даже если в списке зависимостей или в API предложен практичный вариант.
