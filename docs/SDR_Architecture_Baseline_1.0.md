# SDR Architecture Baseline 1.0

**Статус:** базовая архитектурная спецификация (решения ADR приняты пользователем; отдельные параметры открыты)  
**Дата:** 2026-11-06  
**Цель:** универсальный мультиплатформенный SDR-приёмник с развитым GUI и расширяемым DSP, конкурент SDR++/SDR#.  
**Приоритет документа:** архитектурная база для реализации MVP 0.1; не утверждает незаполненные требования.

## 0. Правила интерпретации

- **Утверждено (A)** — прямо выбрано в исходном опроснике или при фиксации 12 решений.
- **Проектное предложение (R)** — рекомендуется в этом документе, но пользователь пока не утверждал.
- **Открыто (O)** — нет согласованного решения; проектирование соответствующей части нельзя считать окончательно закрытым.
- **Отложено (D)** — заявлено как требование целевого продукта, но выведено за границы 0.1 отдельным принятым решением.
- **Под проверкой (V)** — зависит от PoC, лицензии, реализации или результатов измерений.

Если ранее перечисленные P0/P1-функции не вошли в 0.1, это означает **изменение границ релиза**, а не отказ от функции. Никакие метрики не считаются измеренными, а целевые значения — согласованными до принятия тестового плана.

## 1. Product charter и границы

### 1.1. Назначение (A)

Универсальный приёмник SDR для начинающих пользователей, радиолюбителей, исследователей, опытных разработчиков расширений и профессионалов. Функции: прослушивание эфира, мониторинг, анализ спектра и платформа внешних демодуляторов/декодеров. Ориентиры по классам продуктов: SDR++ и SDR#; копирование их кода и UI не является требованием.

### 1.2. Платформы (A)

- Релиз 0.1: Windows 10/11 и Linux x86-64, преимущественно Ubuntu/Debian; портативные поставки приложения.
- Целевая дорожная карта: Linux ARM64 и Android; мобильный GUI — отдельная адаптивная композиция на переиспользуемой логике, после desktop.
- macOS и iOS **не заявлены** и не включаются по умолчанию.
- RX в 0.1, TX позднее и только отдельным проектным решением, учитывающим аппаратные и регуляторные ограничения.
- По умолчанию один активный источник SDR, один активный демодулируемый канал в 0.1 (R; несколько VFO остаются в дорожной карте).

### 1.3. Не цели для 0.1 (A/D)

Android-клиент, SoapyRemote, TX, GPU DSP, каталог плагинов, изолированный host и sandbox, все девять заявленных категорий плагинов, сложный docking, расширенные цифровые декодеры, полноценные SigMF/IQ WAV/запись waterfall, многопользовательский сервер и дистанционное управление. Отсутствие этих функций в 0.1 не отменяет соответствующих исходных требований.

## 2. Реестр архитектурных решений (ADR)

### ADR-001 — C++20 + Qt 6 Quick/QML (A)
**Контекст:** производительное DSP-ядро, Windows/Linux и будущий Android, современный GUI.  
**Решение:** C++20 для ядра и desktop; Qt Quick/QML для представления; Qt-модули не должны проникать в публичные интерфейсы DSP Core. C++23 допустим только после отдельного решения и проверки toolchain.  
**Последствия:** единый основной язык, но потребуются C ABI для плагинов, собственные render-компоненты и сборка Qt по условиям лицензий.  
**Проверка:** headless DSP-тесты собираются без Qt; GUI PoC показывает живые FFT и waterfall.

### ADR-002 — Собственный ограниченный docking (A)
**Решение:** собственный docking только с необходимыми операциями (панели, вкладки, простое разделение, сохранение layout); не создавать сразу аналог полной docking-системы IDE.  
**Последствия:** снижен риск GPL-зависимости, но появляются собственные задачи по drag/drop, восстановлению расположения и HiDPI.  
**Открыто:** точный минимум возможностей docking в 0.1, мультиоконность, detaching и UX-контракты.

