# Design

## Decisions
[ADR-019](../../../../docs/sdr/ADR-019-rx-worker-wakeup.md): generation counter и stop-aware condition variable для внутреннего worker. Data push, command post и acquisition completion уведомляют worker. Counter читается перед обработкой очередей; publish revision и notify выполняются под wake mutex, исключая потерянное уведомление перед wait.

Регрессия сначала измеряет число idle waits текущего polling worker на пустом каталоге устройств; прежний код должен многократно просыпаться без работы. После замены число не растёт в idle; command/RX/stop/error должны продолжать работать. Существующий 200 ms stall/overflow тест сохраняется: переполнение не скрывается.

Добавляются max spectrum/audio-push/полного блока, чтобы не исключать ещё не измеренные участки. Эти maxima — wall time; причина внешнего scheduling stall требует отдельных свидетельств. MMСSS, timer resolution, размеры очередей и новые пороги этим изменением не вводятся.
