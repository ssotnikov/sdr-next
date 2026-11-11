# Microsoft Redistributables в Windows Preview

Проверка относится к локальному `sdr-next-0.1-preview-20261111-win64-qt-notices`. Все четыре файла побайтово совпали с оригиналами на машине сборки. Windows подтвердила действительность подписей Microsoft Corporation. В метаданных файлов флаг prerelease отсутствует. Visual Studio Professional 18.11.12328.437 установлена из канала `VisualStudio.18.Release`.

| Файл | Версия | SHA-256 |
| --- | --- | --- |
| `d3dcompiler_47.dll` | 10.0.26100.9457 | `1da540a0ddbc54de4142570c8ec4a4566ef610fce72c181efb55a9332c480790` |
| `dxcompiler.dll` | 1.9.2602.17 | `5da37a35d53123274e2e4e7e247148350871df8ae9ecc05778bf2ac2057932be` |
| `dxil.dll` | 1.9.2602.17 | `ba2f78f784b39b6e1af3a90ec99ab794953b09f4f617a5db765f800bec22d360` |
| `vc_redist.x64.exe` | 14.51.36247.0 | `843068991daaa1f73ad9f6239bce4d0f6a07a51f18c37ea2a867e9beca71295c` |

## Windows SDK

Три DLL взяты из `Windows Kits/10/Redist/D3D/x64`. Установленный `Licenses/10.0.26100.0/sdk_license.rtf` отсылает к перечню REDIST. Microsoft указывает все три пути в разделе классических Windows-приложений. Публичная документация относит сборку 10.0.26100.9457 к Windows SDK для Windows 11. В System32 эталонной машины находится другой `d3dcompiler_47.dll` версии 10.0.26100.9818. Файлов `dxcompiler.dll` и `dxil.dll` там нет. [Перечень Windows SDK REDIST](https://learn.microsoft.com/en-us/legal/windows-sdk/redist) · [Версии Windows SDK](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads)

Установленная лицензия и опубликованный раздел Distributable Code допускают поставку перечисленного объектного кода с приложением при соблюдении условий. Среди них сохранение уведомлений, использование на Windows, наличие самостоятельной функциональности и согласие получателей и распространителей с условиями, защищающими поставляемый код. Для публикации нужно сверить применимые условия выпускающего лица и способ доведения этих условий до получателей. [Условия Windows SDK, раздел 2(a)](https://learn.microsoft.com/en-us/legal/windows-sdk/license#2-additional-licensing-requirements-andor-use-rights)

На эталонной Windows 11 отдельно вызвана `D3DCompile` из каждой копии `d3dcompiler_47.dll`. В двух процессах проверен полный путь загруженной DLL. Обе версии скомпилировали минимальный пиксельный шейдер `ps_5_0` и вернули 452 байта DXBC. Замена файла в поставке требует отдельной проверки приложения на целевых Windows.

## DirectX Shader Compiler

Обе DLL из SDK сообщают версию `1.9.2602.17 (21d28f727)`, которая совпадает с версией бинарников публичного релиза `v1.9.2602`. SHA-256 официального ZIP совпал с опубликованным GitHub значением `a1e89031421cf3c1fca6627766ab3020ca4f962ac7e2caa7fab2b33a8436151e`. Бинарники из ZIP и SDK различаются и после исключения таблицы сертификатов и связанных полей PE из сравнения. Для поставляемой пары основанием аудита остаются происхождение из Windows SDK, его перечень REDIST и сопроводительные материалы SDK. Файлы лицензий из ZIP относятся к публичному релизу. [Релиз DXC](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2602) · [Ревизия DXC](https://github.com/microsoft/DirectXShaderCompiler/commit/21d28f727ad395b59394815ef76012e432f7e4e5)

В отдельной папке подписанный `dxc.exe` из этого релиза с парой DLL из Windows Preview скомпилировал минимальный пиксельный шейдер `ps_6_9`. Результат размером 2 540 байт прочитан командой `dxc -dumpbin`. Проверка охватывает работу компилятора в эталонной Windows 11. Использование Shader Model 6.9 самим SDR Next и выполнение этого шейдера на GPU остаются за пределами проверки.

## Visual C++ Runtime

Установщик взят из `VC/Redist/MSVC/14.51.36231` в Visual Studio Professional. Перечень Visual Studio 2026 охватывает этот каталог с указанными Microsoft исключениями. Право распространения зависит от действующей лицензии Visual Studio и её условий. Канал Release подтверждает происхождение установленного инструментария. Право выпускающей стороны на распространение по конкретной лицензии или подписке остаётся непроверенным. Условия Professional отдельно описывают ограничения пробного режима и требуют согласия внешних пользователей с условиями распространения Distributable Code. [Перечень Visual Studio 2026 REDIST](https://learn.microsoft.com/en-us/visualstudio/releases/2026/redistribution#visual-c-runtime-files) · [Лицензия Visual Studio 2026 Professional](https://visualstudio.microsoft.com/license-terms/vs2026-ga-pro-enterprise/)

`sdr_app.exe` напрямую импортирует `MSVCP140.dll`, `MSVCP140_ATOMIC_WAIT.dll`, `VCRUNTIME140.dll` и `VCRUNTIME140_1.dll`. В предыдущем [сравнительном прогоне](MVP_V0_1_VERIFICATION.md#проверка-windows-preview-без-дополнительных-графических-dll) Windows загружала VC++ Runtime из System32. Отдельный установщик в архиве служит для установки этих библиотек. Вариант с получением x64 runtime напрямую у Microsoft требует проверки установки и первого запуска на чистых целевых системах. [Документация по развёртыванию VC++](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files) · [Загрузки Microsoft VC++ Runtime](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

Окончательный состав публичного Windows-пакета и прохождение 7.7 остаются открытыми. Проверенные файлы и права на их распространение учитываются отдельно. Для текущего ZIP-подхода требуется подтверждение применимых условий, если эти файлы войдут в публичный пакет.