### ADR-003 — Native plugins в 0.1, isolated runtime позже (A)
**Решение:** плагины работают в процессе приложения; отдельный plugin-host/watchdog/sandbox после MVP.  
**Последствия:** нельзя обещать, что авария нативного плагина не завершит приложение. Разработать API, допускающий будущий IPC, но не изображать нативный ABI как безопасную песочницу.  
**Требование к 0.1:** недоверенные плагины не поддерживаются; установка — явное действие пользователя, ошибки инициализации обрабатываются, DLL выгружается только при отсутствии живых объектов и активных вызовов.

### ADR-004 — Plugin SDK v0.1: Source + Demodulator (A)
**Решение:** две внешние категории плагинов в 0.1, остальные категории остаются в дорожной карте. SDK рассчитан на C++ и Rust через стабильный C ABI.  
**Последствия:** расширения спектра, визуализаций, GUI, рекордера, сетевых сервисов и автоматизации не являются публичным SDK 0.1.  
**Важное уточнение:** аппаратные SoapySDR-модули — отдельная система, не эквивалент плагинам приложения.

### ADR-005 — Гибридный DSP pipeline (A)
**Решение:** оптимизированная фиксированная магистраль IQ с известными точками расширения source/demodulator; графовые связи внутри ядра формируются и управляются отдельно от UI.  
**Последствия:** предсказуемое планирование и буферизация без обязательного универсального графового редактора.  
**Открыто:** физический формат обмена и принадлежность ресемплинга/частотного переноса демодулятору.

### ADR-006 — Надёжность RX и latency выше throughput (A)
**Решение:** не допускать бесконтрольной очереди IQ, блокирования аппаратного потока GUI, скрытых дропов и деградации аудио ради FPS.  
**Последствия:** при перегрузке сначала сокращать визуализационную работу; затем диагностировать невозможность поддержки выбранной конфигурации. Политика потерь записывается и измеряется.  
**Открыто:** числовой бюджет задержки, условия прекращения приёма и допустимость потерь IQ в режиме обычного мониторинга.

### ADR-007 — MIT и аудит лицензий (A)
**Решение:** собственный код MIT; зависимости допустимы при отдельно подтверждённой совместимости условий использования и распространения; использовать LGPL и permissive-компоненты с выполнением всех обязательств.  
**Последствия:** нельзя автоматически считать весь исполняемый пакет «целиком MIT»; при включении Qt по LGPLv3 обеспечить надлежащие уведомления, тексты лицензий, возможность замены/перелинковки и прочие применимые требования. Никаких неаудированных GPL-зависимостей в поставке.  
**Особый риск:** liquid-dsp — MIT, но может опционально использовать GPL FFTW; проверять build flags и бинарные зависимости. Окончательная правовая оценка зависит от релизной конфигурации.

### ADR-008 — Android позже, отдельный адаптивный GUI (A)
**Решение:** ядро и бизнес-логика не зависят от desktop QML-компоновки, аппаратный слой допускает платформенные адаптеры.  
**Последствия:** USB Host, мобильные разрешения/драйверы и ограничения фоновых процессов будут отдельным проектом; Android не является критерием приёмки 0.1.

### ADR-009 — Локальное приложение, SoapyRemote после MVP (A)
**Решение:** 0.1 не требует сервера, удалённого UI или сетевого управления. В перспективе SoapyRemote — альтернативный источник SoapySDR.  
**Последствия:** безопасность сети не входит в 0.1, но до релиза сетевой функции обязателен отдельный security-review и решение о защите канала.

### ADR-010 — WAV Audio + Raw IQ в 0.1 (A)
**Решение:** ручная запись аудио WAV и бинарного IQ.  
**Последствия:** Raw IQ без описания формата не самодостаточен. В 0.1 запись обязана сопровождаться собственным metadata JSON sidecar (R), который документирует формат, byte order, sample rate, центральную частоту и время. Это **не объявляется SigMF** до подтверждения соответствия спецификации.  
**Открыто:** точное представление Raw IQ и формат sidecar, политика атомарности и разрывов записи.

### ADR-011 — Профили benchmark 2.4/10/20 MSPS (A)
**Решение:** эти скорости включены в матрицу тестов, а не считаются гарантией для каждой SDR-модели.  
**Последствия:** throughput, CPU, температура, потери и буферизация должны измеряться по каждому доступному источнику и сценарию.  
**Открыто:** какая конфигурация и источник обязаны выдерживать конкретную скорость.

