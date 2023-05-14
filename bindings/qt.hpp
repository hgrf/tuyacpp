#pragma once

#include <QThread>

#include "../scanner.hpp"
#include "../logging.hpp"

namespace tuya {

class TuyaWorker : public QThread {
    Q_OBJECT

public:
    void run() {
        LOGI() << "Worker started" << std::endl;

        tuya::Loop loop;
        tuya::Scanner scanner(loop);
        for (;;) {
            loop.loop();
        }

        LOGI() << "Worker done" << std::endl;
    }


private:
    LOG_MEMBERS(WORKER);
};

} // namespace tuya
