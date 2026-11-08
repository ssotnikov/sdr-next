import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import SDR.Native 1.0

ApplicationWindow {
    id: window
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint
    width: Math.max(1000, Number(receiver.settings.windowWidth || 1320))
    height: Math.max(650, Number(receiver.settings.windowHeight || 820))
    minimumWidth: 1000
    minimumHeight: 650
    title: Qt.application.displayName + " • 0.1"
    property bool enduranceActive: Qt.application.arguments.indexOf("--endurance-dir") >= 0
    property string language: receiver.settings.language || "ru"
    property int theme: Number(receiver.settings.theme || 0)
    property bool dark: theme === 0 || (theme === 2 && Qt.styleHints.colorScheme === Qt.Dark)
    property bool panelRight: receiver.settings.panelRight || false
    property string accent: receiver.settings.accent || "#ffb224"
    property string filePath: receiver.settings.file || ""
    property string recordingFolder: receiver.settings.recordingFolder || ""
    property bool sourceRestored: false
    property bool workspaceReady: false
    onPanelRightChanged: { if (workspaceReady) workspace.moveItem(panelRight ? 0 : 1, panelRight ? 1 : 0) }
    BrandTheme { id: brand; objectName: "brandTheme"; dark: window.dark }
    property color base: brand.colors.bg0
    property color surface: brand.colors.bg1
    property color ink: brand.colors.ink
    property color muted: brand.colors.inkMuted
    readonly property color accentInk: accent.toLowerCase() === "#ffb224" ? brand.colors.dialInk : (dark ? accent : brand.colors.trace)
    TextMetrics { id: frequencyMetrics; font.family: brand.monoMedium; font.pixelSize: brand.text["freq-xl"].size; text: "6000.000000" }

    component BrandButton: Button {
        id: control
        implicitHeight: brand.controlHeight
        font.family: brand.sansMedium
        palette.buttonText: highlighted ? brand.colors.onDial : window.ink
        opacity: enabled ? 1 : brand.tokens.opacity.disabled
        background: Rectangle {
            radius: brand.radius["radius-sm"]
            color: control.highlighted ? (control.down ? Qt.darker(window.accent, 1.12) : control.hovered ? brand.colors.dialHover : window.accent)
                : (control.down ? brand.colors.bg0 : control.hovered ? brand.colors.bg3 : brand.colors.bg2)
            border.color: control.highlighted ? window.accent : brand.colors.lineControl
            Rectangle {
                anchors.fill: parent; anchors.margins: -brand.tokens.focusRing.gap - brand.tokens.focusRing.width
                visible: control.visualFocus; color: "transparent"; radius: parent.radius + 2
                border.width: brand.tokens.focusRing.width; border.color: brand.colors.focus
            }
        }
    }
    component BrandField: TextField {
        id: control
        font.family: brand.mono
        implicitHeight: brand.controlHeight
        leftPadding: brand.spacing["space-2"]; rightPadding: brand.spacing["space-2"]
        opacity: enabled ? 1 : brand.tokens.opacity.disabled
        background: Rectangle {
            radius: brand.radius["radius-sm"]; color: brand.colors.bg2
            border.color: control.hovered ? brand.colors.inkMuted : brand.colors.lineControl
            Rectangle {
                anchors.fill: parent; anchors.margins: -brand.tokens.focusRing.gap - brand.tokens.focusRing.width
                visible: control.activeFocus; color: "transparent"; radius: parent.radius + 2
                border.width: brand.tokens.focusRing.width; border.color: brand.colors.focus
            }
        }
    }
    component BrandCombo: ComboBox {
        id: control
        implicitWidth: Math.max(140, implicitContentWidth + leftPadding + rightPadding)
        implicitHeight: brand.controlHeight
        opacity: enabled ? 1 : brand.tokens.opacity.disabled
        background: Rectangle {
            radius: brand.radius["radius-sm"]
            color: control.down ? brand.colors.bg0 : control.hovered ? brand.colors.bg3 : brand.colors.bg2
            border.color: brand.colors.lineControl
            Rectangle {
                anchors.fill: parent; anchors.margins: -brand.tokens.focusRing.gap - brand.tokens.focusRing.width
                visible: control.visualFocus; color: "transparent"; radius: parent.radius + 2
                border.width: brand.tokens.focusRing.width; border.color: brand.colors.focus
            }
        }
    }
    component BrandCheck: CheckBox {
        implicitHeight: brand.controlHeight
        opacity: enabled ? 1 : brand.tokens.opacity.disabled
    }
    component BrandSlider: Slider {
        implicitHeight: brand.controlHeight
        opacity: enabled ? 1 : brand.tokens.opacity.disabled
    }
    component BrandTab: TabButton {
        id: control
        implicitHeight: brand.controlHeight
        font.family: brand.sansMedium
        background: Rectangle {
            color: control.checked || control.hovered ? brand.colors.bg3 : brand.colors.bg2
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; visible: control.checked; color: window.accent }
            border.width: control.visualFocus ? brand.tokens.focusRing.width : 0
            border.color: brand.colors.focus
        }
    }
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
    palette.base: brand.colors.bg2
    palette.alternateBase: brand.colors.bg3
    palette.mid: brand.colors.lineControl
    palette.dark: brand.colors.lineControl
    palette.light: brand.colors.bg3
    palette.placeholderText: muted
    palette.accent: accent
    palette.brightText: brand.colors.danger
    palette.text: ink
    palette.button: surface
    palette.buttonText: ink
    palette.highlight: accent
    palette.highlightedText: brand.colors.onDial
    font.family: brand.sans
    font.pixelSize: brand.text.body.size
    onClosing: save()
    Component.onCompleted: {
        restoreSource()
        if (panelRight) workspace.moveItem(0, 1)
        workspaceReady = true
    }

    Shortcut { objectName: "rxSpaceShortcut"; sequence: "Space"; autoRepeat: false; enabled: !window.enduranceActive && !mainMenu.visible && !aboutDialog.visible; onActivated: receiver.status.running ? receiver.stop() : window.start() }
    Shortcut { sequence: "Ctrl+O"; enabled: !window.enduranceActive && !mainMenu.visible && !aboutDialog.visible; onActivated: iqFile.open() }
    Shortcut { objectName: "rxEscapeShortcut"; sequence: "Escape"; autoRepeat: false; enabled: !window.enduranceActive && !mainMenu.visible && !aboutDialog.visible; onActivated: receiver.stop() }

    function openMainMenu() { if (!window.enduranceActive) mainMenu.open() }
    Shortcut { objectName: "mainMenuShortcut"; sequences: ["F10", "Alt+M"]; enabled: !window.enduranceActive && !aboutDialog.visible; onActivated: mainMenu.visible ? mainMenu.close() : window.openMainMenu() }
    Action {
        id: movePanelAction; objectName: "movePanelAction"
        text: window.panelRight ? tr("Панель слева", "Panel on the left") : tr("Панель справа", "Panel on the right")
        enabled: !window.enduranceActive
        onTriggered: window.panelRight = !window.panelRight
    }
    component AppMenu: Menu {
        implicitWidth: 248
        popupType: Popup.Item
        font.family: brand.sans
        font.pixelSize: brand.text.body.size
        background: Rectangle {
            color: brand.colors.bg1; radius: brand.radius["radius-md"]
            border.width: 1; border.color: brand.colors.lineControl
        }
    }
    component AppMenuItem: MenuItem {
        id: control
        implicitHeight: brand.controlHeight
        font.family: brand.sans
        palette.text: highlighted ? brand.colors.onDial : window.ink
        palette.windowText: highlighted ? brand.colors.onDial : window.ink
        opacity: enabled ? 1 : brand.tokens.opacity.disabled
        background: Rectangle {
            radius: brand.radius["radius-sm"]
            color: control.highlighted ? window.accent : "transparent"
        }
    }
    AppMenu {
        id: mainMenu; objectName: "mainMenu"
        parent: windowHeader; x: 8; y: titleBar.height
        AppMenu {
            title: tr("Файл", "File")
            AppMenuItem { text: tr("Открыть IQ…", "Open IQ…"); enabled: !window.enduranceActive && !receiver.status.running; onTriggered: iqFile.open() }
            AppMenuItem { text: tr("Добавить manifest…", "Add manifest…"); enabled: !window.enduranceActive && !receiver.status.running; onTriggered: pluginFile.open() }
            AppMenuItem { text: tr("Папка записи…", "Recording folder…"); enabled: !window.enduranceActive && !receiver.status.recording; onTriggered: folder.open() }
            MenuSeparator {}
            AppMenuItem { text: tr("Экспорт диагностики…", "Export diagnostics…"); onTriggered: diagnosticsFile.open() }
            MenuSeparator {}
            AppMenuItem { text: tr("Выход", "Exit"); enabled: !window.enduranceActive; onTriggered: window.close() }
        }
        AppMenu {
            title: tr("Приём", "Receiver")
            AppMenuItem { objectName: "menuRx"; text: receiver.status.running ? tr("Остановить", "Stop RX") : tr("Начать приём", "Start RX"); enabled: !window.enduranceActive; onTriggered: receiver.status.running ? receiver.stop() : window.start() }
            AppMenuItem { text: tr("Обновить устройства", "Refresh devices"); enabled: !window.enduranceActive && !receiver.status.running; onTriggered: receiver.scan() }
        }
        AppMenu {
            title: tr("Вид", "View")
            AppMenuItem { action: movePanelAction }
            AppMenu {
                title: tr("Тема", "Theme")
                AppMenuItem { text: tr("Тёмная", "Dark"); checkable: true; checked: window.theme === 0; onTriggered: window.theme = 0 }
                AppMenuItem { text: tr("Светлая", "Light"); checkable: true; checked: window.theme === 1; onTriggered: window.theme = 1 }
                AppMenuItem { text: tr("Системная", "System"); checkable: true; checked: window.theme === 2; onTriggered: window.theme = 2 }
            }
            AppMenu {
                title: tr("Язык", "Language")
                AppMenuItem { text: "Русский"; checkable: true; checked: window.language === "ru"; onTriggered: window.language = "ru" }
                AppMenuItem { text: "English"; checkable: true; checked: window.language === "en"; onTriggered: window.language = "en" }
            }
        }
        AppMenu {
            title: tr("Справка", "Help")
            AppMenuItem { text: tr("О SDR Next…", "About SDR Next…"); onTriggered: aboutDialog.open() }
        }
    }
    Dialog {
        id: aboutDialog; objectName: "aboutDialog"
        implicitWidth: 360
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: tr("О SDR Next", "About SDR Next")
        modal: true; standardButtons: Dialog.Ok
        contentItem: Label { text: "SDR Next " + Qt.application.version + "\n" + tr("Мультиплатформенный SDR-приёмник", "Multiplatform SDR Receiver"); color: window.ink; wrapMode: Text.WordWrap }
    }
    WindowResizeHandles {
        objectName: "windowResizeHandles"
        hostWindow: window
        parent: window.header ? window.header.parent : null
        anchors.fill: parent
    }

    header: Column {
        id: windowHeader
        width: window.width
        WindowTitleBar {
            id: titleBar
            objectName: "windowTitleBar"
            width: parent.width
            hostWindow: window; theme: brand
            russian: window.language === "ru"
            menuEnabled: !window.enduranceActive
            menuOpen: mainMenu.visible
            testCaption: window.enduranceActive ? window.title : ""
            onMenuRequested: window.openMainMenu()
        }
        Rectangle {
        width: parent.width
        enabled: !window.enduranceActive
        height: 112
        color: surface
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: brand.spacing["space-3"]
            spacing: brand.spacing["space-2"]
            RowLayout {
                Layout.fillWidth: true
                Rectangle { width: 7; height: 7; radius: 4; color: receiver.status.running ? brand.colors.ok : muted }
                Label { text: receiver.status.running ? tr("Приём", "Receiving") : receiver.status.state; color: muted }
                Item { Layout.fillWidth: true }
                BrandCombo { model: [tr("Тёмная", "Dark"),tr("Светлая", "Light"),tr("Системная", "System")]; objectName: "themeSelector"; currentIndex: window.theme; onActivated: window.theme = currentIndex; Accessible.name: tr("Тема", "Theme") }
                BrandCombo { objectName: "languageSelector"; implicitWidth: 72; model: ["RU","EN"]; currentIndex: window.language === "ru" ? 0 : 1; onActivated: window.language = currentIndex === 0 ? "ru" : "en"; Accessible.name: "Language" }
                BrandButton { text: panelRight ? "◀" : "▶"; onClicked: movePanelAction.trigger(); Accessible.name: tr("Переставить панель", "Move control panel") }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: tr("Частота", "Frequency"); color: muted }
                BrandField {
                    id: frequency
                    objectName: "frequencyField"
                    text: receiver.settings.centerMHz || "100.000000"
                    Layout.preferredWidth: Math.ceil(frequencyMetrics.advanceWidth) + 24
                    implicitHeight: brand.text["freq-xl"].lineHeight + 8
                    font.pixelSize: brand.text["freq-xl"].size
                    font.family: brand.monoMedium
                    validator: DoubleValidator { bottom: Number(window.device.minHz || 0)/1e6; top: Number(window.device.maxHz || 6000000000)/1e6; locale: "C" }
                    onAccepted: if(receiver.status.running) window.start()
                    Accessible.name: tr("Частота в МГц", "Frequency in MHz")
                }
                Label { text: "MHz"; color: muted }
                Label { text: tr("Смещение", "Offset"); color: muted; Layout.leftMargin: brand.spacing["space-2"] }
                BrandField { id: offset; text: receiver.settings.offsetKHz || "0"; Layout.preferredWidth: 90; validator: DoubleValidator { locale: "C" } onAccepted: if(receiver.status.running) window.start(); Accessible.name: tr("Смещение канала в кГц", "Channel offset in kHz") }
                Label { text: "kHz"; color: muted }
                BrandCombo { id: mode; model: receiver.demods; Layout.preferredWidth: 130; Component.onCompleted: { let n = find(receiver.settings.mode || "WFM"); if(n >= 0) currentIndex = n } Accessible.name: tr("Демодуляция", "Demodulation") }
                Item { Layout.fillWidth: true }
                BrandButton { text: tr("Применить", "Apply"); enabled: receiver.status.running && !receiver.status.recording; onClicked: window.start() }
                BrandButton { objectName: "rxButton"; text: receiver.status.running ? tr("Остановить", "Stop") : tr("Начать приём", "Start RX"); highlighted: !receiver.status.running; onClicked: receiver.status.running ? receiver.stop() : window.start() }
            }
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
            enabled: !window.enduranceActive
            color: surface
            SplitView.preferredWidth: Number(receiver.settings.panelWidth || brand.sizes["sidebar-w"])
            SplitView.minimumWidth: 255
            SplitView.maximumWidth: 430
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: brand.spacing["space-3"]
                spacing: brand.spacing["space-3"]
                TabBar {
                    id: tabs
                    Layout.fillWidth: true
                    BrandTab { text: tr("Приём", "RX") }
                    BrandTab { text: tr("Аудио", "Audio") }
                    BrandTab { text: tr("Файлы", "Files") }
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
                            Label { text: tr("Источник IQ", "IQ source"); font.family: brand.sansSemibold }
                            BrandCombo { id: sourceBox; objectName: "sourceSelector"; Layout.fillWidth: true; model: receiver.devices; textRole: "label"; displayText: currentIndex < 0 ? tr("Сохранённый источник недоступен", "Saved source is unavailable") : currentText; Accessible.name: tr("Источник IQ", "IQ source") }
                            BrandButton { text: tr("Обновить устройства", "Refresh devices"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: receiver.scan() }
                            Label { text: tr("Скорость IQ, отсчётов/с", "IQ sample rate, samples/s"); color: muted }
                            BrandCombo {
                                id: rate
                                objectName: "rateSelector"
                                font.family: brand.mono
                                Layout.fillWidth: true
                                editable: true
                                model: window.device.rates && window.device.rates.length ? window.device.rates.map(String) : ["2400000","10000000","20000000","192000"]
                                Component.onCompleted: editText = String(receiver.settings.rate || "2400000")
                                Accessible.name: tr("Скорость IQ", "IQ sample rate")
                            }
                            Label { text: tr("Усиление", "Gain") + "   " + gain.value.toFixed(0) + " dB"; color: muted }
                            BrandSlider { id: gain; Layout.fillWidth: true; from: Number(window.device.minGain || 0); to: window.device.gain ? Number(window.device.maxGain) : 76; value: receiver.settings.gain === undefined ? 20 : Number(receiver.settings.gain); enabled: window.device.gain || false; Accessible.name: tr("Усиление", "Gain") }
                            Label { visible: receiver.status.running && (window.device.gain || false); text: tr("Применено: ", "Applied: ") + Number(receiver.status.gain || 0).toFixed(1) + " dB"; color: muted }
                            Label { text: tr("Частотная коррекция, ppm", "Frequency correction, ppm"); color: muted }
                            BrandField { id: ppm; objectName: "ppmSetting"; text: receiver.settings.ppm || "0"; enabled: window.device.ppm || false; Layout.fillWidth: true; validator: DoubleValidator { locale: "C" } }
                            BrandCheck { id: bias; objectName: "biasSetting"; text: "Bias-T"; checked: receiver.settings.biasT || false; enabled: window.device.biasT || false }
                            Label { text: "Direct sampling"; enabled: window.device.directSampling || false; color: muted }
                            BrandCombo { id: direct; objectName: "directSetting"; model: [tr("Выключено", "Off"),"I","Q"]; currentIndex: Number(receiver.settings.directSampling || 0); enabled: window.device.directSampling || false; Layout.fillWidth: true }
                            Rectangle { height: 1; color: brand.colors.line; Layout.fillWidth: true; Layout.topMargin: 10 }
                            Label { text: tr("Расширения", "Extensions"); font.family: brand.sansSemibold }
                            Label { text: tr("Включайте только проверенные модули. Они работают внутри приложения.", "Enable trusted modules only. They run inside the application."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                            BrandButton { text: tr("Добавить manifest…", "Add manifest…"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: pluginFile.open() }
                            Label { text: receiver.status.plugins ? receiver.status.plugins.join("\n") : ""; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: accentInk }
                            BrandButton { text: tr("Отключить расширения", "Disable extensions"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: receiver.unloadPlugins() }
                        }
                    }
                    ColumnLayout {
                        spacing: brand.spacing["space-3"]
                        Label { text: tr("Аудиовыход", "Audio output"); font.family: brand.sansSemibold }
                        BrandCombo { id: audioOutput; model: receiver.audioOutputs; Layout.fillWidth: true; onActivated: receiver.setAudio(currentIndex, volume.value); Accessible.name: tr("Аудиоустройство", "Audio device") }
                        Label { text: tr("Громкость", "Volume"); color: muted }
                        BrandSlider { id: volume; from: 0; to: 1; value: 0.5; Layout.fillWidth: true; onMoved: receiver.setAudio(audioOutput.currentIndex,value); Accessible.name: tr("Громкость", "Volume") }
                        Label { text: tr("Порог шумоподавителя", "Squelch threshold") + "  " + squelch.value.toFixed(0) + " dBFS"; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                        BrandSlider { id: squelch; from: -120; to: 0; value: receiver.settings.squelch === undefined ? -100 : Number(receiver.settings.squelch); Layout.fillWidth: true; Accessible.name: "Squelch" }
                        Label { text: tr("Изменение режима, полосы источника и порога применяется кнопкой «Применить».", "Use Apply to change the mode, source rate and threshold."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                        Label { text: tr("Палитра waterfall", "Waterfall palette"); font.family: brand.sansSemibold; Layout.topMargin: 14 }
                        BrandCombo { id: colors; objectName: "waterfallSelector"; model: [tr("Океан", "Ocean"),tr("Янтарь", "Amber"),"Next"]; currentIndex: receiver.settings.palette === undefined ? 2 : Number(receiver.settings.palette); Layout.fillWidth: true }
                        Label { text: tr("Акцент", "Accent"); color: muted }
                        RowLayout {
                            Repeater { model: ["#ffb224","#72d5d2","#e8b979","#a7c5f2"]
                                delegate: BrandButton { required property string modelData; text: "●"; palette.buttonText: modelData; onClicked: window.accent = modelData }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                    ColumnLayout {
                        spacing: brand.spacing["space-3"]
                        Label { text: tr("Воспроизведение IQ", "IQ playback"); font.family: brand.sansSemibold }
                        Label { text: "CF32 little-endian"; color: muted }
                        BrandButton { text: tr("Открыть файл IQ…", "Open IQ file…"); enabled: !receiver.status.running; Layout.fillWidth: true; onClicked: iqFile.open() }
                        Label { text: window.filePath || tr("Файл не выбран", "No file selected"); wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; color: muted }
                        Label { text: tr("Без metadata задайте частоту и скорость IQ перед запуском.", "Without metadata, set the frequency and sample rate before starting."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: muted }
                        Rectangle { height: 1; color: brand.colors.line; Layout.fillWidth: true; Layout.topMargin: 10 }
                        Label { text: tr("Запись", "Recording"); font.family: brand.sansSemibold }
                        BrandCheck { id: wav; text: "WAV / 48 kHz / mono"; checked: true; enabled: !receiver.status.recording }
                        BrandCheck { id: raw; text: "Raw IQ / CF32 + JSON"; checked: true; enabled: !receiver.status.recording }
                        BrandButton { text: tr("Выбрать папку…", "Choose folder…"); Layout.fillWidth: true; enabled: !receiver.status.recording; onClicked: folder.open() }
                        Label { text: window.recordingFolder || tr("Выберите папку записи", "Choose a recording folder"); Layout.fillWidth: true; wrapMode: Text.WrapAnywhere; color: muted }
                        BrandButton { text: receiver.status.recording ? tr("Завершить запись", "Stop recording") : tr("Начать запись", "Start recording"); highlighted: receiver.status.recording; enabled: receiver.status.running && (receiver.status.recording || (window.recordingFolder.length > 0 && (wav.checked || raw.checked))); Layout.fillWidth: true; onClicked: receiver.status.recording ? receiver.stopRecording() : receiver.record(window.recordingFolder, wav.checked, raw.checked) }
                        BrandButton { text: tr("Экспорт диагностики…", "Export diagnostics…"); Layout.fillWidth: true; onClicked: diagnosticsFile.open() }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
        Rectangle {
            objectName: "plotPanel"
            SplitView.fillWidth: true
            color: brand.colors.bg0
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: brand.spacing["space-4"]
                spacing: brand.spacing["space-2"]
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: tr("Спектр и waterfall", "Spectrum and waterfall"); color: ink; font.pixelSize: brand.text.heading.size; font.family: brand.sansSemibold }
                    Item { Layout.fillWidth: true }
                    Label { text: Number(receiver.status.powerDb || -120).toFixed(1) + " dBFS"; color: accentInk; font.family: brand.mono; font.pixelSize: brand.text.readout.size }
                    BrandButton { text: "1:1"; palette.buttonText: ink; onClicked: { spectrum.span = 1; spectrum.position = 0.5 } }
                }
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    SpectrumView { id: spectrum; objectName: "spectrum"; anchors.fill: parent; colorMap: colors.currentIndex; traceColor: brand.colors.trace; waterfallStops: brand.tokens.waterfall.stops }
                    Repeater { model: 5
                        delegate: Rectangle { required property int index; x: parent.width*(index+1)/6; width: 1; height: parent.height * 0.36; color: brand.colors.line }
                    }
                    Rectangle { x: parent.width/2; width: 1; height: parent.height; color: window.accent; opacity: 0.3 }
                    Rectangle { y: parent.height*0.36; height: 1; width: parent.width; color: brand.colors.line }
                    Label { text: "−120 dBFS"; y: parent.height*0.36-20; color: muted; font.pixelSize: brand.text.readout.size; font.family: brand.mono }
                    Label { text: "0 dBFS"; color: muted; font.pixelSize: brand.text.readout.size; font.family: brand.mono }
                    Label { visible: !receiver.status.running && Number(receiver.status.samples || 0) === 0; objectName: "emptySpectrumHint"; anchors.centerIn: parent; anchors.verticalCenterOffset: -parent.height * 0.32; text: tr("Выберите источник и начните приём", "Select a source and start receiving"); color: muted; font.pixelSize: brand.text.title.size }
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
                            color: muted
                            font.family: brand.mono; font.pixelSize: brand.text.readout.size
                            horizontalAlignment: index === 0 ? Text.AlignLeft : index === 4 ? Text.AlignRight : Text.AlignHCenter
                            Layout.fillWidth: true
                        }
                    }
                }
                Label { text: tr("Колесо — масштаб · перетаскивание — обзор · щелчок — смещение канала · MHz", "Wheel: zoom · drag: pan · click: channel offset · MHz"); color: muted; font.pixelSize: brand.text.caption.size }
            }
        }
    }
    footer: ColumnLayout {
        spacing: 0
        Rectangle {
            visible: String(receiver.status.error || "").length > 0
            Layout.fillWidth: true
            implicitHeight: errorLabel.implicitHeight + 18
            color: brand.colors.dangerSoft
            Label { id: errorLabel; anchors.fill: parent; anchors.margins: 9; text: receiver.status.error || ""; wrapMode: Text.WordWrap; color: brand.colors.danger }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 32
            color: surface
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Label { font.family: brand.mono; font.pixelSize: brand.text.readout.size; text: (Number(receiver.status.rate || 0)/1e6).toFixed(3) + " MSPS"; color: muted }
                Label { font.family: brand.mono; font.pixelSize: brand.text.readout.size; text: tr("IQ разрывы: ", "IQ gaps: ") + Number(receiver.status.gaps || 0); color: muted; Layout.leftMargin: 14 }
                Label { font.family: brand.mono; font.pixelSize: brand.text.readout.size; text: tr("Очередь: ", "Queue: ") + Number(receiver.status.queueDepth || 0) + "/8"; color: muted; Layout.leftMargin: 14 }
                Label { font.family: brand.mono; font.pixelSize: brand.text.readout.size; text: "Audio underrun: " + Number(receiver.status.audioUnderruns || 0); color: muted; Layout.leftMargin: 14 }
                Label { font.family: brand.mono; font.pixelSize: brand.text.readout.size; text: Number(receiver.status.processingMs || 0).toFixed(2) + " ms/block"; color: muted; Layout.leftMargin: 14 }
                Item { Layout.fillWidth: true }
                Label { font.family: brand.mono; font.pixelSize: brand.text.readout.size; text: receiver.status.recording ? "● " + tr("Запись", "Recording") : tr("Локально · без телеметрии", "Local · no telemetry"); color: receiver.status.recording ? brand.colors.rec : muted }
            }
        }
    }
    FileDialog { id: iqFile; title: tr("Открыть CF32 little-endian IQ", "Open CF32 little-endian IQ"); nameFilters: ["Raw IQ (*.cf32 *.iq *.raw)", "All files (*)"]; onAccepted: { window.filePath = selectedFile.toString(); sourceBox.currentIndex = 1; tabs.currentIndex = 2 } }
    FileDialog { id: pluginFile; title: tr("Добавить проверенное расширение", "Add a trusted extension"); nameFilters: ["Plugin manifest (*.json)"]; onAccepted: receiver.loadPlugin(selectedFile.toString()) }
    FileDialog { id: diagnosticsFile; fileMode: FileDialog.SaveFile; defaultSuffix: "jsonl"; title: tr("Экспорт локальной диагностики", "Export local diagnostics"); onAccepted: receiver.exportDiagnostics(selectedFile.toString()) }
    FolderDialog { id: folder; title: tr("Папка для записей", "Recording folder"); onAccepted: window.recordingFolder = selectedFolder.toString() }
    Connections { target: receiver; function onSpectrumReady(frame) { spectrum.setFrame(frame) } function onDevicesChanged() { window.restoreSource() } }
}