### ADR-012 — Референсный ноутбук до benchmark (A)
**Решение:** эталонное оборудование должно быть документировано до утверждения метрик производительности.  
**Открыто:** CPU, RAM, iGPU, ОС, драйверы, питание, USB-контроллер, параметры энергосбережения.

## 3. Согласованность и трассировка требований

| Исходное требование | Текущее состояние | Примечание |
|---|---|---|
| Open Source / MIT | A | Исходный код MIT; юридические обязанности зависимостей сохраняются |
| Windows и Linux x86-64 | A | Обе платформы обязательны для 0.1 |
| Linux ARM64 / Android | D | Целевые платформы без обязательств 0.1 |
| RTL-SDR / HackRF / Airspy / файл | A, V | Функциональный объём заявлен; аппаратные тесты должны подтвердить |
| SoapySDR основной API | R | Было одновременно выбрано «SoapySDR» и «определить»; конкретный подход утверждать нельзя |
| AM / NFM / WFM | A | Обязательны в 0.1 |
| DSP Graph | A | Зафиксирована гибридная модель |
| GPU waterfall | R, V | Предлагаем Qt Quick Scene Graph; требуется PoC |
| FFT/DSP на GPU | D | После MVP; в 0.1 CPU/SIMD baseline |
| Сложный dockable GUI | A с ограничением | Только собственный ограниченный docking вначале |
| Source/Demodulator Plugin SDK | A | Две категории в 0.1 |
| Устойчивость GUI при сбое плагина | D | Невозможна как универсальная гарантия для нативного кода в 0.1 |
| Audio WAV, Raw IQ | A | В 0.1 |
| IQ WAV, SigMF, spectrum/waterfall capture | D | Последующие релизы |
| SoapyRemote | D | После MVP |
| Цифровые декодеры | D | Не требуются для MVP |
| Адаптивные FPS | R | Технический вариант, целевые числа не утверждены |
| Много VFO, сканирование, ADS-B/ACARS | D | Дорожная карта |
| JSON настройки | A | Формат выбран, схема и механизм миграции — R |
| Автоматические обновления | O | P1-ожидание; политика безопасности и реализация не определены |

## 4. Компонентная архитектура и правила зависимостей

### 4.1. Слои

1. **platform/base**: примитивы времени, ошибок, метрики, логирование, API потоков, утилиты; без Qt.
2. **radio/core**: `RadioSession`, `DeviceManager`, `SourceController`, параметры тюнинга, жизненный цикл сеанса.
3. **hardware/adapters**: SoapySDR адаптер, file IQ source, позднее SoapyRemote и платформенные источники.
4. **dsp**: блоки IQ, DDC/channelizer, фильтры, AGC, demodulator, audio conditioning, spectrum analysis.
5. **audio/recording**: audio sink, WAV writer, IQ writer, metadata, error/backpressure reporting.
6. **plugin-runtime**: discovery/manifest, ABI negotiation, реестр Source/Demodulator, lifecycle, teardown.
7. **application**: команды, read models, use cases, сохраняемые настройки и сессии, ограничения доступа.
8. **gui**: QML, ViewModels, темы, workspace, спектр/waterfall renderer. Только этот слой зависит от Qt Quick.
9. **optional plugin-host** (D): out-of-process runtime, IPC, watchdog, sandbox.

### 4.2. Направление зависимостей

`gui -> application -> {radio, dsp, recording, plugin-runtime} -> base`  
`hardware adapters -> radio interfaces`  
`plugin implementations -> public plugin API`, не на внутренние классы `application`/`gui`.

Нельзя включать Qt-типы в публичный DSP API; проводить блокирующие device operations в GUI thread; вызывать QML из DSP thread; связывать плагины с C++ ABI внутренних классов; считать SoapySDR-плагины собственными плагинами приложения.

### 4.3. Поток данных

```text
[SDR Hardware | File Source]
         |
   IQ acquisition thread
         |
   Format adapter/normalizer
         |
  Bounded IQ block transport + monotonic sequence
         |----------------------> [IQ Writer / Sidecar]
         |----------------------> [FFT -> Decimated frames -> GUI]
         v
   DDC -> filter -> decimation
         |
   Demodulator (built-in/plugin)
         |
   Audio postprocessing -> bounded audio queue
         |----------------------> [WAV Recorder]
         v
      Audio output
```

