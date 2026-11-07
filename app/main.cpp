#include "controller.hpp"
#include "spectrum_item.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("sdr-app");
    QCoreApplication::setApplicationName("sdr-app");
    QCoreApplication::setApplicationVersion("0.1.0");
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND"))
        qputenv("QT_MEDIA_BACKEND", "windows");
#endif
    QQuickStyle::setStyle("Fusion");
    qmlRegisterType<SpectrumItem>("SDR.Native", 1, 0, "SpectrumView");
    Controller controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("receiver", &controller);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("SDR", "Main");
    const auto args = app.arguments();
    const auto index = args.indexOf("--smoke-test");
    if (index >= 0)
        QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    return app.exec();
}
