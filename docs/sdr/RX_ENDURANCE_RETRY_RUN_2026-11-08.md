# Повторный восьмичасовой RX — 2026-11-08

Статус: **RUNNING; результат восьми часов не подведён**. Это диагностический повтор по поручению пользователя после [подробного разбора](RX_ENDURANCE_RETRY_ANALYSIS.md), не заявление об устранении первоначальной причины.

- Начало измеряемого интервала: **13:54:22 МСК 2026-11-08**.
- Ожидаемое завершение 28800 секунд: **21:54:22 МСК 2026-11-08**, затем штатная финализация.
- Процесс SDR Next: PID 12936; runner: PID 38476. PID относятся только к этому запуску.
- Ревизия: `bb5a3e6`, worktree при запуске чистый. SHA-256 executable: `99F9BEA29605C0DD041405D1AF73BC5A78E365FF7DF4B26BDC059F7B32F4386E`.
- Сборка: `build/windows-retry-msvc2026`, MSVC 2026 / Qt 6.10.0 msvc2022_64. Итоговая CTest серия **7/7 pass**, включая 200 ms stall/overflow regression и audio accounting. Отдельный 61 s реальный HackRF прогон проверил RX/audio/WAV и новые метрики, но не прошёл лимит памяти.
- Локальные результаты: `build/endurance/20261108-8h-retry/`; окружение/версии/hashes — `environment.json`, процесс — `process.json`, raw metrics — `session/metrics.jsonl`, финал — `session/summary.json`, WAV — в `session/`. Старый run `20261108-8h` сохранён.

HackRF, 100 MHz, gain 16, 2.4 MSPS, WFM, PPM 0, Bias-T/direct off, native Windows GUI, системный аудиовыход, WAV mono PCM16 48 kHz. Те же queue capacities, 20 ms backpressure, audio prefill и пороги. AC подтверждено. Метрики расширены maxima стадий, audio frame counters, sink timings/state, видимостью/экспозицией окна. Эти wall-time метрики различают участки задержки, но не устанавливают причину автоматически.

Первые 39 s: реальный RX, аудио и запись активны, audio underrun/drops и IQ gaps/overflows 0. Активный WAV достиг 3 870 720 bytes. Working set уже достиг 532.715 MiB, поэтому лимит 512 MiB вновь нарушен. Минимум displayed FPS в начальных строках около 9.16, далее около 18; стартовый интервал не исключён задним числом. Полный прогон продолжается для диагностики; положительный итог по всем критериям не заявляется.

Приложение удерживает Windows от автоматического сна на время процесса. Для сравнения FPS окно следует оставить видимым и не сворачивать; не закрывать крышку, питание — от сети. Закрытие окна прервёт тест. Физическая audio latency и UI input latency остаются not_verified; queued GUI ping — только proxy. После завершения задача 1.4 OpenSpec требует сравнения двух полных наборов данных и фактического pass/fail/not_verified.
