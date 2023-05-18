#pragma once

#include <QJsonDocument>
#include <QThread>

#include <nlohmann/json.hpp>
using ordered_json = nlohmann::ordered_json;

#include "../scanner.hpp"

namespace tuya {

class TuyaWorker : public QThread, public Handler {
    Q_OBJECT

public:
    /* workaround to initialize mLoop before SocketHandler, which needs an initialized loop as argument */
    TuyaWorker() : mScanner(mLoop) {
        mLoop.attachExtra(this);
    }

    ~TuyaWorker() {
        mLoop.detachExtra(this);
    }

    tuya::Scanner& scanner() {
        return mScanner;
    }

    virtual void handleMessage(MessageEvent& e) override {
        const auto& qip = QString::fromStdString(e.addr);
        if (e.fd == mScanner.fd()) {
            emit deviceDiscovered(qip);
            return;
        } else {
            const auto& doc = QJsonDocument::fromJson(QByteArray::fromStdString(e.msg.data().dump()));
            emit newDeviceData(qip, doc);
        }
        return;
    }

    virtual void run() override {
        for (;;) {
            try {
                mLoop.loop();
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

    Loop mLoop;
    tuya::Scanner mScanner;
};

} // namespace tuya
