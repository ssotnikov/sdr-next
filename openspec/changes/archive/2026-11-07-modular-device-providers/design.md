# Design

## Context

См. proposal.md. Нынешний core содержит Hardware/Soapy и явный whitelist семейств. Engine уже отделяет acquisition/DSP/GUI и умеет удерживать native библиотеки для Source-плагинов. Новый контракт устройств должен охватить недостающие capabilities и enumeration, не ломая ABI v1.0 старых расширений.

## Goals / Non-Goals

**Goals:** один Device Provider ABI для всех аппаратных модулей; core без Soapy и списка семейств; опциональная установка; проверяемый lifetime и отсутствие глобальной ошибки при отсутствии железа/runtime.

**Non-Goals:** sandbox/isolated host, hot unload во время RX, интерфейсы TX, реальная квалификация RTL-SDR/Airspy (ADR-016), изменение DSP pipeline или SLO.

## Decisions

- [ADR-017](../../../../docs/sdr/ADR-017-device-provider-modules.md): отдельный C ABI Device Provider v1.0, сохраняющий старый Source/Demodulator ABI. Общий POD descriptor: ключ экземпляра, label, rates, диапазоны и capabilities. Config: rate/center/gain/ppm/bias/direct. Callbacks: enumerate/create/read/destroy/last_error; actual config возвращается при create. Буферы IQ выделяет host; instance создаёт/освобождает module. Fixed arrays и struct_size/major/minor ограничивают размер и проверку.
- Три DLL/SO собираются из общего приватного Soapy backend и отдельной точки входа с family/id из сборки. Семейства известны только самим модулям. SDK/runtime Soapy не линкуются в core или GUI.
- Engine сканирует JSON manifest только в applicationDir/devices, проверяет platform/arch/version/basename/duplicate, загружает DeviceModule и перечисляет устройства. Запись registry связывает непрозрачный host ID с module/key. Ошибки отдельных модулей остаются локальными событиями, source/file продолжают работать. В тестах путь инъецируется отдельным каталогом.
- DeviceSource удерживает shared module до destroy и адаптирует единый C ABI к внутреннему Source/CF32. Источник создаётся/читается/закрывается в acquisition worker; GUI получает реальные capabilities и applied rate/center через существующий snapshot. Конфигурация меняется существующим stop/recreate.
- Ключи, labels и error ограничены 1024/256/512 bytes; rates максимум 64, устройств максимум 256 на provider. Некорректные числа/длины/выход за capacity отклоняются.
- При проверке обнаружен неверный dependency prefix MSVC/Ninja: CMake перекодировал UTF-8 вывод cl как OEM, старые объекты не пересобирались после изменения ABI заголовков. Root CMake определяет фактический /showIncludes prefix через ENCODING NONE и передаёт Ninja исходные bytes. Проверка включает чистую сборку и повторную сборку после изменения timestamp заголовка.
- Discovery при запуске и ручном обновлении только в Idle; файловый hot watcher не нужен. Соседняя библиотека manifest — установленный доверенный код. Другие папки/PATH для manifest не сканируются. Внешние зависимости конкретного модуля разрешаются ОС.

## Risks / Trade-offs

- Native crash → остаётся общей границей доверия ADR-003, не выдаётся за изоляцию.
- Soapy runtime отсутствует → такой provider выдаёт локальную диагностическую ошибку; прочие providers не зависят от него.
- ABI расширение → отдельная версия/entrypoint исключает повреждение существующих Rust/Source/Demod plugins.
- Медленный native вызов → аппаратные read timeout ограничены 100 ms, прежний 5 s no-IQ timeout сохраняется. Произвольно зависший сторонний native код не изолируется.

## Migration Plan

Сохранить старые пакеты, собрать новые устройства в bin/devices и копировать их отдельно в пакет. ID становится device:<provider-id>:<device-key>. Старый soapy:<key> выбирается только при единственном совпадении key, иначе остаётся недоступным. Benchmark получает --module и необязательный --device key. Обновить SDK/user/developer docs и связь родительского MVP с новым change. Отсутствие отдельных библиотек проверить на отдельном чистом каталоге.
