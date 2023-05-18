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

    virtual int handleRead(ReadEvent& e) override {
        std::unique_ptr<Message> msg = parse(e.fd, e.data);
        if (!msg->hasData()) {
            EV_LOGE(e) << "failed to parse " << static_cast<std::string>(*msg) << std::endl;
            return 0;
        }

        const auto& qip = QString::fromStdString(e.addr);
        if (e.fd == mScanner.fd()) {
            emit deviceDiscovered(qip);
            return 0;
        } else {
            const auto& doc = QJsonDocument::fromJson(QByteArray::fromStdString(msg->data().dump()));
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
