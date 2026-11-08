#include "controller.hpp"
#include "endurance.hpp"
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
    QGuiApplication::setApplicationDisplayName("SDR Next");
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
    std::unique_ptr<Endurance> endurance;
    const auto endurance_index = args.indexOf("--endurance-dir");
    if (endurance_index >= 0) {
        const auto value = [&](const QString &flag, const QString &fallback) {
            const auto i = args.indexOf(flag);
            return i >= 0 && i + 1 < args.size() ? args[i + 1] : fallback;
        };
        bool valid = false;
        const double duration = value("--endurance-seconds", "28800").toDouble(&valid);
        try {
            if (!valid || engine.rootObjects().isEmpty())
                return 2;
            endurance = std::make_unique<Endurance>(
                controller, qobject_cast<QQuickWindow *>(engine.rootObjects().front()),
                value("--endurance-dir", ""), duration, value("--endurance-provider", ""));
        } catch (const std::exception &e) {
            qCritical("Endurance setup failed: %s", e.what());
            return 2;
        }
    }
    const auto index = args.indexOf("--smoke-test");
    if (index >= 0)
        QTimer::singleShot(2000, &app, &QCoreApplication::quit);
    QTimer receiver_smoke;
    if (args.contains("--smoke-rx")) {
        QTimer::singleShot(100, &app, [&] {
            controller.start(0, {{"centerMHz", 100.0},
                                 {"rate", 2400000.0},
                                 {"offsetKHz", 0.0},
                                 {"mode", "WFM"},
                                 {"gain", 0.0},
                                 {"ppm", 0.0},
                                 {"biasT", false},
                                 {"directSampling", 0},
                                 {"squelch", -120.0}});
        });
        QObject::connect(&receiver_smoke, &QTimer::timeout, &app, [&] {
            const auto status = controller.status();
            if (!status["error"].toString().isEmpty()) {
                app.exit(2);
                return;
            }
            if (status["running"].toBool() && status["frames"].toULongLong() > 20 &&
                status["samples"].toULongLong() >= 2400000) {
                const bool valid =
                    status["gaps"].toULongLong() == 0 && status["overflows"].toULongLong() == 0;
                controller.stop();
                app.exit(valid ? 0 : 2);
            }
        });
        receiver_smoke.start(50);
        QTimer::singleShot(15000, &app, [&] { app.exit(3); });
    }
    return app.exec();
}
