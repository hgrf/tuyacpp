#pragma once

#include <QThread>

#include "../scanner.hpp"

namespace tuya {

class TuyaWorker : public QThread {
    Q_OBJECT

public:
    TuyaWorker() : mScanner(mLoop) {
    }

    tuya::Scanner& scanner() {
        return mScanner;
    }

    void run() {
        for (;;) {
            mLoop.loop();
        }
    }

private:
    tuya::Loop mLoop;
    tuya::Scanner mScanner;
};

} // namespace tuya