Ветка рекордера должна получать данные в чётко определённой точке (до/после нормализации), что отражается в metadata. Для нескольких потребителей применяется опубликованный блок с явным управлением временем жизни, а не небезопасное совместное чтение одной SPSC-очереди.

### 4.4. Рекомендации по очередям и времени

- IQ block: размер, формат, `sample_index`, timestamp/domain, `sample_rate`, `center_frequency`, `discontinuity`, sequence number.
- `SPSC` — только для реальных связей один производитель — один потребитель; фан-аут через pool и независимые bounded queues.
- Приёмное оборудование не ждёт GUI, recorder никогда не создаёт неограниченную очередь. Запись без достаточной пропускной способности завершается/ошибается с диагностикой, не делает вид, что файл полон.
- Управление тюнингом и параметрами осуществляется через control plane, атомарно согласуемый с DSP state.
- Уровни обработки: acquisition (высокий приоритет), DSP, audio, recording, FFT, GUI; конкретные thread priority и realtime политики определяются тестированием.

## 5. API-контракты ядра (проектный уровень)

Здесь определены семантические контракты, а не окончательные C++-заголовки.

### 5.1. `IInputSource`

- `enumerate()` / `capabilities()` возвращают идентификатор устройства, диапазон частот, варианты sample rate, gain, antenna, bias-t/direct sampling только там, где поддерживаются.
- `open(settings) -> Result<SourceHandle>`; `start(on_iq_block)`; `stop()`; `close()`.
- Изменения частоты/sample rate имеют явно определённый статус (`applied`, `unsupported`, `requires_restart`, `failed`).
- Передаваемые IQ-блоки имеют задокументированные время жизни, тип данных, порядок байт, семпловые индексы и сигнал discontinuity.
- Callback не вызывает UI и не может блокировать acquisition без контракта.

### 5.2. `IDemodulator`

- `describe()` определяет поддерживаемые входные sample rate, требуемую канальную полосу, аудиоформат, диапазоны параметров.
- `create(config)`, `prepare(rate, output_audio)`, `process(input IQ, output audio, metadata)`, `reset(reason)`, `destroy()`.
- Никаких аллокаций и блокирующих операций в `process()` после `prepare()` в realtime-конфигурации (предложение; подтверждается SDK-тестами).
- При изменении sample rate обязательно `reconfigure()` или управляемый stop/recreate, без неявного обращения к объекту из чужого потока.

### 5.3. `IRadioSession`

Состояния: `Idle -> Opening -> Ready -> Streaming -> Stopping -> Ready/Closed` и `Faulted`; запрещённые переходы возвращают детерминированную ошибку. Команды: start/stop/tune/set gain/select demodulator/start/stop recording. Все операции возвращают `Result` с структурированным кодом, без неопределённого состояния UI.

### 5.4. `ISpectrumService`

Принимает IQ-источник, конфигурацию FFT (размер/окно/перекрытие/нормировка), возвращает кадры `PowerSpectrumFrame` с центром, частотным шагом, единицами шкалы, последовательностью и timestamp. GUI может отбросить устаревшие кадры; DSP-поток не ожидает их отрисовки. Запрещено выдавать dBm без калибровки/опорного уровня.

### 5.5. `IRecorder`

Записывает audio WAV или документированный Raw IQ; `start(metadata, target)`, `push(block)`, `stop()`, `status()`; дисковое переполнение и пропуски блока — видимые ошибки. После аварийного завершения допускается восстановление/пометка incomplete, механизм уточняется при выборе файловой схемы.

## 6. Публичный Plugin ABI v0.1

### 6.1. Принципы

