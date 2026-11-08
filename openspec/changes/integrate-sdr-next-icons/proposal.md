# Proposal

## Why
Пользователь предоставил готовые иконки в assets/icon и поручил использовать их при следующей сборке SDR Next.

## What Changes
Встроить предоставленные PNG в Qt resources и установить application window icon. Для Windows EXE добавить многоразмерный ICO и RC resource. Сохранить исходные SVG/PNG, добавить воспроизводимую упаковку ICO без изменения пикселей.

## Capabilities
### New Capabilities
Нет.
### Modified Capabilities
- `product-identity`: иконка приложения из пользовательского набора.

## Impact
Assets, app/main.cpp, app/CMakeLists.txt и Windows resource template. Текущий endurance не останавливается; сборка и runtime-проверка выполняются при следующем сборочном этапе.
