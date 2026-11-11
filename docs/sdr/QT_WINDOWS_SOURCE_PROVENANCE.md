# Исходники Qt для SDR Next 0.1 Windows Preview

Локальный архив `sdr-next-0.1-preview-20261111-win64-qt-6.10.0-source.tar.gz` относится к Qt DLL из `sdr-next-0.1-preview-20261111-win64-system-icu`. Он содержит Qt superproject, четыре модуля, из которых происходят DLL (`qtbase`, `qtdeclarative`, `qtmultimedia`, `qtsvg`), и `qtshadertools`. Ревизии закреплены в `dependencies.lock.json` в корне архива. Ревизия Qt Multimedia дополнительно сверена с файлом `.tag` из официального архива Qt. [Архивы исходников Qt 6.10.0](https://download.qt.io/archive/qt/6.10/6.10.0/submodules/)

В `sdk-spdx/` сохранены исходные SPDX четырёх модулей из Windows Qt SDK. Таблица `binary-to-source-spdx.tsv` связывает файлы preview с их путями в SDK, идентификаторами бинарного SPDX, SHA-256 подписанного файла и SHA-1 до подписания. Файл `source-verification.txt` содержит итоги проверки исходного SPDX. Для файлов с переводом строки CRLF Windows SDK совпадение проверялось после преобразования окончаний строк в памяти.

Архив сохраняет исходники Qt. Для сборки на Windows нужны MSVC x64, Windows SDK, CMake и Ninja. Конкретная конфигурация сборки Qt для замены DLL требует проверки по функциям и совместимости с официальным SDK. Повторная сборка Windows Qt из этого архива пока не проверена. Источники и уведомления Mesa/LLVM для `opengl32sw.dll` и условия Microsoft Redist проверяются отдельно. [Развёртывание Qt на Windows](https://doc.qt.io/qt-6.10/windows-deployment.html)

## Уведомления для Windows-пакета

Передай упаковщику папку `qt-source` из проверенного архива и `NOTICE.txt` Apache Tika из [закреплённой ревизии](https://github.com/apache/tika/blob/dbcbb368e0a913e1a74cca61347fd6ce24bc6d7e/NOTICE.txt). Нужен Python 3.9 или новее в `PATH`. Выбери новую папку для пакета.

```powershell
.\scripts\package-windows.ps1 -QtRoot $qtSdk -BuildDirectory $buildDir -OutputDirectory $previewDir `
    -QtNoticeSource $verifiedQtSource -TikaNotice $tikaNotice
```

Упаковщик сопоставляет установленные DLL с бинарным SPDX Windows Qt SDK, сверяет SHA-1 с учётом подписания PE и собирает уведомления по достижимым `DEPENDS_ON`. Результат в `licenses/qt-attributions/` попадает в SHA-256 манифест пакета. Индекс включает условные зависимости из графа Qt SDK. Включение записи в индекс само по себе не подтверждает, что код компонента присутствует в DLL. [Атрибуции стороннего кода Qt](https://doc.qt.io/qt-6.10/licenses-used-in-qt.html)
