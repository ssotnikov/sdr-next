#include "controller.hpp"
#include "spectrum_item.hpp"
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQuickTest/quicktest.h>

class Setup : public QObject {
    Q_OBJECT
    std::unique_ptr<Controller> controller_;
  public slots:
    void applicationAvailable() {
#ifdef Q_OS_WIN
        qputenv("QT_MEDIA_BACKEND", "windows");
#endif
        QCoreApplication::setApplicationName("sdr-app-ui-tests");
        QStandardPaths::setTestModeEnabled(true);
        QQuickStyle::setStyle("Fusion");
        qmlRegisterType<SpectrumItem>("SDR.Native", 1, 0, "SpectrumView");
    }
    void qmlEngineAvailable(QQmlEngine *engine) {
        controller_ = std::make_unique<Controller>();
        engine->rootContext()->setContextProperty("receiver", controller_.get());
        engine->rootContext()->setContextProperty(
            "testHackrf", qEnvironmentVariableIntValue("SDR_TEST_HACKRF") == 1);
        engine->rootContext()->setContextProperty(
            "testAudioOutput", qEnvironmentVariableIntValue("SDR_TEST_AUDIO_OUTPUT") == 1);
    }
};
QUICK_TEST_MAIN_WITH_SETUP(sdr_ui, Setup)
#include "ui_setup.moc"
