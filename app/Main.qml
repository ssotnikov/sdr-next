import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import SDR.Native 1.0

ApplicationWindow {
    id: window
    visible: true
    width: Math.max(1000, Number(receiver.settings.windowWidth || 1320))
    height: Math.max(650, Number(receiver.settings.windowHeight || 820))
    minimumWidth: 1000
    minimumHeight: 650
    title: Qt.application.displayName + " • 0.1"
    property string language: receiver.settings.language || "ru"
    property int theme: Number(receiver.settings.theme || 0)
    property bool dark: theme === 0 || (theme === 2 && Qt.styleHints.colorScheme === Qt.Dark)
    property bool panelRight: receiver.settings.panelRight || false
    property string accent: receiver.settings.accent || "#72d5d2"
    property string filePath: receiver.settings.file || ""
    property string recordingFolder: receiver.settings.recordingFolder || ""
    property bool sourceRestored: false
    property bool workspaceReady: false
    onPanelRightChanged: { if (workspaceReady) workspace.moveItem(panelRight ? 0 : 1, panelRight ? 1 : 0) }
    property color base: dark ? "#16232f" : "#e7edf0"
    property color surface: dark ? "#1e303e" : "#f7fafb"
    property color ink: dark ? "#e2ebed" : "#182a35"
    property color muted: dark ? "#9baeb8" : "#4a626f"
    property var device: receiver.devices.length > sourceBox.currentIndex && sourceBox.currentIndex >= 0 ? receiver.devices[sourceBox.currentIndex] : ({})
    function tr(ru, en) { return language === "ru" ? ru : en }
    function sourceIndex(saved, devices) {
        for (let i=0; i<devices.length; ++i) if (devices[i].id === saved) return i
        if (!saved.startsWith("soapy:")) return -1
        let match = -1
        for (let i=0; i<devices.length; ++i) {
            const id = String(devices[i].id)
            if (id.startsWith("device:") && id.endsWith(":" + saved.substring(6))) {
                if (match >= 0) return -1
                match = i
            }
        }
        return match
    }
    function restoreSource() {
        if(sourceRestored || receiver.devices.length === 0) return
        const saved = receiver.settings.sourceId || "synthetic"
        sourceBox.currentIndex = sourceIndex(saved, receiver.devices)
        sourceRestored = true
    }
    function parameters() {
        return {centerMHz: Number(frequency.text), rate: Number(rate.editText), offsetKHz: Number(offset.text),
            gain: window.device.gain ? Number(gain.value) : 0, ppm: window.device.ppm ? Number(ppm.text) : 0,
            biasT: (window.device.biasT || false) && bias.checked, directSampling: window.device.directSampling ? direct.currentIndex : 0,
            squelch: squelch.value, mode: mode.currentText, file: filePath}
    }
    function start() { receiver.start(sourceBox.currentIndex, parameters()) }
    function save() {
        receiver.saveSettings({windowWidth: width, windowHeight: height, theme: theme, language: language,
            panelRight: panelRight, panelWidth: side.width, accent: accent, centerMHz: frequency.text,
            rate: rate.editText, offsetKHz: offset.text, mode: mode.currentText, gain: gain.value,
            squelch: squelch.value, file: filePath, recordingFolder: recordingFolder, palette: colors.currentIndex,
            sourceId: window.device.id || receiver.settings.sourceId || "synthetic", ppm: ppm.text, biasT: bias.checked, directSampling: direct.currentIndex})
    }
    color: base
    palette.window: base
    palette.windowText: ink
    palette.base: surface
    palette.text: ink
    palette.button: surface
    palette.buttonText: ink
    palette.highlight: accent
    palette.highlightedText: "#14232d"
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "Sans Serif"
    font.pixelSize: 13
    onClosing: save()
    Component.onCompleted: {
        restoreSource()
        if (panelRight) workspace.moveItem(0, 1)
        workspaceReady = true
    }

    Shortcut { sequence: "Space"; onActivated: receiver.status.running ? receiver.stop() : window.start() }
    Shortcut { sequence: "Ctrl+O"; onActivated: iqFile.open() }
    Shortcut { sequence: "Escape"; onActivated: receiver.stop() }

    header: Rectangle {
        height: 100
        color: surface
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: "SDR"; font.pixelSize: 21; font.weight: Font.DemiBold; color: accent }
                Label { text: "0.1"; color: muted }
                Rectangle { width: 1; height: 22; color: muted; opacity: 0.4; Layout.leftMargin: 8; Layout.rightMargin: 8 }
                Rectangle { width: 7; height: 7; radius: 4; color: receiver.status.running ? accent : muted }
                Label { text: receiver.status.running ? tr("Приём", "Receiving") : receiver.status.state; color: muted }
                Item { Layout.fillWidth: true }
                ComboBox { model: [tr("Тёмная", "Dark"),tr("Светлая", "Light"),tr("Системная", "System")]; currentIndex: window.theme; onActivated: window.theme = currentIndex; Accessible.name: tr("Тема", "Theme") }
                ComboBox { model: ["RU","EN"]; currentIndex: window.language === "ru" ? 0 : 1; onActivated: window.language = currentIndex === 0 ? "ru" : "en"; Accessible.name: "Language" }
                ToolButton { text: panelRight ? "◀" : "▶"; onClicked: panelRight = !panelRight; Accessible.name: tr("Переставить панель", "Move control panel") }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: tr("Частота", "Frequency"); color: muted }
                TextField {
                    id: frequency
                    text: receiver.settings.centerMHz || "100.000000"
                    Layout.preferredWidth: 190
                    font.pixelSize: 23
                    font.family: Qt.platform.os === "windows" ? "Cascadia Mono" : "Monospace"
                    validator: DoubleValidator { bottom: Number(window.device.minHz || 0)/1e6; top: Number(window.device.maxHz || 6000000000)/1e6; locale: "C" }
                    onAccepted: if(receiver.status.running) window.start()
                    Accessible.name: tr("Частота в МГц", "Frequency in MHz")
                }
                Label { text: "MHz"; color: muted }
                Label { text: tr("Смещение", "Offset"); color: muted; Layout.leftMargin: 18 }
                TextField { id: offset; text: receiver.settings.offsetKHz || "0"; Layout.preferredWidth: 90; validator: DoubleValidator { locale: "C" } onAccepted: if(receiver.status.running) window.start(); Accessible.name: tr("Смещение канала в кГц", "Channel offset in kHz") }
                Label { text: "kHz"; color: muted }
                ComboBox { id: mode; model: receiver.demods; Layout.preferredWidth: 170; Component.onCompleted: { let n = find(receiver.settings.mode || "WFM"); if(n >= 0) currentIndex = n } Accessible.name: tr("Демодуляция", "Demodulation") }
                Item { Layout.fillWidth: true }
                Button { text: tr("Применить", "Apply"); enabled: receiver.status.running && !receiver.status.recording; onClicked: window.start() }
                Button { objectName: "rxButton"; text: receiver.status.running ? tr("Остановить", "Stop") : tr("Начать приём", "Start RX"); highlighted: !receiver.status.running; onClicked: receiver.status.running ? receiver.stop() : window.start() }
            }
        }
    }

    SplitView {
        id: workspace
        anchors.fill: parent
        orientation: Qt.Horizontal
        handle: Rectangle { implicitWidth: 5; color: SplitHandle.hovered ? accent : base }
        Rectangle {
            id: side
            objectName: "controlPanel"
            color: surface
            SplitView.preferredWidth: Number(receiver.settings.panelWidth || 270)
            SplitView.minimumWidth: 255
            SplitView.maximumWidth: 430
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                TabBar {
                    id: tabs
                    Layout.fillWidth: true
                    TabButton { text: tr("Приём", "RX") }
                    TabButton { text: tr("Аудио", "Audio") }
                    TabButton { text: tr("Файлы", "Files") }
                }
                StackLayout {
                    currentIndex: tabs.currentIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ScrollView {
                        clip: true
                        ColumnLayout {
                            width: side.width - 26
                            spacing: 10
                            Label { text: tr("Источник IQ", "IQ source"); font.weight: Font.DemiBold }
                            ComboBox { id: sourceBox; objectName: "sourceSelector"; Layout.fillWidth: true; model: receiver.devices; textRole: "label"; displayText: currentIndex < 0 ? tr("Сохранённый источник недоступен", "Saved source is unavailable") : currentText; Accessible.name: tr("Источник IQ", "IQ source") }
                            Button { text: tr("Обновить устройства", "Refresh devices"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: receiver.scan() }
                            Label { text: tr("Скорость IQ, отсчётов/с", "IQ sample rate, samples/s"); color: muted }
                            ComboBox {
                                id: rate
                                objectName: "rateSelector"
                                Layout.fillWidth: true
                                editable: true
                                model: window.device.rates && window.device.rates.length ? window.device.rates.map(String) : ["2400000","10000000","20000000","192000"]
                                Component.onCompleted: editText = String(receiver.settings.rate || "2400000")
                                Accessible.name: tr("Скорость IQ", "IQ sample rate")
                            }
                            Label { text: tr("Усиление", "Gain") + "   " + gain.value.toFixed(0) + " dB"; color: muted }
                            Slider { id: gain; Layout.fillWidth: true; from: Number(window.device.minGain || 0); to: window.device.gain ? Number(window.device.maxGain) : 76; value: receiver.settings.gain === undefined ? 20 : Number(receiver.settings.gain); enabled: window.device.gain || false; Accessible.name: tr("Усиление", "Gain") }
                            Label { visible: receiver.status.running && (window.device.gain || false); text: tr("Применено: ", "Applied: ") + Number(receiver.status.gain || 0).toFixed(1) + " dB"; color: muted }
                            Label { text: tr("Частотная коррекция, ppm", "Frequency correction, ppm"); color: muted }
                            TextField { id: ppm; objectName: "ppmSetting"; text: receiver.settings.ppm || "0"; enabled: window.device.ppm || false; Layout.fillWidth: true; validator: DoubleValidator { locale: "C" } }
                            CheckBox { id: bias; objectName: "biasSetting"; text: "Bias-T"; checked: receiver.settings.biasT || false; enabled: window.device.biasT || false }
                            Label { text: "Direct sampling"; enabled: window.device.directSampling || false; color: muted }
                            ComboBox { id: direct; objectName: "directSetting"; model: [tr("Выключено", "Off"),"I","Q"]; currentIndex: Number(receiver.settings.directSampling || 0); enabled: window.device.directSampling || false; Layout.fillWidth: true }
                            Rectangle { height: 1; color: muted; opacity: 0.25; Layout.fillWidth: true; Layout.topMargin: 10 }
                            Label { text: tr("Расширения", "Extensions"); font.weight: Font.DemiBold }
                            Label { text: tr("Включайте только проверенные модули. Они работают внутри приложения.", "Enable trusted modules only. They run inside the application."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                            Button { text: tr("Добавить manifest…", "Add manifest…"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: pluginFile.open() }
                            Label { text: receiver.status.plugins ? receiver.status.plugins.join("\n") : ""; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: accent }
                            Button { text: tr("Отключить расширения", "Disable extensions"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: receiver.unloadPlugins() }
                        }
                    }
                    ColumnLayout {
                        spacing: 12
                        Label { text: tr("Аудиовыход", "Audio output"); font.weight: Font.DemiBold }
                        ComboBox { id: audioOutput; model: receiver.audioOutputs; Layout.fillWidth: true; onActivated: receiver.setAudio(currentIndex, volume.value); Accessible.name: tr("Аудиоустройство", "Audio device") }
                        Label { text: tr("Громкость", "Volume"); color: muted }
                        Slider { id: volume; from: 0; to: 1; value: 0.5; Layout.fillWidth: true; onMoved: receiver.setAudio(audioOutput.currentIndex,value); Accessible.name: tr("Громкость", "Volume") }
                        Label { text: tr("Порог шумоподавителя", "Squelch threshold") + "  " + squelch.value.toFixed(0) + " dBFS"; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                        Slider { id: squelch; from: -120; to: 0; value: receiver.settings.squelch === undefined ? -100 : Number(receiver.settings.squelch); Layout.fillWidth: true; Accessible.name: "Squelch" }
                        Label { text: tr("Изменение режима, полосы источника и порога применяется кнопкой «Применить».", "Use Apply to change the mode, source rate and threshold."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                        Label { text: tr("Палитра waterfall", "Waterfall palette"); font.weight: Font.DemiBold; Layout.topMargin: 14 }
                        ComboBox { id: colors; model: [tr("Океан", "Ocean"),tr("Янтарь", "Amber")]; currentIndex: Number(receiver.settings.palette || 0); Layout.fillWidth: true }
                        Label { text: tr("Акцент", "Accent"); color: muted }
                        RowLayout {
                            Repeater { model: ["#72d5d2","#e8b979","#a7c5f2"]
                                delegate: Button { required property string modelData; text: "●"; palette.buttonText: modelData; onClicked: window.accent = modelData }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                    ColumnLayout {
                        spacing: 12
                        Label { text: tr("Воспроизведение IQ", "IQ playback"); font.weight: Font.DemiBold }
                        Label { text: "CF32 little-endian"; color: muted }
                        Button { text: tr("Открыть файл IQ…", "Open IQ file…"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: iqFile.open() }
                        Label { text: window.filePath || tr("Файл не выбран", "No file selected"); wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; color: muted }
                        Label { text: tr("Без metadata задайте частоту и скорость IQ перед запуском.", "Without metadata, set the frequency and sample rate before starting."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                        Rectangle { height: 1; color: muted; opacity: 0.25; Layout.fillWidth: true; Layout.topMargin: 10 }
                        Label { text: tr("Запись", "Recording"); font.weight: Font.DemiBold }
                        CheckBox { id: wav; text: "WAV / 48 kHz / mono"; checked: true; enabled: !receiver.status.recording }
                        CheckBox { id: raw; text: "Raw IQ / CF32 + JSON"; checked: true; enabled: !receiver.status.recording }
                        Button { text: tr("Выбрать папку…", "Choose folder…"); Layout.fillWidth: true; enabled: !receiver.status.recording; onClicked: folder.open() }
                        Label { text: window.recordingFolder || tr("Выберите папку записи", "Choose a recording folder"); Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: muted }
                        Button { text: receiver.status.recording ? tr("Завершить запись", "Stop recording") : tr("Начать запись", "Start recording"); highlighted: receiver.status.recording; enabled: receiver.status.running && (receiver.status.recording || (window.recordingFolder.length > 0 && (wav.checked || raw.checked))); Layout.fillWidth: true; onClicked: receiver.status.recording ? receiver.stopRecording() : receiver.record(window.recordingFolder, wav.checked, raw.checked) }
                        Button { text: tr("Экспорт диагностики…", "Export diagnostics…"); Layout.fillWidth: true; onClicked: diagnosticsFile.open() }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
        Rectangle {
            objectName: "plotPanel"
            SplitView.fillWidth: true
            color: "#101c2c"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 7
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: tr("Спектр и waterfall", "Spectrum and waterfall"); color: "#d9e8ee"; font.pixelSize: 16 }
                    Item { Layout.fillWidth: true }
                    Label { text: Number(receiver.status.powerDb || -120).toFixed(1) + " dBFS"; color: "#e8b979"; font.family: "Monospace" }
                    ToolButton { text: "1:1"; palette.buttonText: "#d9e8ee"; onClicked: { spectrum.span = 1; spectrum.position = 0.5 } }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    SpectrumView { id: spectrum; objectName: "spectrum"; anchors.fill: parent; colorMap: colors.currentIndex }
                    Repeater { model: 5
                        delegate: Rectangle { required property int index; x: parent.width*(index+1)/6; width: 1; height: parent.height; color: "#e0eef5"; opacity: 0.055 }
                    }
                    Rectangle { x: parent.width/2; width: 1; height: parent.height; color: window.accent; opacity: 0.3 }
                    Rectangle { y: parent.height*0.36; height: 1; width: parent.width; color: "#526776" }
                    Label { text: "−120 dBFS"; y: parent.height*0.36-20; color: "#91a6b9"; font.pixelSize: 11 }
                    Label { text: "0 dBFS"; color: "#91a6b9"; font.pixelSize: 11 }
                    Label { visible: !receiver.status.running && Number(receiver.status.samples || 0) === 0; anchors.centerIn: parent; text: tr("Выберите источник и начните приём", "Select a source and start receiving"); color: "#9aafbd"; font.pixelSize: 18 }
                    MouseArea {
                        anchors.fill: parent
                        property real lastX: 0
                        property bool dragged: false
                        onPressed: function(mouse) { lastX = mouse.x; dragged = false }
                        onPositionChanged: function(mouse) { if(pressed) { let dx = mouse.x-lastX; if(Math.abs(dx)>1) dragged = true; spectrum.position -= dx/width*spectrum.span; lastX = mouse.x } }
                        onReleased: function(mouse) { if(!dragged) offset.text = (((mouse.x/width-0.5)*spectrum.span+spectrum.position-0.5)*Number(receiver.status.rate)/1000).toFixed(2) }
                        onWheel: function(wheel) { spectrum.span *= wheel.angleDelta.y>0 ? 0.8 : 1.25 }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Repeater { model: 5
                        delegate: Label {
                            required property int index
                            text: ((Number(receiver.status.centerHz)+(spectrum.position-spectrum.span/2+spectrum.span*index/4-0.5)*Number(receiver.status.rate))/1e6).toFixed(4)
                            color: "#a5bbc8"
                            font.family: "Monospace"
                            horizontalAlignment: index === 0 ? Text.AlignLeft : index === 4 ? Text.AlignRight : Text.AlignHCenter
                            Layout.fillWidth: true
                        }
                    }
                }
                Label { text: tr("Колесо — масштаб · перетаскивание — обзор · щелчок — смещение канала · MHz", "Wheel: zoom · drag: pan · click: channel offset · MHz"); color: "#829ba9"; font.pixelSize: 11 }
            }
        }
    }
    footer: ColumnLayout {
        spacing: 0
        Rectangle {
            visible: String(receiver.status.error || "").length > 0
            Layout.fillWidth: true
            implicitHeight: errorLabel.implicitHeight + 18
            color: dark ? "#49322b" : "#f5dcd1"
            Label { id: errorLabel; anchors.fill: parent; anchors.margins: 9; text: receiver.status.error || ""; wrapMode: Text.WordWrap; color: dark ? "#ffd7bf" : "#6c3022" }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 32
            color: surface
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Label { text: (Number(receiver.status.rate || 0)/1e6).toFixed(3) + " MSPS"; color: muted }
                Label { text: tr("IQ разрывы: ", "IQ gaps: ") + Number(receiver.status.gaps || 0); color: muted; Layout.leftMargin: 14 }
                Label { text: tr("Очередь: ", "Queue: ") + Number(receiver.status.queueDepth || 0) + "/8"; color: muted; Layout.leftMargin: 14 }
                Label { text: "Audio underrun: " + Number(receiver.status.audioUnderruns || 0); color: muted; Layout.leftMargin: 14 }
                Label { text: Number(receiver.status.processingMs || 0).toFixed(2) + " ms/block"; color: muted; Layout.leftMargin: 14 }
                Item { Layout.fillWidth: true }
                Label { text: receiver.status.recording ? "● " + tr("Запись", "Recording") : tr("Локально · без телеметрии", "Local · no telemetry"); color: receiver.status.recording ? "#e8b979" : muted }
            }
        }
    }
    FileDialog { id: iqFile; title: tr("Открыть CF32 little-endian IQ", "Open CF32 little-endian IQ"); nameFilters: ["Raw IQ (*.cf32 *.iq *.raw)", "All files (*)"]; onAccepted: { window.filePath = selectedFile.toString(); sourceBox.currentIndex = 1; tabs.currentIndex = 2 } }
    FileDialog { id: pluginFile; title: tr("Добавить проверенное расширение", "Add a trusted extension"); nameFilters: ["Plugin manifest (*.json)"]; onAccepted: receiver.loadPlugin(selectedFile.toString()) }
    FileDialog { id: diagnosticsFile; fileMode: FileDialog.SaveFile; defaultSuffix: "jsonl"; title: tr("Экспорт локальной диагностики", "Export local diagnostics"); onAccepted: receiver.exportDiagnostics(selectedFile.toString()) }
    FolderDialog { id: folder; title: tr("Папка для записей", "Recording folder"); onAccepted: window.recordingFolder = selectedFolder.toString() }
    Connections { target: receiver; function onSpectrumReady(frame) { spectrum.setFrame(frame) } function onDevicesChanged() { window.restoreSource() } }
}
