# MVP v0.1 — реализация и проверка

Дата: 2026-11-07. Change: `plan-mvp-v0-1`, schema `spec-driven`.

**Результат: предварительная Windows-сборка, полная приёмка MVP не завершена.** Код приёмника, DSP, GUI, записи, настроек, диагностики и SDK реализован. Наличие кода не закрывает обязательные аппаратные, платформенные и длительные проверки. Change остаётся активным, архивировать его пока нельзя.

Прогон [после замены polling](RX_ENDURANCE_EVENT_RUN_2026-11-08.md) остановился после 2962 s с source timeout. Пользователь предположил отсоединение кабеля; причина не подтверждена журналом. [Отдельный повтор](RX_ENDURANCE_CABLE_RETRY_2026-11-08.md) **8 ноября 19:38 — 9 ноября 03:38 МСК** завершил 8 часов без зарегистрированных разрывов IQ и переполнений; WAV финализирован. Приёмка не пройдена: 1 551 audio underrun, 29 audio drops, RAM 546,352 MiB, видимые интервалы спектра ниже порога. Задержки UI и аудио не измерены. Задача 7.4 остаётся открытой.

2026-11-08 [восьмичасовой HackRF GUI/audio/WAV прогон](RX_ENDURANCE_RUN_2026-11-08.md) остановился с **FAIL** в 13:30 МСК после 2 ч 50 мин 50 с: IQ queue overflow 1, audio underrun 520, audio drops 43, working set до 531.652 MiB. WAV корректно финализирован. Задача 7.4 остаётся открытой, требуются диагностика и повторный полный прогон; UI input и physical audio latency не измерены.

После [подробного разбора](RX_ENDURANCE_RETRY_ANALYSIS.md) выполнен [диагностический повтор](RX_ENDURANCE_RETRY_RUN_2026-11-08.md), **13:54–14:41 МСК**, с расширенной телеметрией и прежними буферами/порогами. Он прерван штатной остановкой RX после 2812 s: IQ/audio counters 0, working set до 533.020 MiB. Журнал содержит stop/start с неизвестным происхождением. Это не повтор прежнего overflow и не успешная восьмичасовая приёмка; корневая причина первого сбоя остаётся открытой.

По повторному поручению пользователя выполнен [прогон с защищённым управлением](RX_ENDURANCE_GUARDED_RUN_2026-11-08.md): **15:31–15:44 МСК**, 757 s, **FAIL** с IQ overflow 1/audio underruns 18/drops 7, worker gap до 405 ms. По результатам исправлен polling ожидания worker ([ADR-019](ADR-019-rx-worker-wakeup.md)); причинная связь этого недостатка со всеми stalls ещё не доказана. Задача 7.4 и полная приёмка остаются открытыми.

## Полномочия и архитектура

Исходные 12 ADR сохранены без переутверждения. Пользователь отдельно утвердил платформы в ADR-013, делегировал технические решения с фиксацией в ADR-014, утвердил эталон и performance/DSP-критерии в ADR-015. O-006 остаётся открытым; O-016…O-018 — несогласованный roadmap после MVP. O-014/O-019 задают технический путь, но не означают прохождения платформенной/релизной матрицы.

Последующее явное решение пользователя [ADR-016](ADR-016-deferred-device-qualification.md): реальные испытания RTL-SDR и Airspy перенесены после MVP. Они теперь deferred, не pass и не самостоятельный блокер v0.1. Полная будущая O-006 остаётся открытой; текущая обязательная аппаратная приёмка — HackRF. Две задачи испытаний перенесены из активного checklist в roadmap без отметки выполнения.

Headless `core/` не зависит от Qt. `app/` содержит QML/controller, acquisition worker и DSP/record worker; конечные очереди отделяют GUI от IQ. `sdk/include/sdr/plugin.h` — C ABI Source/Demodulator. Примеры C++ собираются отдельно; Rust source вызывается тем же C++ host. Нативные модули доверенные, crash isolation отсутствует.

## Среда и свидетельства