- C ABI: `extern "C"`, фиксированные целочисленные типы, opaque handles, функции с явно переданным контекстом; никаких STL/Qt-типов, исключений и C++-классов через границу.
- Взаимное согласование `abi_major`, `abi_minor` и размера каждой структуры (`struct_size`) до обращения к необязательным полям.
- У каждого дескриптора есть `id`, версия, тип, capabilities и точка входа. Версия SDK отделена от версии хоста и версии плагина.
- Host-owned allocator или симметричное выделение/освобождение внутри одного модуля; ресурсы не освобождаются чужим runtime.
- Host копирует строки/метаданные, для которых lifetime не гарантирован; входные sample buffers — borrowed и действительны только во время вызова, если не договорено обратное.
- Любая ошибка сообщается кодом результата; исключения C++/panic Rust не пересекают FFI.
- Запрет неконтролируемой выгрузки DLL при активных callback, потоках или живых объектах плагина.
- Native plugins считаются доверенными и не получают гарантии изоляции процесса.

### 6.2. Концептуальный контракт точки входа (R; не финальный header)

```c
#define SDR_PLUGIN_ABI_MAJOR 1u

typedef struct sdr_host_api sdr_host_api;
typedef struct sdr_plugin_api sdr_plugin_api;

/* Returns 0 on success; all pointers and structs validated before use. */
int32_t sdr_plugin_init(
    uint32_t host_abi_major,
    uint32_t host_abi_minor,
    const sdr_host_api *host,
    sdr_plugin_api *out_plugin);
```

Полный header должен дополнительно определить размер каждой структуры, ABI-level поля функций, `shutdown`, `create/destroy`, `process`, типы сэмплов, коды ошибок, threads, reentrancy, extension negotiation и capability descriptors. Пример выше **не является готовым контрактом для бинарной реализации**.

### 6.3. Плагины

**Source plugin:** обнаружение/перечень устройств или источников, конфигурация, поток IQ, события состояния, корректное завершение.  
**Demodulator plugin:** регистрация идентификатора и описания, prepare/process/reset/destroy, управление аудиоформатом и обработка дискретизации.

**Rust bindings:** безопасная idiomatic-обёртка над тем же C ABI, без обещания общей бинарной совместимости Rust ABI.  
**Manifest:** `id`, `name`, `version`, `api_major`, `api_minor`, `platform`, `arch`, `entrypoint`, `kind`, `license`, optional capabilities; проверять конфликты идентификаторов и пути загрузки.  
**Security baseline:** только ручная установка/включение проверенных расширений в 0.1; никаких утверждений о sandbox.  
**Совместимость:** host с ABI major N обязан отклонять несовместимый major; minor добавляет только расширения с feature negotiation; test fixture под Windows/Linux.

## 7. GUI и рендеринг

- QML Presentation / ViewModels / Commands; частотная настройка и параметры визуализации хранятся независимо от QML.
- Компоненты: main shell, device selector, VFO/tuning, spectrum/waterfall, audio controls, record controls, diagnostics, plugin manager, settings.
- Собственная ограниченная docking-система: панели и вкладки, минимальное закрепление/перемещение, сохранение раскладки; поддержка плавающих окон и сложных вложенных доков — O/D.
- Spectrum: decimated power frames -> GUI data model -> renderer; axes, markers, wheel zoom, pan.
- Waterfall: кольцевая GPU-текстура либо собственный QSG render node; backend выбирается PoC; перегруженный GUI пропускает кадры изображения, не IQ.
- GPU rendering и fallbacks тестируются на Windows и Linux без дискретной видеокарты. Возможность запуска без аппаратного GPU через Qt software rendering — **открытая совместимость, не гарантирована**.
- QSGRenderNode/QRhi реализация требует отдельной обёртки, так как публичная стабильность низкоуровневых API Qt отличается от стабильности обычных QML-компонентов.
- Настройки тем: тёмная/светлая/системная, акцентный цвет, waterfall palettes; точная палитра, layout и accessibility остаются открытыми.

## 8. Зависимости, лицензии и supply-chain

| Компонент | Назначение | Статус |
|---|---|---|
| C++20 toolchains: MSVC / GCC / Clang | Код ядра | A; версии O |
| Qt 6 Core/Gui/Qml/Quick/QuickControls | Desktop GUI | A; модули и лицензии проверять по точным версиям |
| CMake + vcpkg Manifest | Сборка и пакеты | A |
| SoapySDR | Унифицированный hardware API | R, проверка PoC и лицензии модулей |
| liquid-dsp | Filter / Resampler / Demod DSP | R; собирать без FFTW GPL до особого решения |
| FFT backend | FFT spectrum | O/V, тестировать разные варианты |
| spdlog | Логи | R; версию и лицензии зафиксировать |
| GoogleTest | Unit/integration tests | R |
| аудио backend (Qt Multimedia/miniaudio/другое) | Playback/WAV | O, выбрать PoC |
| JSON parser | Config и metadata | O, выбрать конкретную библиотеку |
| собственный QML docking | Panels | A |
| plugin-host/IPC | Изоляция | D |

