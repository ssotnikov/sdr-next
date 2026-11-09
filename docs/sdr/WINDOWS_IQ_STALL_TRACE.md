# Трассировка задержки DSP в Windows

Статус — диагностическая процедура. Проверены состав встроенных профилей и версии инструментов на машине SDR. Захват ETL и привязку временных шкал предстоит проверить коротким прогоном.

## Что измеряем

Для события `dsp` поле `duration_us` охватывает демодуляцию и вызов подключаемого демодулятора, если он активен. Поле `thread_cpu_us` — разность учтённого процессорного времени этого потока по `GetThreadTimes`. В коротком аппаратном прогоне Windows счётчик изменялся с шагом 15 625 мкс. Формат FILETIME задаёт единицу представления 100 нс, а наблюдаемый шаг счётчика зависит от учёта Windows. Нулевой прирост при задержке 100–250 мс совместим с короткими отрезками исполнения и длительным ожиданием или задержкой планирования. Значение −1 означает отсутствие замера. Причину определяем по ETW. [Данные прогона](AUDIO_UNDERRUN_DIAGNOSTICS_PLAN.md#трасса-процессорного-времени-dsp) · [GetThreadTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes)

## Захват

1. Использовать PowerShell с правами администратора для записи, а для анализа — WPA из Windows Performance Toolkit. На машине уже доступен системный WPR 10.0.26100. Найденный в PATH WPA — 6.3.9600.16384 из Windows Kits 8.1. Сначала проверить, открывает ли он пробную запись и показывает ли нужные таблицы. При проблемах с декодированием нужен актуальный WPA из Windows ADK. Для имён функций сохранить PDB точно той сборки SDR и подключить их в WPA через **Trace → Configure Symbol Paths**, затем **Trace → Load Symbols**. [WPR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/introduction-to-wpr) · [Права для записи](https://learn.microsoft.com/en-us/windows/ai/new-windows-ml/logs#prerequisites) · [Windows Performance Toolkit](https://learn.microsoft.com/en-us/windows-hardware/test/wpt) · [Символы WPA](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/load-symbols-or-configure-symbol-paths)
2. Проверить, что запись свободна, и сверить параметры установленного профиля. На WPR 10.0.26100 профиль `CPU` включает `CSwitch` и `ReadyThread`, но без `DPC` и `Interrupt`. У `GeneralProfile.Light` есть все четыре, а также `ProcessThread` и `Loader`. Для разбора стеков ожидания нужен `GeneralProfile.Verbose`, в котором проверены стеки `CSwitch` и `ReadyThread`. [Встроенный General profile](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/recording-for-basic-system-diagnosis) · [Команды WPR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options)
3. Начать с пробной записи на 20–30 с и проверить в WPA окно, маркер и события. Для воспроизведения использовать тот же бинарник и параметры RX, запустить приложение с `-TraceAudio` через штатный сценарий и заранее подготовить остановку записи по первому целевому событию. Записывать в кольцевые буферы памяти и сохранять ETL сразу после обнаружения задержки или IQ overflow, до длительного завершения приложения. Сохранить ETL рядом с CSV, summary и параметрами запуска в локальном каталоге прогона. [Режимы записи WPR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/logging-mode) · [Протокол RX](RX_ENDURANCE_PROTOCOL.md)

```powershell
# Из корня репозитория, в PowerShell с правами администратора.
# Использовать существующий каталог прогона или создать отдельный для пробной записи.
wpr -status
wpr -profiles
wpr -profiledetails GeneralProfile.Light
wpr -start GeneralProfile.Light
wpr -marker "SDR-smoke"
# При пробе остановить через 20–30 с. При воспроизведении — по первому целевому событию.
wpr -status collectors -details
wpr -stop ".\build\endurance\<run>\iq-stall.etl" "SDR IQ stall"
```

Кольцо ограничено объёмом, а глубина в секундах зависит от потока событий. На проверенной машине Light показывает 472 системных и 157 пользовательских буферов по 1 024 КиБ, Verbose — 787 и 157 соответственно. Это параметры профиля, а фактический охват нужно измерить в WPA под нагрузкой. Проверить отдельно потери приложения и `Dropped event` WPR. Даже при нуле в WPR ранние записи могли быть вытеснены кольцом. Критичен непрерывный интервал от нескольких секунд до начала задержки до возврата DSP и ближайших событий очереди. Если широкий профиль заметно влияет на воспроизведение или окно теряется до сохранения, следующий шаг — ограниченный `.wprp` с указанными системными событиями и прикладным провайдером, если он добавлен. Настроить объём буферов по измеренному охвату. Режим `-filemode` у WPR пишет в растущий файл и подходит только для отдельной ограниченной по времени записи. [Сеансы ETW](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/sessions) · [Буферы WPR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/buffers) · [Команды WPR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options)

## Как совместить CSV и ETL

В текущем CSV `time_us` — конец стадии относительно начала `AudioTrace`, а начало DSP равно `time_us - duration_us`. В строках отсутствуют PID, TID и привязка к ETL. На MSVC `steady_clock` использует QPC. Для точной привязки предлагается прикладное ETW-событие для редких длинных блоков с `iq_sequence`, точными `qpc_begin` и `qpc_end`, `qpc_frequency` и идентификаторами процесса и потока. Включить его провайдер в профиль WPR. Длительности вычислять через разность QPC, делённую на частоту. При чтении ETL напрямую проверить тип часов сеанса и использовать `PROCESS_TRACE_MODE_RAW_TIMESTAMP` для сеанса с QPC. По умолчанию `ProcessTrace` преобразует метки в системное время. [Код AudioTrace](../../app/audio_trace.hpp) · [Измерение DSP](../../app/engine.cpp) · [MSVC steady_clock](https://learn.microsoft.com/en-us/cpp/standard-library/steady-clock-struct?view=msvc-170) · [QPC](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps) · [Чтение ETL](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/ns-evntrace-event_trace_logfilew)

В WPA найти прикладной маркер через **Generic Events** и сверить `iq_sequence`. Для каждой границы использовать `t_wpa = t_marker_wpa + (qpc_boundary - qpc_marker) / qpc_frequency`, где `qpc_marker` — исходная QPC-метка того же события в ETL, а `t_marker_wpa` — его время в WPA в секундах. Сверить привязку на пробном событии. `wpr -marker` годится для проверки видимости отметок и грубой ориентации. Для точного сопоставления нужна проверенная привязка к моменту, измеренному внутри процесса. Секундные строки endurance оставляют слишком широкую неопределённость для классификации 100–250 мс. [Графики WPA](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/list-of-wpa-graphs) · [Маркер WPR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options#marker)

## Как прочитать интервал в WPA

Открыть **CPU Usage (Precise) → Timeline by Process, Thread**, отфильтровать PID и TID DSP и увеличить окно блока. Учитывать миграцию между логическими процессорами. Использовать моменты switch-out, ready и switch-in, а также столбцы `Ready`, `Waits`, `NewPrevState`/`OldState`, `NewPrevWaitReason`/`OldWaitReason` и `Readying ThreadId`. Обрезать интервалы по границам блока. При недостающих событиях оставить соответствующий участок неклассифицированным. [CPU Usage (Precise)](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis#cpu-usage-precise-graph)

| Наблюдение внутри блока | Что проверить |
| --- | --- |
| Поток в **Ready** большую часть паузы | В **Timeline by CPU** посмотреть, кто занимал доступные DSP логические процессоры в тот же интервал. Сверить приоритеты и допустимые процессоры. |
| Поток в **Waiting** после switch-out | В Verbose открыть стек переключения и `ReadyThreadStack`. Найти ожидаемый ресурс, таймер или поток, который возобновил DSP. Связать интервалы дискового ввода-вывода и hard faults с тем же потоком, если они есть. |
| **DPC/ISR** перекрывают паузу | Сопоставить **DPC/ISR Duration by CPU** с теми же логическими процессорами, измерить длительность перекрытия и открыть разбивку по модулю и функции. DPC/ISR могут прервать исполнение без смены потока, поэтому интервал между двумя `CSwitch` сам по себе может завысить время DSP. |
| Поток был запланирован большую часть блока, перекрытие DPC/ISR мало | Открыть **CPU Usage (Sampled) → Utilization by Process, Stack** и сравнить стеки с обычными блоками. Сопоставить с CPU-счётчиком и отметить расхождения. |

В отчёт по эпизоду записать номер блока, PID/TID, границы в ETL, потери и доли **Running / Ready / Waiting**, а также перекрытие DPC/ISR. Перекрытие учитывается отдельно от состояний потока. Для вывода о причине указать, какое событие объясняет паузу на критическом пути, и проверить связь на повторном эпизоде, если он доступен. [Разбор критического пути и DPC/ISR](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/cpu-analysis)
