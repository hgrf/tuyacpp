#pragma once

#include <QJsonDocument>
#include <QThread>

#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

#include "../scanner.hpp"

namespace tuya {

class TuyaWorker : public QThread, public SocketHandler {
    Q_OBJECT

public:
    /* workaround to initialize mLoop before SocketHandler, which needs an initialized loop as argument */
    TuyaWorker(std::unique_ptr<tuya::Loop> loop = std::make_unique<tuya::Loop>()) : SocketHandler(*loop), mLoop(std::move(loop)), mScanner(*mLoop) {
    }

    tuya::Scanner& scanner() {
        return mScanner;
    }

    virtual int handleRead(Event e, const std::string& ip, const ordered_json& data) override {
        LOGI() << "new message from " << ip << ": " << data << std::endl;

        const auto& qip = QString::fromStdString(ip);
        if (e.fd == mScanner.fd()) {
            emit deviceDiscovered(qip);
            return 0;
        } else {
            const auto& doc = QJsonDocument::fromJson(QByteArray::fromStdString(data.dump()));
            emit newDeviceData(qip, doc);
        }
        return 0;
    }

    virtual void run() override {
        for (;;) {
            try {
                mLoop->loop();
            } catch (const std::runtime_error& e) {
                LOGE() << "runtime error: " << e.what() << std::endl;
                return;
            }
        }
    }

signals:
    void deviceDiscovered(QString ip);
    void newDeviceData(QString ip, QJsonDocument data);

private:
    LOG_MEMBERS(WORKER);

    std::unique_ptr<tuya::Loop> mLoop;
    tuya::Scanner mScanner;
};

} // namespace tuya
