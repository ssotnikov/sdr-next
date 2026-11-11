# Происхождение opengl32sw.dll в SDR Next 0.1 Windows Preview

Проверено 11 ноября 2026 года для `sdr-next-0.1-preview-20261111-win64-system-icu` и `sdr-next-0.1-preview-20261111-win64-qt-notices` на Qt 6.10.0 MSVC x64. Qt загружает `opengl32sw.dll` для программной реализации OpenGL. Поставщик бинарного файла установлен по совпадению SHA-256 с архивом Qt. Историческая инструкция Qt называет версии Mesa и LLVM и параметры сборки. Точный набор исходников и уведомлений для этой сборки остаётся открытым. [Qt 6.10, графика на Windows](https://doc.qt.io/qt-6.10/windows-graphics.html)

## Бинарная поставка Qt

| Объект | Проверенное значение |
| --- | --- |
| DLL из preview, Windows Qt SDK и архива Qt | SHA-256 `b04de4541863bc7d8879040a78889c4849c1b1da2784c4630f734c146c2998ce` |
| `opengl32sw-64-mesa_11_2_2-signed_sha256.7z` | SHA-256 `c61f801c1760aa24b02c7a8354323cddf368b86f8f5c34b50b3b224f7d728afd` |
| SHA-1 архива в скрипте Qt 6.10.0 | `58f948746696b17a594b2f542e87b0e831b28dc3` |
| Подпись DLL | Authenticode, The Qt Company Oy, действительная при проверке Windows |
| Идентификатор PDB | `100c1f6e-5856-4f25-8719-cac2c4b1949d`, age 1 |
| Путь PDB в DLL | `C:\work\mesa\mesa\build\windows-x86_64\gallium\targets\libgl-gdi\opengl32.pdb` |

Архив Qt содержит один файл `opengl32sw.dll`. Скрипт подготовки Windows в теге Qt 6.10.0 загружает этот архив, проверяет SHA-1 и распаковывает его. Индекс загрузок также содержит отдельные версии x64 от июня 2016 года (без подписи) и июня 2020 года (`-signed.7z`). [Архив 2022 года](https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/opengl32sw-64-mesa_11_2_2-signed_sha256.7z) · [SHA-256 Qt](https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/sha256sums.txt) · [Скрипт Qt 6.10.0](https://github.com/qt/qt5/blob/v6.10.0/coin/provisioning/common/windows/mesa_llvmpipe.ps1) · [Индекс Qt](https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/)

Сборка прослеживается до исходной неподписанной публикации Qt от 14 июня 2016 года. SHA-256 её архива `opengl32sw-64-mesa_11_2_2.7z` совпал со значением Qt `142a74bfbe7d2b1bf633925f130b29eb10ffa9fb87cc1bbd05ee2624ee81d261`. Извлечённая DLL имеет размер 20 627 456 байт, SHA-256 `d303dd60bbf11418063f4bb72292e481ca1e88d84915fbd049a21666b92c0ee0` и PE timestamp `2016-06-14 10:22:08 UTC`. После исключения таблицы сертификатов и восстановления PE checksum и записи Security Directory в памяти подписанный образ побайтово совпал с неподписанным. `dumpbin /dependents` перечисляет системные `imagehlp.dll`, `SHELL32.dll`, `ADVAPI32.dll`, `GDI32.dll`, `USER32.dll` и `KERNEL32.dll`. Такой список импортов согласуется со статической линковкой LLVM по историческому рецепту. [Исходный архив Qt 2016 года](https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/opengl32sw-64-mesa_11_2_2.7z.mirrorlist)

## Исходные релизы и рецепт Qt

Редакция Qt Wiki от 11 ноября 2016 года, автор Agocs, называет исходные архивы `mesa-11.2.2.tar.xz` и `llvm-3.6.2.src.tar.xz`. Вводный текст упоминает дополнительные патчи и ошибки оптимизации LLVM, мешавшие запуску приложений Qt Quick в сторонних сборках. Дальнейшие шаги сразу переходят от распаковки релизов к конфигурации и сборке. В этой редакции нет идентификаторов патчей, ссылок на них или шагов применения. Остаётся установить, к какой сборке и к каким деревьям исходников относится это упоминание. [Qt Wiki, редакция 28759](https://wiki.qt.io/index.php?title=MesaLlvmpipe&oldid=28759)

| Релиз | Публичные исходники |
| --- | --- |
| Mesa 11.2.2 | Архив `mesa-11.2.2.tar.xz`, SHA-256 из release notes `40e148812388ec7c6d7b6657d5a16e2e8dabba8b97ddfceea5197947647bdfb4`. Тег `mesa-11.2.2` указывает на коммит `3a9f6283f435f90ca1a2901be39ec9d629c95bb6`. [Архив исходников](https://archive.mesa3d.org/older-versions/11.x/11.2.2/) · [Release notes](https://docs.mesa3d.org/relnotes/11.2.2.html#sha256-checksums) · [Тег Mesa](https://gitlab.freedesktop.org/mesa/mesa/-/tags/mesa-11.2.2) |
| LLVM 3.6.2 | Архив `llvm-3.6.2.src.tar.xz` и его отделённая подпись. Тег `llvmorg-3.6.2` указывает на коммит `ee46064b83e7acd5a215ea5bb1c9708d75de6164`. [Архив](https://releases.llvm.org/3.6.2/llvm-3.6.2.src.tar.xz) · [Подпись](https://releases.llvm.org/3.6.2/llvm-3.6.2.src.tar.xz.sig) · [Тег LLVM](https://github.com/llvm/llvm-project/tree/llvmorg-3.6.2) |

Историческая инструкция требует Visual Studio 2015 Update 2, Python 2.7, SCons 2.4.1, CMake, Flex, Bison (3.0 или 2.4.1), M4, Mako и Python-пакет libxml2 2.9.3. Команды для Release x64 выполняются в командной строке Visual Studio. Первые две запускаются из дерева LLVM, последние две из дерева Mesa. [Qt Wiki, редакция 28759](https://wiki.qt.io/index.php?title=MesaLlvmpipe&oldid=28759)

```bat
cmake -G "Visual Studio 14 Win64" -DLLVM_USE_CRT_RELEASE=MT -DLLVM_USE_CRT_DEBUG=MTd -DCMAKE_INSTALL_PREFIX=c:/work/mesa/llvm/bin .
msbuild /p:Configuration=Release INSTALL.vcxproj

set LLVM=c:/work/mesa/llvm/bin
scons build=release platform=windows machine=x86_64 libgl-gdi
```

Результат — `build\windows-x86_64\gallium\targets\libgl-gdi\opengl32.dll`, который Qt предлагает переименовать в `opengl32sw.dll`. Путь в записи PDB согласуется с этой целью сборки. Исторические исходники Mesa описывают статическую линковку LLVM и совпадающий CRT. Цель `libgl-gdi` включает WGL, GDI, Mesa, softpipe, trace и rbug, а также включает llvmpipe при наличии LLVM. Строка `GL_RENDERER` в исходнике llvmpipe выводит только major/minor LLVM. Пара `3.6` в этой строке и `3.6.2` в рецепте согласуется с таким форматированием. [Документация Mesa 11.2.2](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/docs/llvmpipe.html) · [Цель SCons](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/src/gallium/targets/libgl-gdi/SConscript) · [Настройка LLVM в SCons](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/scons/llvm.py) · [Строка рендерера](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/src/gallium/drivers/llvmpipe/lp_screen.c)

Архивы и извлечённые уведомления сохранены локально в `build/endurance/windows-llvmpipe/`. SHA-256 Mesa совпал с опубликованным значением. SHA-256 скачанного LLVM составил `f60dc158bfda6822de167e87275848969f0558b3134892ff54fced87e4667b94`. Извлечённый LLVM `LICENSE.TXT` побайтово совпал с отдельным файлом на сервере релизов. Проверка отделённой подписи LLVM остаётся открытой. Версию LLVM 3.6.2 независимо подтверждает строка версии `llvm-mc` в DLL. [Архив Mesa](https://archive.mesa3d.org/older-versions/11.x/11.2.2/) · [LLVM 3.6.2](https://releases.llvm.org/3.6.2/)

## Источники уведомлений

- **Mesa 11.2.2.** В `docs/license.html` указана MIT для ядра Mesa. Документ направляет к заголовкам исходных файлов для условий отдельных компонентов. Для llvmpipe и WGL такие заголовки содержат, в частности, уведомления VMware. Для атрибуции бинарной поставки учитываются исходники всего связанного пути `libgl-gdi`, включая сгенерированный код и применённые правки. [Лицензия Mesa 11.2.2](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/docs/license.html) · [Заголовок llvmpipe](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/src/gallium/drivers/llvmpipe/lp_screen.c) · [Заголовок WGL](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-11.2.2/src/gallium/state_trackers/wgl/stw_device.c)
- **LLVM 3.6.2.** `LICENSE.TXT` указывает University of Illinois/NCSA Open Source License. Для распространения бинарного файла этот текст требует воспроизвести уведомление об авторских правах, условия и отказ от гарантий в сопроводительных материалах. Файл перечисляет компоненты с дополнительными условиями. Рецепт Mesa связывает `LLVMSupport`, в который в этом релизе входят исходники OpenBSD regex и MD5 Александра Песнякова. Их уведомления входят в базовый комплект для этой конфигурации. Список из `LICENSE.TXT` охватывает весь исходный архив LLVM. Присутствие конкретного объектного файла в DLL требует подтверждения по сборке или по самому бинарному файлу. [LLVM 3.6.2 LICENSE.TXT](https://releases.llvm.org/3.6.2/LICENSE.TXT) · [Цель LLVMSupport](https://github.com/llvm/llvm-project/blob/llvmorg-3.6.2/llvm/lib/Support/CMakeLists.txt) · [COPYRIGHT.regex](https://github.com/llvm/llvm-project/blob/llvmorg-3.6.2/llvm/lib/Support/COPYRIGHT.regex) · [MD5.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-3.6.2/llvm/lib/Support/MD5.cpp)
- **Атрибуция Qt Creator.** Страница Qt Creator подтверждает назначение `opengl32sw.dll` в Windows-поставке и приводит текст MIT для Mesa. Для текста лицензии исторического LLVM и его дополнительных уведомлений первичным источником служит релиз LLVM 3.6.2. [Атрибуция Qt Creator](https://doc.qt.io/qtcreator/qtcreator-binary-attribution-llvmpipe.html) · [LLVM 3.6.2 LICENSE.TXT](https://releases.llvm.org/3.6.2/LICENSE.TXT)

В DLL найдены три строки ошибок, совпадающие с `lib/Support/regerror.c` из LLVM 3.6.2. Это подтверждает присутствие кода OpenBSD regex и применимость `COPYRIGHT.regex`. Извлечённый файл уведомления имеет SHA-256 `0424e57d4303164dc59a8509c20dae0518b853692e5c2b0e98b11816fdbc97c7`. Для MD5 установлена принадлежность исходников к `LLVMSupport`. Присутствие MD5 в DLL требует отдельного подтверждения. [Исходник regerror.c](https://github.com/llvm/llvm-project/blob/llvmorg-3.6.2/llvm/lib/Support/regerror.c) · [COPYRIGHT.regex](https://github.com/llvm/llvm-project/blob/llvmorg-3.6.2/llvm/lib/Support/COPYRIGHT.regex)

## Что ещё нужно подтвердить

Для привязки исходников к конкретной DLL остаются нужны ревизии рабочих деревьев Qt или архивы этих деревьев, сведения о применении упомянутых патчей, параметры исходной компиляции и линковки, а также перечень фактически включённого стороннего кода. Публичный архив Qt и скрипт подготовки Windows подтверждают происхождение бинарника. Историческая инструкция подтверждает версии и описывает сборку из релизных архивов. Повторная сборка по этому рецепту и сопоставление результата с Qt DLL в этой проверке не выполнялись. [Архив Qt](https://download.qt.io/development_releases/prebuilt/llvmpipe/windows/) · [Инструкция Qt 2016 года](https://wiki.qt.io/index.php?title=MesaLlvmpipe&oldid=28759)