| Проверка | Фактический результат / граница свидетельства |
|---|---|
| Windows native | Windows 11 build 26300; повторная проверка с Qt 6.10.0 msvc2022_64 и MSVC 19.44.35228.0/v143, Build Tools 2022 17.14.40, SDK 10.0.22621.0: сборка и CTest 4/4. CMake 3.31.6, Ninja 1.13.0.git.kitware.jobserver-pipe-1. Ранее отдельно прошёл MinGW 13.1 preview |
| Эталон | Ryzen 5 4600H, 16 GB, AMD iGPU driver 31.0.21925.1001, AC, Balanced. Эталон согласован; полноценный performance-прогон ещё не выполнен |
| DSP | FFT 2048/Hann: правильный bin, −6.0206 dBFS для амплитуды 0.5 с ошибкой <0.05 dB. AM/NFM/WFM: 1 kHz, offset 12 kHz, Fs 240 k/1 M/2.4 MSPS; частота 1000 Hz, корреляция 0.9974…1.0000 после 50 ms. Контрольное AM подавление +200 kHz при Fs 2.4 MSPS: 50.54 dB. Это минимальные принятые тесты, не полная RF-характеристика |
| Core | Bounded queue/order/threaded transfer, ограниченное ожидание/timeout/stop; WAV header/count, CF32 round-trip/EOF/truncation, ABI mismatch и lifetime. Вызов Rust DLL из C++ host прошёл |
| Settings | Round-trip, нулевые значения, повреждённый JSON и schema 99 сохраняются без перезаписи |
| Engine | Synthetic → WAV/IQ/sidecar → file EOF; sample count/complete; отказ reconfigure при записи; plugin duplicate/unload; темп обработки 2.4 MSPS проверяется отдельной короткой регрессией |
| QML | Настоящий Main.qml: synthetic RX, >20 новых кадров, zoom/pan clamps, темы, RU/EN, перенос панели и stop. Отдельный физический HackRF-сценарий включается `SDR_TEST_HACKRF=1` |
| Независимый SDK | Windows Source и Demodulator CMake projects собраны без внутренних заголовков приложения. Rust 1.89.0/Cargo 1.89.0, GNU Windows DLL и ранее Linux SO реально вызваны host; финальная Linux-ревизия требует повторной проверки |
| Linux core | Ранее в этом change прошли Ubuntu 22.04/WSL, GCC 11.4 и headless CTest. После последних изменений повторный запуск WSL блокирован `Wsl/Service/CreateInstance/ERROR_TIMEOUT`; это не pass финальной ревизии |
| Linux GUI | Ранее собран Qt 6.10/Ubuntu 24.04 container, core+engine 2/2. Первоначальный X11 smoke выявил отсутствие libxcb-shape.so.0; Dockerfile дополнен. Финальный GUI/QML прогон не выполнен: Docker Desktop затем перестал запускать daemon |
| Переносимый Windows пакет | `scripts/package-windows.ps1` + `scripts/smoke-windows.ps1`; запуск/закрытие окна с developer PATH, исключённым из окружения. `qt.conf`, явный Windows plugin в launcher, отдельные SDK examples, notices, Qt SBOM и SHA-256 inventory. Это не чистая VM и не финальный лицензионный аудит |

Повторный MSVC-пакет: локальный `dist/sdr-app-0.1-preview-msvc2022-win64`, `Start-SDR-HackRF.cmd` использует уже установленный внешний Soapy runtime. SHA-256 sdr_app.exe: `7B539D3002E09B84C38349A9A3E4259549EEE26E450B847C674B9788087C78BB`; binary в пакете совпадает с результатом сборки. MSVC/QML физический HackRF-тест: 6,553,600 IQ samples, 41 FFT кадр, IQ gaps/overflow/audio drops/underrun/backend underrun — 0. Runtime VC++ уже доступен на этом компьютере; windeployqt включил vc_redist.x64.exe для возможных prerequisites других машин, его установка не запускалась.

Следующая полная пересборка по запросу пользователя: Visual Studio 2026 18.11.12328.437, MSVC 19.51.36260.0 / toolset 14.51.36231, SDK 10.0.26100.0, Qt 6.10.0 msvc2022_64, Ninja Release. Отдельный build-каталог `build/windows-msvc2026`; CTest **4/4**, 13.03 s, с включённым `SDR_TEST_HACKRF=1` (включая проверки нулевых IQ gaps/audio underrun). Предыдущий пакет MSVC 2022 сохранён. Это локальная проверка дополнительного компилятора, не завершение всей матрицы или длительной приёмки.

Новый пакет: `dist/sdr-app-0.1-preview-msvc2026-win64`, локальный launcher `Start-SDR-HackRF.cmd`; SHA-256 приложения `E32D538DFFCC86A4A11E6B852DBCB3D6C0CF2A81FDE7D04230989EF63BF01D63`. Запуск и закрытие GUI с исключённым developer PATH прошли, exit 0. Дополнительная визуальная проверка этой сборки вручную не проводилась; прежний визуальный протокол выше относится к пакету MSVC 2022.

После MSVC-сборки визуально проверено настоящее окно пользовательского пакета: читаемые элементы управления, выбор источников с обнаруженным HackRF, запуск synthetic RX, обновляемые спектр и waterfall, остановка в Ready; видимые IQ gaps/audio underrun — 0. Это локальная визуальная проверка, не ручное прослушивание, не вся DPI/backend matrix и не восьмичасовой тест. Выбор HackRF для реального RX отдельно проверен автоматическим QML-сценарием; визуальный RX использовал synthetic.

Сырые локальные журналы находятся в игнорируемом `build/`, двоичные пакеты в игнорируемом `dist/`; они не коммитятся. Команды повторения — в Developer Guide. Для устройств в отчёт не включается серийный номер.

## HackRF One

Runtime: SoapySDR 0.8.1 (Radioconda py312hd5eb7cc_5), SoapyHackRF 0.3.4, libhackrf 2024.02.1; firmware 2024.02.1/API 1.08, hardware r9. Отдельная старая утилита hackrf_info 2023.01.1 не является версией runtime приложения.

