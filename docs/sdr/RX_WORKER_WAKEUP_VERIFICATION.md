# Проверка пробуждения RX worker

Дата: 2026-11-08. OpenSpec: `wake-rx-worker-on-data`, [ADR-019](ADR-019-rx-worker-wakeup.md).

## Обратная связь и реализация

До изменения regression `engine` завершалась за 0.45 s с ошибкой `Idle worker must not poll periodically without commands or IQ`: worker просыпался на пустой очереди без событий. Проверка использует настоящий Engine с пустым каталогом модулей и 200 ms простоя; отдельная команда затем должна разбудить его. Это подтверждает недостаток polling, не воспроизводит первопричину каждой длинной аппаратной паузы.

После изменения worker блокируется на stop-aware notification. Generation читается до обработки очередей; producers изменяют revision под тем же mutex, который используется для wait. Поэтому уведомление между empty check и переходом в wait не теряется. Data/commands/source completion будят worker; деструктор прерывает wait через stop token. Существующая регрессия на искусственный 200 ms stall всё ещё требует overflow и сохранения diagnostic maxima: реальная нехватка буфера не скрывается.

Добавлены maxSpectrumMs, maxAudioPushMs, maxBlockMs и idleWaitEntries. Размеры очередей, audio prefill, sample rate и thresholds не менялись. GUI/SDK контракты прежние.

## Проверки и пределы

Итоговая сборка MSVC 2026 и CTest **7/7 pass**, включая idle/no-lost-command, file EOF, source-module lifecycle, settings и QML. При первой промежуточной серии `settings` один раз завершился с 0xc0000409; последующие 10 и 100 последовательных повторов прошли. Причина единичного отказа не установлена, его нельзя объявлять исправленным. Артефакты: `build/wakeup-after-tests.log`, `build/wakeup-settings-repeat.log`, `build/wakeup-settings-100.log`, `build/wakeup-final-tests.log`.

Один дополнительный запуск msvc-dependencies был ошибочно выполнен без VS developer environment и не нашёл include dependencies; после загрузки DevShell вся серия прошла. Это ошибка окружения проверки, не свидетельство сбоя RX.

Во время диагностики на компьютере наблюдалась меняющаяся доступная физическая память: CIM давал около 0.6 GiB, другой срез performance counters — 2584 MiB Available, с PageReads/sec 47 и PagesInput/sec 492; WSL имел около 4.1 GiB private allocation. Это текущие срезы, **не измерения в момент исходного failure**, и причинная связь с 405 ms паузой не доказана. Чужие процессы/службы не останавливались. MMCSS, системные таймеры и power plan не изменялись.

## Аппаратная проверка

`build/endurance/qualification-20261108-wakeup`: реальный HackRF GUI/audio/WAV, 2.4 MSPS, WFM, прежние буферы, **120.9998 s**, штатное завершение и process/test exit=0. IQ gaps/overflows/audio underruns/drops — 0. Max полного блока 3.5433 ms, spectrum 0.907 ms, worker block gap 76.9668 ms. 2259 переходов в event wait за весь короткий RX; idle regression отдельно подтверждает отсутствие периодического опроса без работы. Минимум отображённых свежих спектров 16.991 fps. WAV 11712638 bytes, RIFF/data lengths проверены независимо.

Память достигла **538.891 MiB**, выше порога 512 MiB. Короткая проверка нового ожидания прошла, количественная приёмка памяти — нет. Это не восьмичасовой pass и не доказательство, что все редкие системные stalls устранены. Физическая/input latency не измерены. Исходная 405 ms пауза не воспроизведена в коротком RX; причина единичного промежуточного settings crash остаётся отдельным открытым предупреждением для дальнейшей квалификации.
