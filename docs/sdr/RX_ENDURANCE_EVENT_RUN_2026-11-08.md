# Длительный RX после замены polling — 2026-11-08

Статус: **RUNNING, полная приёмка не подтверждена**. Пользователь повторно поручил запустить приложение и тест. Владелец — OpenSpec `plan-mvp-v0-1`, задача 7.4 остаётся открытой.

- Измеряемое начало: **8 ноября 2026, 18:46:01 МСК**.
- Ожидаемое завершение 28800 секунд: **9 ноября 2026, 02:46:01 МСК**, затем штатная финализация WAV.
- Receiver PID: 23200; runner PID: 6576, только для этого запуска.
- Код: `f5a6fcb`, при запуске worktree чистый. Executable SHA-256: `F97BF9A28FFD21CB72D53B208703E10D927F25D864F875E6E71E99EB09B86929`.
- Локальные результаты: `build/endurance/20261108-8h-event-wakeup/`: `environment.json`, `process.json`, `session/metrics.jsonl`, итоговый `session/summary.json` и WAV. Предыдущие попытки сохранены отдельно.

HackRF One, 100 MHz, gain 16, actual 2.4 MSPS, WFM, PPM 0, Bias-T/direct off; native GUI, системный audio output и WAV PCM16 mono 48 kHz. SDK/драйверы/питание и hashes фиксирует environment.json. Используется сборка MSVC 2026/Qt 6.10.0 из `build/windows-retry-msvc2026`. DSP worker ожидает notifications по ADR-019; размеры очередей, backpressure и согласованные пороги прежние.

Через UI подтверждены активный приём, индикатор записи, actual footer 2.400 MSPS и отключённые панели управления. За первые 32 s IQ gaps/overflows и audio underruns=0; active WAV вырос до 3 207 168 bytes. Working set достиг **555.496 MiB**, выше лимита 512 MiB. Это сохраняется как нарушение, а не скрывается успешным запуском. Физическая audio latency и UI input latency остаются not_verified.

Оставить окно видимым, ноутбук открытым и подключённым к сети; не менять параметры измеряемого сеанса. При failure окно остаётся с причиной и сохранённым summary, runner отмечает failed_window_open. При ручном закрытии тест прерывается. Итоговую длительность, RX/audio counters, stage maxima и WAV необходимо проверить после окончания; начало прогона не закрывает MVP.
