# Design

## Decisions
См. [разбор и план сравнения](../../../../docs/sdr/RX_ENDURANCE_RETRY_ANALYSIS.md). Не увеличивать очереди и не менять resampling по корреляции из одного прогона. Диагностический повтор сохраняет нагрузку, ёмкости и thresholds ADR-015/014.

Накопительные maxima: source read, queue wait, DSP, recorder write, worker block interval, snapshot publish. Для acquisition-потока используются atomics, worker поля публикуются существующим snapshot. Метрики не пишутся на диск из realtime-пути. Audio accounting и max callback/push gaps снимаются под существующим mutex; lifetime сбрасывается при новом RX, сохраняется при stop. QAudioSink state/processed time/bytesFree и видимость QQuickWindow снимаются на GUI thread.

Runtime-проверка имени SDR Next закрывается при сборке и коротком тесте, без изменения технических идентификаторов. Новый прогон использует новый каталог, старые артефакты сохраняются.
