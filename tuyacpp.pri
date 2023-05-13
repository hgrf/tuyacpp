include(crc.pri)
include(json.pri)

INCLUDEPATH += $$PWD

DEFINES += TUYA_SINGLE_HEADER

HEADERS += \
    $$PWD/loop/event.hpp \
    $$PWD/loop/loop.hpp \
    $$PWD/protocol/common.hpp \
    $$PWD/protocol/protocol.hpp \
    $$PWD/protocol/message.hpp \
    $$PWD/protocol/message55aa.hpp \
    $$PWD/logging.hpp \
    $$PWD/device.hpp \
    $$PWD/scanner.hpp

SOURCES += \
    $$PWD/device.cpp \
    $$PWD/protocol/protocol.cpp

LIBS += -lcrypto -lssl

CONFIG( tuyaCpp ){
    !build_pass:message(using tuyaCpp)
}