| Профиль | IQ samples / длительность | IQ gaps | Значение результата |
|---|---|---|---|
| 2.4 MSPS, WFM, headless | 24,000,000 / 10.0511 s | 0 | Короткий реальный RX, не GUI/audio/burn-in |
| 10 MSPS, WFM, headless | 30,000,000 / 3.00514 s | 0 | Исследовательский короткий RX |
| 20 MSPS, WFM, headless | 60,000,000 / 3.01888 s | 0 | Исследовательский короткий RX |
| 2.4 MSPS, GUI/controller/audio | 6,291,456 IQ samples, 41 новых FFT кадров, около 2.6 s IQ | 0 в повторном тесте | Queue overflow 0, audio drops/underrun/backend underrun 0, audio device buffer 2400 frames; полные SLO требуют 8 h и отдельных измерений |

В ходе GUI-тестов исправлены: освобождение старой waterfall texture до её отсоединения; перестановка SplitView items; безусловная пауза DSP между готовыми блоками; реакция на пакетную выдачу Soapy; недостаточное стартовое заполнение audio и лишнее предварительное чтение QIODevice (адаптер теперь Unbuffered). Промежуточный вариант иногда давал 2 audio underrun; после последнего исправления три последовательных коротких HackRF/QML прогона прошли с нулевыми счётчиками и без ошибок. Счётчики gaps/overflow/audio сохраняются после stop. Это регрессия наблюдавшихся дефектов, не восьмичасовая приёмка.

## Матрица F-001…F-014

`partial` — код и часть проверок есть; `blocked` — нет обязательного окружения/железа; `pass` относится только к явно указанной проверке.

| ID | Статус | Свидетельство / что осталось |
|---|---|---|
| F-001 | partial/blocked | Windows 11/MSVC 2022 build/tests/package/visual pass; Windows 10, Debian 12 и полная Linux GUI matrix не проверены |
| F-002 | partial; RTL-SDR/Airspy deferred | Для v0.1 не завершена полная применимая HackRF OS/settings/failure matrix; реальные RTL-SDR/Airspy испытания после MVP по ADR-016, не блокируют текущий выпуск |
| F-003 | partial | CF32/metadata/EOF round-trip пройден; повторить финальную ревизию Linux |
| F-004 | partial | Capability UI, rate/center readback, gain/unsupported checks в коде; полный аппаратный tune/gain/options/unplug протокол не выполнен |
| F-005 | partial | Принятые synthetic DSP тесты проходят; ручное прослушивание на двух ОС отсутствует |
| F-006 | partial | FFT numerical + QML многокадровый тест; iGPU/HiDPI/Wayland и длительный FPS отдельно |
| F-007 | partial | Один VFO, QAudioSink, короткий HackRF GUI RX; слышимость и измеренная latency не подтверждены |
| F-008 | partial | WAV PCM16 mono/48 kHz header/count и engine capture; финальный Linux storage test отдельно |
| F-009 | partial | CF32/sidecar complete/sample count/round-trip; disk full/permission/slow disk injection не завершён |
| F-010 | partial | C++ DLL/SO и Rust FFI; negative major/minor/size/callback/init, manifest kind/platform/duplicate и busy unload пройдены Windows; forced process error/panic и финальная Linux-ревизия отдельно |
| F-011 | partial | Две области/вкладки/ширина/лево-право и JSON; полный визуальный restart/HiDPI protocol отдельно |
| F-012 | partial | Settings tests pass на Windows; миграция старой схемы пока означает явный отказ с сохранением исходника |
| F-013 | partial | Локальный JSONL, counters и экспорт; долгий сбор CPU/RAM/UI p95/latency ещё не выполнен |
| F-014 | partial/blocked | Windows preview smoke; Linux portable, чистые машины и финальный аудит не завершены |

## OpenSpec: соответствие требований и сценариев

Все 34 ADDED requirements и 40 сценариев перечислены ниже. Путь capability означает `openspec/changes/plan-mvp-v0-1/specs/<capability>/spec.md`. Это карта реализации и покрытия, а не объявление всех сценариев pass.