**CI license gate:** SBOM для релизных артефактов, список зависимостей + их лицензий и notices, анализ бинарных link dependencies, проверка запрещённых/неаудированных копилефт-компонентов, фиксация vcpkg `builtin-baseline`, контроль Qt deployment. Не считать `vcpkg.json` самодостаточным lockfile всех внешних SDK/драйверов.

## 9. Функциональные требования MVP 0.1

| ID | Требование | Приёмочное свидетельство |
|---|---|---|
| F-001 | Windows 10/11 и Ubuntu/Debian x86-64 | CI-пакеты + ручной smoke test на каждой ОС |
| F-002 | Один активный SDR: RTL-SDR, HackRF One, Airspy | Device matrix с реальным железом и драйверами |
| F-003 | File IQ source | Повторяемый golden-recording test |
| F-004 | Tune, sample rate, gain, capability-based options | Поддерживаемые настройки работают, неподдерживаемые отключены |
| F-005 | AM, NFM, WFM | Синтетические тестовые IQ-сигналы и ручная проверка звука |
| F-006 | FFT + spectrum и waterfall | Корректный пик/частота по тестовому сигналу, живой GUI |
| F-007 | Один VFO и аудиовывод | Приём слышен, stop/start не оставляет устройства занятыми |
| F-008 | WAV audio recording | Валидные заголовки, sample rate/channels, сверенная длительность |
| F-009 | Raw IQ recording | Воспроизводимый файл + metadata sidecar с форматом и частотой |
| F-010 | Source/Demodulator Plugin SDK | Две раздельные примерные DLL/SO и тест ABI |
| F-011 | Собственный ограниченный docking | Минимальные панели/вкладки, сохранение и восстановление layout — точный перечень O |
| F-012 | Конфигурация в JSON | Сохранение, миграция схемы, ошибка испорченного файла |
| F-013 | Диагностика | Дропы IQ, заполненность очередей, источник, sample rate, ошибки |
| F-014 | Release package | Портативный пакет с явными требованиями к драйверам и лицензиям |

**Не считать готовым:** Если хотя бы одна обязательная аппаратная модель не прошла проверку, нельзя заявлять полную поддержку F-002. Можно выпускать предварительную/экспериментальную сборку с явной маркировкой, но не считать критерий приёмки выполненным.

## 10. Критерии приёмки и benchmark plan

### 10.1. Обязательные проверки без фиктивных чисел

- **IQ continuity:** счётчики последовательностей/разрывов, отсутствие неопознанных дропов; каждое обнаруженное нарушение отражено в diagnostics.
- **RX stability:** start/stop, смена частоты, повторное подключение, отключение USB, отказ драйвера, timeout, некорректные параметры.
- **DSP correctness:** golden IQ с AM/NFM/WFM; частота тона/спектра и ожидаемые уровни в пределах утверждённых допусков (O).
- **Audio correctness:** каналы/частота аудио, отсутствие систематических underrun при согласованном профиле, валидность WAV.
- **Waterfall correctness:** частота и длительность на тестовом тоне, тест режимов backpressure; FPS не влияет на непрерывность IQ.
- **Plugin contract:** major mismatch reject, optional minor features, clean teardown, callback lifetime, errors, hot disable только при отсутствии активных вызовов.
- **Release:** package boots on both OS, dynamic libraries and LICENSE/NOTICE included, reproducible manifest/dependencies documented.
- **Performance:** 2.4/10/20 MSPS separately per available hardware, CPU load, memory, frame pacing, IQ continuity, latency, storage throughput.

### 10.2. Ещё не согласованные количественные SLO (O)

