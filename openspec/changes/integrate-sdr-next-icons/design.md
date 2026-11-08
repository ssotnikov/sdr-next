# Design

## Decisions
[Контракт ресурсов](../../../docs/sdr/PRODUCT_ICON_INTEGRATION.md). PNG 16–512 входят в executable через Qt resources; QIcon содержит все варианты. Runtime не требует внешней папки assets или нового SVG plugin. Windows RC использует ICO 16–256 с PNG payload; CMake формирует RC из template с абсолютным путём ресурса. Исходные картинки не редактируются.

## Validation
Сейчас: проверить размеры и структуру PNG/ICO, неизменность исходных файлов, детерминированность упаковки и diff/OpenSpec. При следующей сборке проверить RC compilation и иконки EXE/обычного и тестового окна/панели задач. Активный длительный тест не прерывать и не нагружать сборкой. До runtime-проверки change не архивировать.
