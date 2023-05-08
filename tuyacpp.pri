INCLUDEPATH += $$PWD

# SOURCES +=

HEADERS += \
    $$PWD/protocol/common.hpp \
    $$PWD/protocol/protocol.hpp \
    $$PWD/protocol/message.hpp \
    $$PWD/protocol/message55aa.hpp \
    $$PWD/device.hpp \
    $$PWD/scanner.hpp

LIBS += -lcrypto -lssl

CONFIG( tuyaCpp ){
    !build_pass:message(using tuyaCpp)
}