| Capability / требование | Реализация | Сценарии и статус |
|---|---|---|
| desktop-foundation: Сборка desktop на двух платформах | CMakeLists.txt, ci/Dockerfile.qt, scripts/build.ps1 | Сборка из чистого окружения — blocked полной матрицей |
| desktop-foundation: Независимая проверка ядра | core/, tests/core_tests.cpp | Проверка без GUI — pass ранее Windows/Linux; последняя Linux-ревизия not verified |
| desktop-foundation: Воспроизводимость зависимостей | vcpkg.json, DEPENDENCIES.md, точный find Qt | Отсутствует обязательная зависимость — configure fail предусмотрен; clean vcpkg install обеих ОС not verified |
| rx-session: Аппаратные и файловый источники | core/src/source.cpp, app/engine.cpp | Выбор подключённого источника — partial HackRF/file; Второй активный источник — один worker/source, start делает stop; отдельный полный сценарий not verified |
| rx-session: Возможности устройства | source.cpp, Main.qml | Неподдерживаемая настройка — код отклоняет/GUI отключает, полная device matrix blocked |
| rx-session: Управляемый жизненный цикл | Engine start/stop/run | Остановка и повторный запуск — engine/QML pass; Отсоединение USB или timeout — код есть, физический unplug not verified |
| rx-session: Ограниченная буферизация и учёт разрывов | bounded_queue.hpp, Engine/Controller | Замедление отображения — архитектурно RX независим, отдельный stall test not verified; Переполнение IQ — queue timeout тест и наблюдавшийся Faulted, fault injection не завершён |
| demod-audio: Три режима демодуляции | core/src/dsp.cpp | Демодуляция контрольного сигнала — pass согласованных минимальных DSP тестов |
| demod-audio: Изменение параметров канала | Demodulator::configure, Engine::start | Смена sample rate — DSP тесты трёх rates + offset, end-to-end hardware reconfigure partial |
| demod-audio: Выбор аудиовыхода | Controller::setAudio/startAudio | Недоступное аудиоустройство — диагностика в коде, физический unplug not verified |
| demod-audio: Непрерывность аудио | AudioBuffer, QAudioSink | Нехватка аудиоданных — счётчики/zero-fill; 8 h fail: 1 551 underrun, 29 drops |
| spectrum-workspace: Спектр и waterfall | spectrum_item.cpp, Main.qml | Тон известной частоты — FFT pass, GUI frames pass; визуальная частотная калибровка partial |
| spectrum-workspace: Управление частотой и видом | Main.qml, Controller | Изменение частоты — applied center/rate задают шкалу, zoom clamp pass; аппаратный tune protocol partial |
| spectrum-workspace: Ограниченный docking | SplitView::moveItem, JSON layout | Восстановление раскладки — код/JSON tests, полный визуальный restart not verified |
| spectrum-workspace: Темы и читаемость | Main.qml themes/accent/RU-EN | Смена темы — QML pass во время RX; DPI/contrast manual not verified |
| spectrum-workspace: Подтверждённые режимы отображения | Qt RHI/software, этот отчёт | Проверка неподтверждённого backend — ограничения отмечены, неподтверждённые среды не объявлены pass |
| native-plugin-sdk: Две категории SDK | plugin.h, plugins/, sdk/rust/ | Подключение примеров — C++ host вызывает source/demod/Rust; финальная Linux matrix partial |
| native-plugin-sdk: Версионированный бинарный контракт | core/src/plugin.cpp | Несовместимый major — pass fixture; Отсутствующая необязательная функция — v1.0 optional extensions не определяет; baseline Source/Demod работает без callbacks другой категории, unsupported minor/short struct/missing required callback — pass Windows |
| native-plugin-sdk: Владение памятью и ошибки FFI | PluginModule/Instance, examples, Rust catch_unwind | Ошибка при обработке — проверки возвратов в коде, forced error/panic matrix not verified |
| native-plugin-sdk: Выгрузка после завершения работы | shared module lifetime, stop/unload | Отключение занятого плагина — код отклоняет до stop; lifetime и busy unload rejection — pass Windows; фоновые callbacks запрещены контрактом v1.0 |
| native-plugin-sdk: Явное доверие нативным расширениям | Engine::load_plugin, manifest guide | Некорректный manifest — duplicate/kind/platform/init rollback — pass Windows |
| recording-playback: Ручная запись WAV | recording.cpp, Engine | Завершённая WAV-запись — header/count/capture pass Windows, Linux ранее |
| recording-playback: Документированный Raw IQ | IqWriter, metadata, ADR-014 | Проверка записи IQ — round-trip/count/complete pass Windows, Linux ранее |
| recording-playback: Файловый источник | source.cpp File, Engine metadata | Контрольное воспроизведение — pass; Неизвестный формат — version/format/finite/truncation checks, часть negative cases pass |
| recording-playback: Видимые ошибки записи | recording.cpp, stop_record(false) | Диск переполнен — обработчик есть, injection not verified; absent folder rejection pass |
| settings-diagnostics: Локальная конфигурация JSON | Controller, QSaveFile | Повторный запуск — pass controller recreation; Повреждённый файл настроек — pass preservation/schema tests |
| settings-diagnostics: Диагностика приёма | Snapshot, JSONL, status counters | Перегрузка тракта — Faulted/counters наблюдались при диагностике, полный sustained protocol not verified |
| settings-diagnostics: Приватность по умолчанию | Локальные QFile события, ручной export | Первый запуск и ошибка — автоматических network/crash-upload путей в коде нет; runtime network capture не выполнялся |
| release-acceptance: Портативная поставка | package/smoke scripts | Запуск пакета — Windows startup/exit pass; весь RX/audio/SDK/storage и Linux package partial |
| release-acceptance: Свидетельства аппаратной поддержки | этот отчёт, ADR-016 | Airspy недоступен для проверки — deferred по решению пользователя; совместимость по непроведённым испытаниям не заявлена; текущая HackRF matrix остаётся обязательной |
| release-acceptance: Воспроизводимые benchmarks | core/src/bench.cpp, app/endurance.hpp | Короткие 2.4/10/20 MSPS есть; 8 h завершены, количественная приёмка не пройдена, физические задержки не измерены |
| release-acceptance: Числовая приёмка после утверждения порогов | ADR-015, tests/core_tests.cpp | Пороги не согласованы — историческое условие теперь не наступает: пользователь их утвердил; числовой performance verdict всё ещё not evaluated |
| release-acceptance: Аудит релизных зависимостей | notices/licenses/Qt SBOM/package inventory | Неожиданная зависимость в бинарнике — initial DLL inventory есть; финальный license/link audit not verified |
| release-acceptance: Полнота MVP | tasks.md, F matrix | Завершение G6 — blocked, не архивируется |

