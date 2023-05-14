include(crc.pri)
include(json.pri)

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/loop/event.hpp \
    $$PWD/loop/handler.hpp \
    $$PWD/loop/sockethandler.hpp \
    $$PWD/loop/loop.hpp \
    $$PWD/protocol/message.hpp \
    $$PWD/protocol/message55aa.hpp \
    $$PWD/logging.hpp \
    $$PWD/device.hpp \
    $$PWD/scanner.hpp \
    $$PWD/bindings/qt.hpp

LIBS += -lcrypto -lssl

CONFIG( tuyaCpp ){
    !build_pass:message(using tuyaCpp)
}
