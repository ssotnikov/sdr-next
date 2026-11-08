# Примеры будущего контракта оформления

Это проектные fixtures JSON v1, не установленные профили текущей сборки. Формат: ../../appearance-resource-v1.schema.json. Семантика: ../../APPEARANCE_PROFILES.md.

- lab-slate.ui-theme.json — собственная синяя UI theme с двумя основами.
- midnight-plot.spectrum-style.json — всегда тёмный спектр, независимо от UI.
- quiet-waterfall.waterfall-style.json — отдельные background и noData waterfall.
- lab-signal.color-map.json — карта с опорными точками.
- grey-scale.color-map.json — собственный пример массива цветов, не копия данных SDR++.
- settings-appearance.fragment.json — только фрагмент объекта settings, не файл профиля и не полный settings.json; не импортировать как ресурс.

Проверка структуры в PowerShell 7: Test-Json -Json (Get-Content -Raw <resource.json>) -SchemaFile docs/sdr/appearance-resource-v1.schema.json. Фрагмент settings проверяется отдельно. Schema не заменяет будущую семантическую проверку stops/endpoints, контраста и файловых квот.

## Проверка проектных fixtures — 2026-11-08

PowerShell Test-Json: 5 валидных примеров приняты, 5 ошибочных вариантов отклонены (format, неизвестное поле, отсутствующий light, stops+colors вместе, alpha в карте). Отдельно проверены порядок/endpoints stops и 34 пары контраста примеров после подстановки defaults. Это проверка контракта/примеров, не выполнение будущих unit/render/QML/benchmark задач приложения.

## Classic Teal — схема со скриншота пользователя

- [classic-teal.ui-theme.json](classic-teal.ui-theme.json) — основной запрошенный JSON подключаемой UI theme; две основы, акцент #72D5D2.
- [classic-teal.spectrum-style.json](classic-teal.spectrum-style.json) — тёмное поле #101C2C, линия #7EE4E0, янтарный readout #E8B979.
- [classic-teal.waterfall-style.json](classic-teal.waterfall-style.json) — независимые цвета waterfall.
- [classic-teal.settings-appearance.fragment.json](classic-teal.settings-appearance.fragment.json) — пример выбора всего набора после импорта трёх профилей и карты builtin:ocean. Это фрагмент настроек, не импортируемый ресурс.

Для будущей встроенной поставки выбраны те же данные с namespace builtin:classic-teal. Внешний импорт даёт user:classic-teal, поскольку namespace назначает каталог, а не JSON. Dark основан на старых исходниках и PNG; light — отдельно обозначенная адаптация. В текущей сборке импорт профилей ещё не реализован, эти файлы не означают уже установленный пункт меню.

Проверка Classic Teal: все три JSON соответствуют schema v1; проверены 68 пар контраста UI/графиков, включая связанный пример Midnight Plot после добавления readout. Восемь resource fixtures в каталоге проходят Test-Json. Числовые параметры RX/устройства со скриншота в профили не входят.
