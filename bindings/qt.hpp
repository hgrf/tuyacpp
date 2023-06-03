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
        mLoop.attach(this);
    }

    ~TuyaWorker() {
        mLoop.detach(this);
    }

    tuya::Scanner& scanner() {
        return mScanner;
    }

    virtual void handleClose(CloseEvent& e) override {
        const auto& qip = QString::fromStdString(e.addr);
        emit deviceDisconnected(qip);
    }

    virtual void handleConnected(ConnectedEvent& e) override {
        const auto& qip = QString::fromStdString(e.addr);
        emit deviceConnected(qip);
    }

    virtual void handleMessage(MessageEvent& e) override {
        const auto& qip = QString::fromStdString(e.addr);
        if (e.fd == mScanner.fd()) {
            emit deviceDiscovered(qip);
        } else {
            const auto& doc = QJsonDocument::fromJson(QByteArray::fromStdString(e.msg.data().dump()));
            emit newDeviceData(qip, doc);
        }
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
    void deviceConnected(QString ip);
    void deviceDisconnected(QString ip);
    void deviceDiscovered(QString ip);
    void newDeviceData(QString ip, QJsonDocument data);

private:
    LOG_MEMBERS(WORKER);

    Loop mLoop;
    tuya::Scanner mScanner;
};

} // namespace tuya
