# MVP v0.1 — реализация и проверка

Дата: 2026-11-07. Change: `plan-mvp-v0-1`, schema `spec-driven`.

**Результат: предварительная Windows-сборка, полная приёмка MVP не завершена.** Код приёмника, DSP, GUI, записи, настроек, диагностики и SDK реализован. Наличие кода не закрывает обязательные аппаратные, платформенные и длительные проверки. Change остаётся активным, архивировать его пока нельзя.

2026-11-08 [восьмичасовой HackRF GUI/audio/WAV прогон](RX_ENDURANCE_RUN_2026-11-08.md) остановился с **FAIL** в 13:30 МСК после 2 ч 50 мин 50 с: IQ queue overflow 1, audio underrun 520, audio drops 43, working set до 531.652 MiB. WAV корректно финализирован. Задача 7.4 остаётся открытой, требуются диагностика и повторный полный прогон; UI input и physical audio latency не измерены.

После [подробного разбора](RX_ENDURANCE_RETRY_ANALYSIS.md) запущен [отдельный диагностический повтор](RX_ENDURANCE_RETRY_RUN_2026-11-08.md), **13:54–21:54 МСК**, с расширенной телеметрией и прежними буферами/порогами. Статус RUNNING; корневая причина не объявлена исправленной. Лимит памяти снова нарушен в первых замерах. Итоговая приёмка остаётся открытой.

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
| demod-audio: Непрерывность аудио | AudioBuffer, QAudioSink | Нехватка аудиоданных — счётчики/zero-fill, короткий GUI тест; 8 h not verified |
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
| release-acceptance: Воспроизводимые benchmarks | core/src/bench.cpp | Измерение профиля — короткие 2.4/10/20 MSPS есть; полный metrics collection/burn-in not verified |
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
3. Восьмичасовой профиль 2.4 MSPS с GUI/audio/recording и сырыми метриками CPU/RAM/new frames/UI p95/audio latency/drops. Короткие результаты выше не доказывают SLO.
4. Негативные SDK/init/lifecycle и storage fault-injection сценарии, ручное прослушивание на обеих ОС, полный restart/layout/DPI protocol.
5. Портативные пакеты на чистых машинах, завершённый аудит фактических зависимостей и notices, повторный OpenSpec verify. После этого — синхронизация спецификаций и archive.

Историческая попытка установки Microsoft Build Tools была отклонена автоматической проверкой исполнения (`blocked by policy`, без подробной причины). При повторной проверке уже установленный полный MSVC 2022 найден и использован без установки; прежний отказ больше не блокирует эту Windows-сборку. Docker/WSL startup errors не исправлялись разрушительными действиями с пользовательскими средами.
## Дополнение: независимые модули устройств

Последующее изменение ADR-017 заменило встроенный Soapy-адаптер независимыми Device Provider модулями. Текущие результаты и покрытие — в [DEVICE_PROVIDER_VERIFICATION.md](DEVICE_PROVIDER_VERIFICATION.md). MSVC 2026: 7/7 CTest, физический HackRF через отдельную DLL, оба Windows пакета (с модулями и без) выполняют synthetic RX. Предыдущие результаты ниже остаются историческими; ограничения полной приёмки и 27/54 задач родительского MVP сохраняются. RTL-SDR/Airspy не объявлены проверенными.