CPU и RAM эталонного ноутбука; минимальный sustained FPS и условия его измерения; end-to-end audio latency; tolerable sample drops, thresholds на underrun; минимальная длительность burn-in; допустимая warm/cold start; разница между требуемой и поддерживаемой скоростью конкретного SDR; допустимое время выбора устройства, stop и reconnect. **Нельзя объявить pass/fail по этим метрикам, пока они не определены и не утверждены.**

### 10.3. Шаблон записи измерения

`OS/build; CPU/RAM/iGPU; power mode; USB bus; SDR model/firmware/driver; sample format; target/actual MSPS; FFT window/size; demod mode; IQ drops and discontinuities; audio underruns; GUI FPS p50/p95; CPU p50/p95; RAM max; recording write bandwidth; runtime duration; logs; verdict.`

## 11. Этапы реализации, зависимости и Definition of Done

| Фаза | Цель | Поставляемое свидетельство | Gate |
|---|---|---|---|
| 0 Foundation | CMake/vcpkg, CI, контракты слоёв, license policy, ADR | Headless build/test Windows+Linux | G0 |
| 1 RX PoC | HackRF/RTL-SDR, Soapy adapter, IQ queues, FFT/waterfall | Устойчивая демонстрация, метрики потерь | G1 |
| 2 DSP/Audio | AM/NFM/WFM, файл IQ, golden tests, audio output | Демодуляция с контрольными сигналами | G2 |
| 3 GUI vertical slice | QML shell, spectrum renderer, управление, минимальный dock | GUI не блокирует RX при перегрузке | G3 |
| 4 SDK v0.1 | ABI, Source+Demodulator, C++ SDK и Rust bindings | Оба примера собираются и загружаются на обеих ОС | G4 |
| 5 Recording/Config | WAV, Raw IQ+metadata, settings, diagnostics | Golden recordings и валидные файлы | G5 |
| 6 Release hardening | Airspy integration, burn-in, benchmark matrix, licenses | Pass F-001…F-014, open issues triaged | G6 |

**Параллельные ветки:** документирование интерфейсов, лицензирование, unit tests и UX-тестирование ведутся с самого начала; финальная стабилизация не заменяет их. В первую очередь стоит сделать узкий end-to-end slice «IQ -> FFT -> GUI» до усложнения UI.

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

## 13. Минимальный пакет документации и процесса разработки

- `docs/adr/ADR-001...ADR-012.md` — по одному ADR на решение, пересмотр только отдельным ADR.
- `docs/architecture/context.md`, `components.md`, `threading.md`, `data-flow.md`, `error-model.md`.
- `docs/plugin-sdk/abi.md`, `source.md`, `demodulator.md`, `lifecycle.md`, `examples.md`.
- `docs/quality/acceptance-matrix.md`, `benchmark-protocol.md`, `hardware-matrix.md`.
- `LICENSE`, `THIRD_PARTY_NOTICES.md`, `SBOM` релизных бинарников, фиксированные toolchain manifests.
- Инструкции для AI-агентов: менять только назначенный модуль, не расширять публичный ABI без ADR, не копировать сторонний код без проверки лицензии, добавлять тесты и benchmark при изменениях DSP.

## 14. Источники и примечания к проверке

Официальная и первичная документация, проверенная 06.11.2026:

1. Qt 6 Licensing — https://doc.qt.io/qt-6/licensing.html
2. Qt Quick QSGRenderNode — https://doc.qt.io/qt-6/qsgrendernode.html
3. Qt custom QSGRenderNode example и QRhi caveat — https://doc.qt.io/qt-6/qtquick-scenegraph-customrendernode-example.html
4. vcpkg Manifest Mode — https://vcpkg.io/en/docs/users/manifests.html
5. vcpkg Versioning — https://learn.microsoft.com/en-us/vcpkg/users/versioning
6. liquid-dsp license and optional FFTW issue — https://github.com/jgaeddert/liquid-dsp
7. SigMF — https://sigmf.org/

**Юридическая оговорка:** совместимость конкретных сборок, пакетов и способов распространения лицензий требует отдельного анализа; документ не является юридическим заключением.

---

**Статус Architecture Baseline 1.0:** 12 решений зафиксированы; функциональная модель согласована с оговорённым сокращением MVP; список O-001…O-020 остаётся открытым. Документ пригоден для запуска этапа G0, но не заменяет готовые детальные C ABI headers или измеренные performance benchmarks.
