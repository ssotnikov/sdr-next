import QtQuick
import QtQuick.Window
import QtTest

TestCase {
    name: "ReceiverWindow"
    when: windowShown
    property var appWindow
    function initTestCase() {
        const component = Qt.createComponent("../../app/Main.qml")
        compare(component.status, Component.Ready, component.errorString())
        appWindow = component.createObject(null)
        verify(appWindow !== null)
        tryVerify(function() { return receiver.devices.length >= 2 }, 15000)
    }
    function cleanupTestCase() { receiver.stop(); appWindow.close(); appWindow.destroy() }
    function cleanup() { receiver.stop(); tryVerify(function() { return !receiver.status.running }, 10000) }
    function test_brand_theme() {
        const brand = findChild(appWindow, "brandTheme")
        verify(brand !== null)
        tryVerify(function() { return brand.fontsReady })
        verify(brand.sans.indexOf("IBM Plex Sans") >= 0)
        verify(brand.mono.indexOf("IBM Plex Mono") >= 0)
        compare(appWindow.font.pixelSize, 13)
        const frequency = findChild(appWindow, "frequencyField")
        compare(frequency.font.pixelSize, 32)
        verify(frequency.width >= frequency.contentWidth + frequency.leftPadding + frequency.rightPadding)
        compare(findChild(appWindow, "rxButton").height, 28)
        for (const name of ["themeSelector", "languageSelector"]) {
            const selector = findChild(appWindow, name)
            verify(selector.width >= selector.contentItem.contentWidth + selector.leftPadding + selector.rightPadding,
                name + " displays its selected text")
        }
        const spectrum = findChild(appWindow, "spectrum")
        const previousMap = spectrum.colorMap
        spectrum.colorMap = 2
        compare(spectrum.waterfallColor(0), "#0a0d10")
        compare(spectrum.waterfallColor(1), "#fff4e0")
        compare(spectrum.waterfallColor(0.4), "#0f6b8e")
        appWindow.theme = 0
        compare(appWindow.base, "#0a0d10")
        compare(spectrum.traceColor, "#8fd8ff")
        compare(brand.colors.traceFill, "#338fd8ff")
        appWindow.theme = 1
        compare(appWindow.base, "#f4f6f8")
        compare(appWindow.ink, "#0e141a")
        compare(spectrum.traceColor, "#0b5f8a")
        const hint = findChild(appWindow, "emptySpectrumHint")
        verify(hint.y + hint.height < spectrum.height * 0.36, "Idle hint stays on the theme surface")
        compare(brand.colors.traceFill, "#260b5f8a")
        appWindow.theme = 0
        spectrum.colorMap = previousMap
        const button = findChild(appWindow, "rxButton")
        button.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(button, "visualFocus", true)
        verify(button.background.children[0].visible)
    }
    function test_brand_minimum_window() {
        const oldWidth = appWindow.width
        const oldHeight = appWindow.height
        appWindow.width = 1000
        appWindow.height = 650
        wait(100)
        const rx = findChild(appWindow, "rxButton")
        const frequency = findChild(appWindow, "frequencyField")
        const point = rx.mapToItem(appWindow.contentItem, rx.width, 0)
        verify(point.x <= appWindow.width, "Tuning controls fit the minimum window")
        verify(frequency.width >= frequency.contentWidth + frequency.leftPadding + frequency.rightPadding)
        appWindow.width = oldWidth
        appWindow.height = oldHeight
    }
    function test_window_header() {
        verify(appWindow.flags & Qt.FramelessWindowHint)
        const titleBar = findChild(appWindow, "windowTitleBar")
        compare(findChild(appWindow, "titleLogo"), null)
        const sdr = findChild(appWindow, "wordmarkSdr")
        const next = findChild(appWindow, "wordmarkNext")
        compare(sdr.text, "SDR")
        compare(next.text, "Next")
        compare(sdr.font.pixelSize, 16)
        compare(next.font.pixelSize, 16)
        const resize = findChild(appWindow, "windowResizeHandles")
        compare(titleBar.height, 44)
        const drag = findChild(appWindow, "titleDragRegion")
        verify(drag.width > 400)
        compare(drag.height, 44)
        compare(findChild(appWindow, "mainMenuButton").width, 34)
        compare(findChild(appWindow, "maximizeButton").width, 46)
        compare(resize.width, appWindow.width)
        compare(resize.height, appWindow.height)
        appWindow.theme = 0
        compare(sdr.color, "#e6ebf0")
        compare(next.color, "#ffb224")
        appWindow.theme = 1
        compare(sdr.color, "#0e141a")
        compare(next.color, "#8a4b00")
        appWindow.theme = 0
        const width = appWindow.width, height = appWindow.height
        titleBar.toggleMaximized()
        tryCompare(appWindow, "visibility", Window.Maximized)
        compare(resize.enabled, false)
        titleBar.toggleMaximized()
        tryCompare(appWindow, "visibility", Window.Windowed)
        compare(resize.enabled, true)
        appWindow.width = width; appWindow.height = height
    }
    function test_menu_keyboard_during_rx() {
        findChild(appWindow, "sourceSelector").currentIndex = 0
        findChild(appWindow, "rateSelector").editText = "2400000"
        appWindow.start()
        tryVerify(function() { return receiver.status.running }, 10000)
        appWindow.requestActivate()
        const menu = findChild(appWindow, "mainMenu")
        mouseClick(findChild(appWindow, "mainMenuButton"))
        tryCompare(menu, "visible", true)
        compare(findChild(appWindow, "rxEscapeShortcut").enabled, false)
        compare(findChild(appWindow, "rxSpaceShortcut").enabled, false)
        keyClick(Qt.Key_Escape)
        tryCompare(menu, "visible", false)
        compare(receiver.status.running, true)
        keyClick(Qt.Key_F10)
        tryCompare(menu, "visible", true)
        keyClick(Qt.Key_Escape)
        tryCompare(menu, "visible", false)
        compare(receiver.status.running, true)
        const oldSide = appWindow.panelRight
        findChild(appWindow, "movePanelAction").trigger()
        compare(appWindow.panelRight, !oldSide)
        findChild(appWindow, "movePanelAction").trigger()
        compare(appWindow.panelRight, oldSide)
    }
    function test_endurance_controls_locked() {
        try {
            appWindow.enduranceActive = true
            compare(findChild(appWindow, "mainMenuButton").enabled, false)
            compare(findChild(appWindow, "mainMenuShortcut").enabled, false)
            compare(findChild(appWindow, "menuRx").enabled, false)
            compare(findChild(appWindow, "rxSpaceShortcut").enabled, false)
            compare(findChild(appWindow, "rxEscapeShortcut").enabled, false)
            compare(findChild(appWindow, "rxButton").enabled, false)
            compare(findChild(appWindow, "controlPanel").enabled, false)
        } finally {
            appWindow.enduranceActive = false
        }
        compare(findChild(appWindow, "rxSpaceShortcut").enabled, true)
        compare(findChild(appWindow, "rxEscapeShortcut").autoRepeat, false)
    }
    function test_receiver_and_workspace() {
        const sources = findChild(appWindow, "sourceSelector")
        const rate = findChild(appWindow, "rateSelector")
        const rx = findChild(appWindow, "rxButton")
        verify(sources && rate && rx)
        sources.currentIndex = 0
        rate.editText = "2400000"
        mouseClick(rx)
        tryVerify(function() { return receiver.status.running }, 10000)
        const initialFrames = Number(receiver.status.frames)
        tryVerify(function() { return receiver.status.frames > initialFrames + 20 }, 10000)
        compare(receiver.status.error, "")
        compare(receiver.status.gaps, 0)
        const spectrum = findChild(appWindow, "spectrum")
        spectrum.span = 0.5
        compare(spectrum.span, 0.5)
        spectrum.position = 0.99
        verify(spectrum.position <= 0.75)
        appWindow.theme = 1
        compare(appWindow.dark, false)
        appWindow.theme = 0
        compare(appWindow.dark, true)
        appWindow.language = "en"
        compare(rx.text, "Stop")
        appWindow.language = "ru"
        appWindow.panelRight = true
        wait(100)
        verify(findChild(appWindow, "controlPanel").x > findChild(appWindow, "plotPanel").x)
        appWindow.panelRight = false
        mouseClick(rx)
        tryVerify(function() { return !receiver.status.running }, 10000)
    }
    function test_hackrf_rx() {
        if (!testHackrf) skip("Physical HackRF test requires SDR_TEST_HACKRF=1")
        let index = -1
        for (let i=0; i<receiver.devices.length; ++i)
            if (String(receiver.devices[i].id).indexOf("device:org.sdr.device.hackrf:") === 0) index = i
        verify(index >= 0, "HackRF must be connected")
        const initial = Number(receiver.status.frames)
        receiver.start(index, {centerMHz: 100, rate: 2400000, offsetKHz: 0, mode: "WFM", gain: 16, ppm: 0, biasT: false, directSampling: 0, squelch: -120})
        tryVerify(function() { return receiver.status.running }, 10000)
        tryVerify(function() { return receiver.status.frames > initial + 40 }, 10000)
        compare(receiver.status.error, "")
        compare(receiver.status.gaps, 0)
        console.info("HackRF short GUI RX: " + JSON.stringify(receiver.status))
        compare(receiver.status.audioUnderruns, 0)
        receiver.stop()
        tryVerify(function() { return !receiver.status.running }, 10000)
    }
    function test_saved_source_identity() {
        const devices = [{id:"synthetic"},{id:"device:vendor.one:key"}]
        compare(appWindow.sourceIndex("soapy:key", devices), 1)
        compare(appWindow.sourceIndex("device:vendor.one:key", devices), 1)
        compare(appWindow.sourceIndex("soapy:missing", devices), -1)
        compare(appWindow.sourceIndex("device:missing:key", devices), -1)
        devices.push({id:"device:vendor.two:key"})
        compare(appWindow.sourceIndex("soapy:key", devices), -1)
    }
    function test_unsupported_settings_not_sent() {
        findChild(appWindow,"sourceSelector").currentIndex=0
        const ppm=findChild(appWindow,"ppmSetting")
        const bias=findChild(appWindow,"biasSetting")
        const direct=findChild(appWindow,"directSetting")
        const previous=[ppm.text,bias.checked,direct.currentIndex]
        ppm.text="25"; bias.checked=true; direct.currentIndex=2
        const parameters=appWindow.parameters()
        compare(parameters.ppm,0)
        compare(parameters.biasT,false)
        compare(parameters.directSampling,0)
        ppm.text=previous[0]; bias.checked=previous[1]; direct.currentIndex=previous[2]
    }
}