## Verify: полнота, корректность, согласованность

- **Completeness: CRITICAL.** Выполнены 27/54 активных задач. Оставшиеся 27 требуют своих результатов, даже если значительная часть кода уже реализована. Две прежние задачи реальных RTL-SDR/Airspy испытаний отложены в roadmap по ADR-016, не отмечены выполненными. Полная матрица F и 34 требований/40 сценариев составлена; partial/blocked/deferred строки не считаются pass.
- **Correctness: WARNING.** Код найден для основных функциональных требований; непокрытые негативные сценарии и непроверенные среды перечислены поимённо выше. После устранения промежуточных GUI/RX дефектов требуется полная приёмка, а не только unit pass.
- **Coherence: соответствует техническим ADR с ограничениями.** Исходная baseline неизменна; новые решения выделены в ADR-013…015, design.md актуализирован. C++20 core/Qt GUI/C ABI и ограниченные очереди соответствуют принятой модели. Own FIR/FFT и mono WFM — осознанный объём ADR-014. Allocation-free realtime и DSP SIMD не заявлены.

## Что требуется для G6

1. Оставшиеся утверждённые Windows/Linux окружения: Windows 10, финальные Linux build/QML/audio, X11/Wayland/HiDPI, USB permissions и Ubuntu 22.04 portable build. MSVC 2022 на Windows 11 теперь проверен.
2. Полная применимая аппаратная tune/gain/options/unplug/reopen matrix HackRF. Реальные RTL-SDR/Airspy испытания отложены после MVP по ADR-016; их непроведение не является блокером текущего G6.
3. Устранить нарушения аудио, памяти и отображения; повторить восьмичасовой профиль 2.4 MSPS с видимым окном и без параллельных сборок. Отдельно измерить UI input p95 и физическую audio latency.
4. Негативные SDK/init/lifecycle и storage fault-injection сценарии, ручное прослушивание на обеих ОС, полный restart/layout/DPI protocol.
5. Портативные пакеты на чистых машинах, завершённый аудит фактических зависимостей и notices, повторный OpenSpec verify. После этого — синхронизация спецификаций и archive.

Историческая попытка установки Microsoft Build Tools была отклонена автоматической проверкой исполнения (`blocked by policy`, без подробной причины). При повторной проверке уже установленный полный MSVC 2022 найден и использован без установки; прежний отказ больше не блокирует эту Windows-сборку. Docker/WSL startup errors не исправлялись разрушительными действиями с пользовательскими средами.
## Дополнение: независимые модули устройств

Последующее изменение ADR-017 заменило встроенный Soapy-адаптер независимыми Device Provider модулями. Текущие результаты и покрытие — в [DEVICE_PROVIDER_VERIFICATION.md](DEVICE_PROVIDER_VERIFICATION.md). MSVC 2026: 7/7 CTest, физический HackRF через отдельную DLL, оба Windows пакета (с модулями и без) выполняют synthetic RX. Предыдущие результаты ниже остаются историческими; ограничения полной приёмки и 27/54 задач родительского MVP сохраняются. RTL-SDR/Airspy не объявлены проверенными.

## Дополнение: G0 / получение зависимостей

Pins, bootstrap и локальное исправление Linux packaging SoapySDR выполнены по ADR-023. [Протокол](DEPENDENCY_ACQUISITION_VERIFICATION.md): SDK producer, независимый CMake consumer и headless SDR Next проверены на Windows/MSVC 2026 и Ubuntu 22.04/GCC 11. MSVC 2022 сейчас не обнаружен; задача 1.4 сохраняет открытый статус до проверки согласованной среды. Это не новая GUI, аппаратная или G6 приёмка; общий прогресс остаётся 27/54.

## Дополнение: Windows preview и проверка зависимостей 11 ноября

