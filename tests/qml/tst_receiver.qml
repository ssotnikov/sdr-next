import QtQuick
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
            if (String(receiver.devices[i].id).indexOf("soapy:") === 0 && String(receiver.devices[i].id).indexOf("driver=hackrf") >= 0) index = i
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
}
