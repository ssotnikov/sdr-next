# Device Provider ABI v1.0

Контракт: `sdk/include/sdr/device_plugin.h`, решение ADR-017. Не меняет прежний Source/Demodulator ABI. Модуль — поставщик аппаратного семейства, который перечисляет физические экземпляры; хост не знает его семейства. Независимый пример: `plugins/device-example`, использует только SDK и стандартную библиотеку.

## Загрузка

В applicationDir/devices устанавливаются соседние JSON manifest и DLL/SO. Поля manifest: id, version, api_major=1, api_minor=0, kind=device, platform=Windows/Linux, arch=x86_64, license, library (только basename). ID provider должен совпадать с возвращённым C ABI. Дубликаты/другая платформа/неизвестный ABI/неверная библиотека отклоняются отдельно от прочих providers. Библиотеки из cwd/PATH без manifest не сканируются. Это доверенный нативный код, не sandbox.

Entry point `sdr_device_plugin_init(major, minor, out_api)` получает занулённую структуру с struct_size. Обязательны enumerate/create/destroy/read/last_error, ожидаемый major, поддержанный minor и полный размер v1.0. Возвраты — SDR_OK, SDR_ERROR и SDR_EOF. C++ exceptions/panics не пересекают ни один callback.

## Устройства и возможности

`enumerate(0, out)` начинает свежий snapshot, последующие индексы читают его до SDR_EOF. Максимум 256 устройств. key (1024 bytes) уникален внутри provider и стабилен между refresh; label (256 bytes) отображается в GUI. Строки UTF-8 с NUL. Не помещающийся ключ отклоняется, не обрезается. Host ID — `device:<provider-id>:<key>`; provider получает свой raw key.

Descriptor имеет до 64 положительных конечных rate presets (список не обязательно исчерпывающий, ввод rate остаётся доступным), frequency/gain ranges и flags GAIN/PPM/BIAS_T/DIRECT_SAMPLING. Они управляют доступностью GUI, без проверки имён семейств в хосте. Пустой список — ручной rate. Неподдерживаемые PPM/Bias-T/direct параметры должны отклоняться; недоступный gain не применяется. Точную поддержку каждого параметра проверяют физическими испытаниями.

## Открытие и IQ

`create(key, requested, actual)` возвращает opaque instance либо null с last_error. Config содержит struct_size, sample_rate/center_hz/gain/ppm, bias_t (0/1), direct_sampling (0/1/2). Host передаёт значения с правильным размером; provider возвращает фактически применённую конфигурацию. Значения конечные, RX host v0.1 принимает Fs 48 k…20 MSPS. Ошибка после частичного открытия освобождает ресурсы внутри create. Host уничтожает instance, если actual оказался некорректным.

`read(instance, out, capacity, count, flags)` заполняет host-owned массив sdr_iq (float32 I/Q). Возвращает count ≤ capacity, DISCONTINUITY после потери IQ, SDR_OK/count=0 при timeout, SDR_ERROR при отказе, SDR_EOF при завершении. Provider не удерживает буферы. Host преобразует IQ в внутренний Block с sequence/sample_index, проверяет finite/capacity/flags. Soapy получает параметр ожидания 100 мс. Настенная длительность чтения также включает работу адаптера и время вне исполнения потока. После пяти секунд без IQ движок завершает RX при очередном пустом результате.

Instance создаётся/читается/уничтожается одним acquisition worker; enumerate выполняется при остановленном RX. Контракт не предоставляет параллельных callbacks/фоновых обращений в host. Фактические аппаратные callback-потоки зависимого SDR runtime завершаются destroy до выгрузки. Библиотека удерживается shared ownership всех instance до destroy. Конфигурация применяется stop/recreate. Refresh во время RX отклоняется.

При включённой трассировке хост может вызвать необязательный символ `sdr_device_read_diagnostics_v1` вместо `read`. Он выполняет одно чтение по тем же правилам. Дополнительный выход содержит длительность вызова драйвера в микросекундах и исходный код или число полученных отсчётов. Длительность −1 означает отсутствие замера, тогда код игнорируется. Soapy передаёт исходный результат `readStream`, включая timeout (−1) и overflow (−4), сохраняя обычную обработку обоих случаев. Старые модули продолжают работать через `read`. [ADR-025](ADR-025-device-read-diagnostics.md)

`last_error()` возвращает thread-local UTF-8 до 511 bytes + NUL, валидный до следующего вызова того же потока. Host немедленно копирует текст. Host и provider не освобождают память друг друга. Ошибка/отсутствие отдельного provider или runtime не означает ошибку всех источников.

## Поставка и миграция

HackRF/RTL-SDR/Airspy — отдельные sdr_device_*.dll/so, общий Soapy helper приватен модулям. SoapySDK/runtime отсутствуют в core. Внешний runtime должен быть доступен загрузчику ОС. При отсутствии Soapy или его driver provider фиксирует локальную ошибку/пустой список, приложение сохраняет file/synthetic и независимые providers. Реальные RTL-SDR/Airspy испытания отложены ADR-016.

Старый `soapy:<key>` мигрируется только в единственный установленный `device:...:<key>`. Отсутствие/неоднозначность не запускает другое устройство. Набор модулей меняется при stopped refresh/следующем запуске. Старый Source SDK по manifest продолжает давать IQ, но аппаратные модули используют Device Provider ABI.
