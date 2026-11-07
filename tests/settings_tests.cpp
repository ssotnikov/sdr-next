#include "controller.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName("sdr-settings-test-" +
                                         QUuid::createUuid().toString(QUuid::Id128));
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const auto path = directory + "/settings.json";
    const auto require = [](bool condition) {
        if (!condition)
            throw std::runtime_error("Settings regression");
    };
    try {
        {
            Controller controller;
            controller.saveSettings({{"gain", 0}, {"theme", 0}, {"sourceId", "file"}});
        }
        {
            Controller controller;
            require(controller.settings()["gain"].toInt() == 0);
            require(controller.settings()["sourceId"] == "file");
        }
        for (const QByteArray data :
             {QByteArray("{broken"), QByteArray("{\"schema_version\":99}")}) {
            {
                QFile file(path);
                require(file.open(QIODevice::WriteOnly));
                require(file.write(data) == data.size());
            }
            {
                Controller controller;
                require(controller.status()["error"].toString().contains("preserved"));
                controller.stop();
                controller.saveSettings({{"gain", 10}});
            }
            QFile file(path);
            require(file.open(QIODevice::ReadOnly));
            require(file.readAll() == data);
        }
        // Only the unique test-owned settings file and empty directory are removed.
        require(QFile::remove(path));
        QDir().rmdir(directory);
        std::cout << "Settings persistence and corrupt-file preservation passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