Пакет `dist/sdr-next-0.1-preview-20261111-win64` собран с MSVC 2026 и Qt 6.10.0. CTest прошёл 8/8, повторная сборка и CTest в Ubuntu WSL с диагностическим SDK прошли 7/7. Windows-пакет обработал synthetic IQ при очищенном PATH. Отдельный запуск из пакета с системным PATH и внешним SoapySDR проверил HackRF, активный аудиовыход и запись WAV. Тесты загрузили Source, Demodulator и Device Provider непосредственно из папки примеров пакета. [Аппаратный прогон и реакция GUI](RX_ENDURANCE_PROTOCOL.md#диагностика-спектра-11-ноября)

Проверены размеры и SHA-256 всех 1 433 файлов в манифесте. В 99 PE-файлах прочитаны обычные и отложенные импорты. На эталонной Windows 11 импортируемые DLL обнаружены в пакете, пространстве Windows API Set или System32. Пакет не содержит бинарников FFmpeg. В приложенных Qt SPDX по имени файла найдены 86 PE-файлов с LGPL-вариантом. Для остальных шести сторонних бинарников карта `windeployqt` указала источники. `d3dcompiler_47.dll`, `dxcompiler.dll` и `dxil.dll` взяты из Windows SDK Redist, `vc_redist.x64.exe` из Visual Studio Redist, `opengl32sw.dll` из Qt SDK, `icuuc.dll` из System32. Последнюю DLL импортирует Qt6Core. Для закрытия 7.7 нужно проверить условия распространения и полноту notices. Инвентаризация PE не учитывает все динамические загрузки по имени. Локальные отчёты находятся в `build/endurance/package-20261111-imports.json` и `build/endurance/package-20261111-source-map.txt`. [Документация windeployqt](https://doc.qt.io/qt-6.10/windows-deployment.html)

Для сборки Qt из закреплённых исходников на Ubuntu 22.04 настроены X11/XCB и PulseAudio, FFmpeg отключён. Чистые машины, Linux tar.gz и окончательная проверка лицензий остаются открытыми по задачам 7.5–7.7.

## Дополнение: перемещение Linux-пакета и системная ICU

С `SDR_INSTALL_LINUX_PORTABLE=ON` CMake устанавливает приложение, Qt, QML, примеры SDK, модули устройств и файлы лицензий. Пробный tar.gz с диагностическим Qt 6.10.0 из conda-forge распакован в новую папку с пробелом в имени. При очищенных путях к SDK успешно выполнен `--smoke-rx` через XCB/WSLg. ABI-тесты загрузили три SDK-примера из распакованного дерева. В дополнительной пробе этого дерева синтетический IQ 2,4 MSPS дал аудиовывод и читаемый WAV PCM16 mono 48 кГц, без разрывов IQ и underrun. Проверено чтение `bin/sdr-settings.json` и сохранение параметров после штатного закрытия окна. Журналы находятся в `build/wsl-ubuntu-2204-core/portable-*.log`.

Для пробного пакета использовался Qt с FFmpeg из conda-forge. Сборка закреплённого Qt из исходников и подготовка релизного tar.gz остаются открытыми. При последующей проверке Ubuntu перестала запускать процессы с ошибкой ввода-вывода. Свободное место Windows на C:, где находится её VHDX, составляло около 20 МиБ. Результат сборки Qt и повтор функциональной проверки после последней переупаковки предстоит подтвердить после восстановления доступа к Ubuntu.

Пакет `dist/sdr-next-0.1-preview-20261111-win64-system-icu` содержит 1 432 файла по манифесту и 98 PE-файлов. Проверены размеры, SHA-256 и разрешение обычных и отложенных импортов на эталонной Windows 11. Пакет прошёл synthetic RX с очищенным PATH. Во время работы процесс загрузил `icuuc.dll` из System32. Условия распространения пяти остальных сторонних бинарников вне Qt SPDX и проверка Windows 10 остаются открытыми.

## Повторная проверка Linux после восстановления WSL

Ubuntu 22.04.5 снова запускается после освобождения места на диске Windows. Повторная упаковка с диагностическим Qt 6.10.0 из conda-forge прошла проверку в новой папке с пробелом в имени. Из распакованного пакета успешно запущен синтетический RX через WSLg/XCB с программным рендерингом и масштабом 150%. В отдельном прогоне зарегистрированы 519 318 аудиокадров, ноль разрывов IQ, переполнений очереди и underrun. WAV PCM16 mono 48 кГц содержит 480 052 кадра, последний кадр читается. Подтверждены загрузка трёх примеров SDK, чтение настроек из каталога пакета и их сохранение после штатного закрытия. В пробном пакете отсутствует Wayland platform plugin, запуск с `QT_QPA_PLATFORM=wayland` завершился ошибкой. Журналы в `build/wsl-ubuntu-2204-core/portable-final-*.log`.

Для Qt из закреплённых исходников проверка XCB при конфигурации падала из-за отсутствующего `xcb.h` рядом с заголовками расширений XCB в локальном sysroot. После добавления `libxcb1-dev`, `libxcb-render0-dev` и `libxcb-shm0-dev` из Ubuntu 22.04 конфигурация подтвердила XCB, Wayland client и PulseAudio. FFmpeg отключён. Сборка продолжена. Проверка обоих графических режимов и фактического аудиовывода из пакета с этой сборкой остаётся открытой. Пакет с Qt из conda-forge остаётся диагностическим.

При сборке XCB линковщик остановился на `cannot find -lxkbcommon-x11`. В локальном sysroot был `libxkbcommon-x11-dev`, но его ссылка `libxkbcommon-x11.so` указывала на отсутствующую runtime-библиотеку. После распаковки `libxkbcommon-x11-0` версии 1.4.0-1 и повторной конфигурации `QXcbIntegrationPlugin` собран. На Ubuntu WSL установлен runtime `libxcb-cursor0` версии 0.1.1-4ubuntu1. В отдельной проверке копий Qt без исходных RUNPATH обе библиотеки XCB разрешились из `/lib/x86_64-linux-gnu`, а тестовое окно Qt отобразилось через XCB и Wayland при DPR 1,50. Журнал `build/wsl-ubuntu-2204-core/pinned-qt-platforms-relocated.log`. Полная сборка Qt и проверка распакованного SDR Next продолжаются по 7.6.

## Linux preview с Qt из закреплённых исходников

Qt 6.10.0 и SDR Next собраны на Ubuntu 22.04.5 под WSL с GCC 11.4. CTest прошёл 7/7. При первой упаковке Qt deployment выбрал только XCB. После явного включения XCB и Wayland архив `dist/sdr-next-0.1-preview-20261111-linux-x86_64.tar.gz` распакован в новую папку с пробелом в имени. С очищенными путями к SDK приложение прошло синтетический RX через XCB и Wayland при масштабе 150% с программным рендерингом. Проверены три примера SDK и сохранение настроек при штатном закрытии через XCB. Архив и контрольная сумма сохранены локально в `dist/`.

В обоих режимах короткий синтетический прогон 2,4 MSPS дал вывод в WSLg PulseAudio, ноль разрывов IQ, переполнений и underrun. В каждом прогоне прочитан WAV PCM16 mono 48 кГц с 480 052 кадрами. Проверка 113 файлов ELF через `ldd` разрешила зависимости из архива или системных каталогов и не обнаружила линковки с FFmpeg. Журналы находятся в `build/wsl-ubuntu-2204-core/portable-pinned-qt-{final,elf,wayland-audio}.log`. Нативные проверки на чистых целевых Linux-системах, Windows 10 и финальный аудит поставки остаются открытыми.

## Предварительный аудит Linux preview

Из 113 файлов ELF 106 совпали по относительному пути с записями Qt SPDX. У всех 106 записей указан вариант LGPL-3.0-only. Оставшиеся семь файлов собраны из кода SDR Next: приложение, три аппаратных модуля и три примера SDK. SPDX относится к сборке SDK. Сопоставление по путям учитывает, что Qt deployment может менять RUNPATH и контрольную сумму ELF. [Документация Qt о сторонних компонентах](https://doc.qt.io/qt-6.10/licenses-used-in-qt.html)

Команды линковки и определённые символы подтвердили код из статических библиотек Ubuntu `libX11-xcb`, `xcb-randr`, `xcb-shape`, `xcb-sync`, `xcb-util`, `xcb-xfixes` и `xcb-xkb` внутри `libQt6XcbQpa`. Код `xcb-glx` найден в `libqxcb-glx-integration.so`. В Linux-архив добавлены три точных файла copyright из использованных dev-пакетов Ubuntu и пять SPDX из Qt SDK. В новом `dist/sdr-next-0.1-preview-20261111-linux-x86_64-notices.tar.gz` все 113 ELF побайтово совпали с предыдущим preview. После распаковки повторно прошёл RX через XCB и Wayland, проверены контрольная сумма архива и CTest 7/7. Журнал `build/wsl-ubuntu-2204-core/portable-pinned-qt-notices.log`.

Для закрытия 7.7 остаются сверка уведомлений компонентов, встроенных в Qt, проверка способа предоставления соответствующих исходников и условий распространения пяти оставшихся сторонних файлов Windows вне Qt SPDX. [Условия LGPL по документации Qt](https://www.qt.io/development/open-source-lgpl-obligations)

## Исходники Qt и дополнительные файлы Windows

В `dist/sdr-next-0.1-preview-20261111-linux-qt-6.10.0-source.tar.gz` сохранены superproject и пять модулей Qt из чистых деревьев с ревизиями, указанными в `dependencies.lock.json`. Проверены состав архива и SHA-256. В архив включены параметры сборки и версии пакетов локального sysroot. После распаковки CMake сгенерировал проект с XCB, Wayland и PulseAudio. Для этого в локальном sysroot явно выбраны три системные библиотеки XCB, как в исходной сборке. Журнал `build/wsl-ubuntu-2204-core/qt-source-companion-configure-final.log`. [Состав и инструкция сборки](QT_LINUX_SOURCE_PROVENANCE.md)

Обход связей `DEPENDS_ON` от 106 поставляемых Qt ELF в SPDX выделил 61 запись об исходном стороннем коде, один встроенный модуль (`BundledTLSF`) и 23 системные зависимости. Это список для сверки уведомлений. Например, SPDX Qt Gui перечисляет атрибуцию кода Direct3D 12 в Linux SDK, поэтому наличие связи само по себе не подтверждает компиляцию этого кода. Локальный список `build/wsl-ubuntu-2204-core/qt-spdx-attribution-review.tsv`.

Все пять Windows-файлов совпали по SHA-256 с локальными источниками из карты `windeployqt`. Проверка Authenticode прошла для всех пяти. Microsoft подписала четыре файла, The Qt Company подписала `opengl32sw.dll`.

| Файлы | Установленный источник и версия | Основание и остаток проверки |
|---|---|---|
| `d3dcompiler_47.dll` | Windows SDK `Redist/D3D/x64`, 10.0.26100.9457 | Указан в списке для классических Windows-приложений. [REDIST Windows SDK](https://learn.microsoft.com/en-us/legal/windows-sdk/redist) |
| `dxcompiler.dll`, `dxil.dll` | Windows SDK `Redist/D3D/x64`, оба 1.9.2602.17 | Оба указаны в том же списке. Для этой пары проверяется происхождение из SDK. [REDIST Windows SDK](https://learn.microsoft.com/en-us/legal/windows-sdk/redist) |
| `vc_redist.x64.exe` | Visual Studio 2026 Professional `VC/Redist`, 14.51.36247.0 | Отдельный подписанный установщик VC++ Runtime. Проверка условий применимой лицензии Visual Studio остаётся в процедуре выпуска. [Visual Studio 2026 REDIST](https://learn.microsoft.com/en-us/visualstudio/releases/2026/redistribution) |
| `opengl32sw.dll` | Qt SDK 6.10.0. Строки бинарника указывают Mesa 11.2.2 и LLVM 3.6.2 | Программный OpenGL. Нужны полные уведомления Mesa/LLVM для этого бинарника либо отдельная проверка графических режимов пакета без этой DLL. [Архив Qt llvmpipe](https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/) |

В отдельной копии Windows preview без `opengl32sw.dll` прошёл синтетический RX со штатным D3D11 на Radeon, программным D3D11 на Microsoft Basic Render Driver и программным backend Qt Quick. Выбранные backend и адаптер подтверждены журналом Qt. Это результат эталонной Windows 11. Проверка Windows 10 и сценария явного выбора программного OpenGL остаётся открытой. Локальные журналы в `build/endurance/windows-no-mesa-6730996a/`. [Графические режимы Qt для Windows](https://doc.qt.io/qt-6.10/windows-graphics.html)

7.7 остаётся открытой. Нужно завершить сверку лицензий встроенного кода с уведомлениями, определить условия включения Microsoft Redist в публичную поставку и подтвердить предоставление соответствующих исходников для неё. Локальные архивы не публиковались. [Условия Windows SDK](https://learn.microsoft.com/en-us/legal/windows-sdk/license)

## Уведомления о стороннем коде в Linux preview

В `dist/sdr-next-0.1-preview-20261111-linux-x86_64-attributions.tar.gz` добавлен каталог `licenses/qt-attributions/`. Скрипт сопоставил 61 запись SPDX с 58 исходными записями Qt по пути и индексу, сверил идентификаторы и выражения лицензий и включил 56 файлов с текстами. В `index.tsv` указаны исходные записи, авторство и приложенные тексты. Две независимые сборки каталога дали побайтово одинаковый результат. Подмена файла Apache Tika или связанного SPDX отклоняется. [Команды упаковки](QT_LINUX_SOURCE_PROVENANCE.md#уведомления-в-бинарном-пакете)

В Linux Qt Network найдены `kDafsa` и `LookupStringInFixedSet`. Архив исходников содержит соответствующие данные Public Suffix List и текст MPL-2.0. В Qt Multimedia найдены символы TLSF, в комплект включён его BSD-текст из заголовка. Резервная база MIME из Apache Tika входит в правило сборки Qt Core, приложен `NOTICE.txt` закреплённой ревизии. Запись D3D12 сохранена в индексе как часть консервативного набора, объект D3D12 RHI в Linux-сборке отсутствует. [Apache Tika NOTICE](https://github.com/apache/tika/blob/dbcbb368e0a913e1a74cca61347fd6ce24bc6d7e/NOTICE.txt)

Из новой распаковки в каталог с пробелом прошёл короткий RX через XCB и Wayland. Совпали SHA-256 всех 113 ELF предыдущего preview. CTest прошёл 7/7. SHA-256 нового архива сохранён в соседнем файле `.sha256`. Журнал находится в `build/wsl-ubuntu-2204-core/portable-pinned-qt-attributions.log`.

7.7 остаётся открытой до проверки полноты соответствующих исходников и способа их предоставления, а также завершения аудита Windows. Сверка 61 записи охватывает граф SPDX для этого Linux preview. Подтверждение фактического включения исходного кода выполнено отдельно для перечисленных выше компонентов.

## Исходники статических компонентов X.Org

Сверены команды линковки Qt для Linux preview. Восемь файлов `.a` из локального sysroot побайтово совпали с файлами из восьми использованных dev-пакетов Ubuntu. Метаданные этих пакетов указали исходники `libx11 2:1.7.5-1ubuntu0.4`, `libxcb 1.14-3ubuntu3` и `xcb-util 0.4.0-1build2`. Дополнительный системный вход `librt.a` в линковке Qt Core оказался архивом без объектных файлов.

Получены десять файлов трёх исходных пакетов через APT с проверкой подписанных индексов Ubuntu. `dpkg-source` распаковал все три пакета и применил изменения Ubuntu, версии сверены с `debian/changelog`. Ключи подписантов отдельных `.dsc` отсутствовали в локальном наборе ключей. Отдельно подтверждены подписи всех трёх `InRelease` ключом архива Ubuntu. Журналы `build/wsl-ubuntu-2204-core/xorg-source-{acquisition,verification,companion}.log`. [Инструкция и состав архива](QT_LINUX_SOURCE_PROVENANCE.md#исходные-пакеты-ubuntu-для-xorg)

Расширенный архив `dist/sdr-next-0.1-preview-20261111-linux-qt-xorg-source.tar.gz` объединяет исходники Qt и исходные пакеты X.Org. Повторная распаковка и проверка SHA-256 вложенных файлов прошли. Пересборка исходных пакетов Ubuntu, в том числе генерация XCB с исходной версией `xcb-proto`, остаётся непроверенной. Для 7.7 также остаются проверка предоставления исходников при публикации и аудит Windows.
