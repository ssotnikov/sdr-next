# Восьмичасовой RX с защищённым управлением — 2026-11-08

Статус: **FAILED после 757 s = 12 мин 37 с**. Владелец OpenSpec — `plan-mvp-v0-1`, задача 7.4 остаётся невыполненной до фактической приёмки.

- Начало измеряемых 28800 секунд: **2026-11-08 15:31:54 МСК**.
- Ожидаемое завершение: **2026-11-08 23:31:54 МСК**, затем финализация записи.
- Receiver PID: 36220; runner PID: 28824, только для этого запуска.
- Код: `ada94ed`, чистый worktree при запуске. Executable SHA-256: `F459F18001207EF69884C76E4F79544E8817E5637DA147844E505FBB2EF63046`.
- Артефакты: `build/endurance/20261108-8h-guarded/`: `environment.json`, `process.json`, `session/metrics.jsonl`, итоговый `session/summary.json` и WAV. Старые прогоны сохранены отдельно.

Профиль прежний: HackRF One, 100 MHz, gain 16, 2.4 MSPS, WFM, PPM 0, Bias-T/direct off; native Windows GUI, системный аудиовыход, WAV PCM16 mono 48 kHz. Сборка MSVC 2026/Qt 6.10.0 в `build/windows-retry-msvc2026`, 7/7 CTest до запуска. Буферы, prefill, backpressure и критерии ADR-015 не изменены.

Через UI подтверждены аппаратный приём, индикатор записи, footer actual 2.400 MSPS и отключённые элементы управления. Фактические параметры берутся из status/raw metrics; значения недоступных для редактирования полей не используются как доказательство частоты дискретизации. Space/Escape/Ctrl+O и конфигурационные панели не меняют сеанс. При failure окно остаётся с причиной и сохранённым summary; runner отмечает `failed_window_open`. Закрытие окна вручную прерывает тест.

Первые 50 секунд: running/recording=true, actual rate=2400000, IQ gaps/overflows и audio underruns/drops=0; средний CPU около 1.95%, max sampled working set **531.703 MiB**, выше 512 MiB. WAV вырос до 4 911 104 bytes, окно visible/exposed=true, AC подтверждено. Лимит памяти вновь нарушен; запуск не объявляется успешной полной приёмкой. Физическая audio latency и UI input latency по-прежнему not_verified.

Для измерений оставить окно видимым, ноутбук открытым и подключённым к сети. Автоматический сон предотвращается только до окончания теста; power plan не меняется. После завершения нужно исследовать summary, весь JSONL и WAV и сопоставить с двумя предыдущими попытками.

## Итог

В **15:44:33 МСК** сохранён state=failed с ошибкой `IQ queue overflow: processing/storage cannot keep up; RX stopped`. Окно осталось открытым, runner отметил `failed_window_open`, как и предусмотрено исправлением. Позднее окно было закрыто; process exit=0 не изменил test_exit_code=2.

Итог: IQ overflow 1, hardware discontinuity gaps 0; audio underruns 18, audio drops 7; CPU average 1.625%, max sampled working set 531.703 MiB. 757 строк JSONL и WAV 72781706 bytes прочитаны/проверены; WAV RIFF/data size соответствует файлу, длительность около 758.142 s с учётом старта записи до измеряемого интервала.

Ключевые maxima wall time: worker block gap **404.860 ms**, source read **243.267 ms**, audio push gap **290.116 ms**, audio read gap 106.942 ms. DSP максимум 14.644 ms, writer 9.654 ms, snapshot publish 9.072 ms, enqueue wait 49.973 ms при запрошенном ожидании 20 ms. Сам timeout не гарантирует пробуждение точно через 20 ms. Эти данные указывают на длинные промежутки без обработки, но не устанавливают их единственную причину. FFT и audio push внутри полного блока тогда ещё не измерялись отдельно.

В 746 s появились первые 6 audio underrun и worker gap 202 ms; в 757 s появились 7 audio drops, source read 243 ms и worker gap 290 ms, а финальный summary сохранил gap 405 ms и overflow. Это отличается от второго прогона со штатным stop/start и подтверждает, что защита UI сама по себе не устраняет первоначальный сбой.

По результатам исправлен отдельный воспроизводимый недостаток ожидания — polling с sleep(1 ms), см. [ADR-019](ADR-019-rx-worker-wakeup.md) и [проверку изменения](RX_WORKER_WAKEUP_VERIFICATION.md). Это не объявляется доказательством устранения всех системных stalls. Согласованные пороги, очереди и fail-on-overflow не изменены.
