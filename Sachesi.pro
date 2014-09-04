QT += network gui widgets quick qml

TARGET="Sachesi"
win32: RC_ICONS += assets/sachesi-114.ico
else:mac: ICON = assets/sachesi-114.icns
else: ICON = assets/sachesi-114.png
VERSION = 1.6.0

# Global specific
CONFIG += c++11
INCLUDEPATH += ext src
P = $$_PRO_FILE_PWD_

win32 {
    SOURCES += $$P/ext/zlib-win/*.c
    HEADERS += $$P/ext/zlib-win/*.h
    INCLUDEPATH += ext/zlib-win
}

win32 {
    # Where is your OpenSSL Install? Hardcoded for Win32
    OPENSSL_PATH = C:\\OpenSSL
    INCLUDEPATH += $$OPENSSL_PATH\\include

    # Is all-in-one binary?
    CONFIG += static
    static: DEFINES += STATIC STATIC_BUILD

    mingw: LIBS += -L$$OPENSSL_PATH -llibssl -llibcrypto -lgdi32

    !mingw {
        DEFINES += NOMINMAX _CRT_SECURE_NO_WARNINGS
        static: LIBS += -L$$OPENSSL_PATH\\lib\\VC\\static -llibeay32MT -lssleay32MT -lGDI32 -lAdvapi32
        else: LIBS += -L$$OPENSSL_PATH\\lib -llibeay32
        # Hardcoded lib folder for winsocks
        LIBS+= -L"C:\\Program Files (x86)\\Windows Kits\\8.1\\Lib\\winv6.3\\um\\x86" -lWSock32 -lUser32 -lCrypt32
    }
}
else:blackberry {
    DEFINES += BLACKBERRY
    LIBS += -lbbcascadespickers -lbbsystem -lQtXml #-lcrypto
}
else:mac {
    INCLUDEPATH += /opt/local/include
    LIBS+= -lcrypto -lssl -lz -framework CoreFoundation -framework IOKit -lobjc /opt/local/lib/libusb-1.0.a
    DEFINES += BOOTLOADER_ACCESS
}
else:freebsd-*|openbsd-* {
    isEmpty(PREFIX): PREFIX = /usr/local/
    LIBS += -lz -lcrypto -lusb
}
else:android {
    LIBS += -L $$P/Android -lcrypto -lssl -lusb1.0
    INCLUDEPATH += $$P/Android/include/
    ANDROID_EXTRA_LIBS += $$P/Android/libusb1.0.so
    ANDROID_PACKAGE_SOURCE_DIR = $$P/Android
    DEFINES += BOOTLOADER_ACCESS
} else {
    LIBS += -lz -ldl -ludev
    # These below should be static for it to be fully portable (changing ABIs)
    LIBS += -lcrypto -lusb-1.0
    DEFINES += BOOTLOADER_ACCESS
}

SOURCES += \
    src/main.cpp \
    src/mainnet.cpp \
    src/splitter.cpp \
    src/lzo.cpp \
    src/ports.cpp

HEADERS += \
    src/mainnet.h \
    src/splitter.h \
    src/lzo.h \
    src/ports.h

# Welcome to the only OS that won't give network access to USB device
!blackberry {
    SOURCES += \
        src/installer.cpp \
        src/installer_qml.cpp \
        src/installer_establish.cpp \
        src/installer_auth.cpp \
        src/apps.cpp \
        src/backupinfo.cpp
    HEADERS += \
        src/installer.h \
        src/apps.h \
        src/backupinfo.h
}

# This requires libusb to be linked
contains(DEFINES, BOOTLOADER_ACCESS) {
    SOURCES += src/boot.cpp
    HEADERS += src/boot.h
}

DEFINES += QUAZIP_STATIC
SOURCES += $$P/ext/quazip/*.cpp $$P/ext/quazip/*.c
HEADERS += $$P/ext/quazip/*.h

RESOURCES += UI.qrc
# The qmldir is built in for dynamic libs but not static.
static: RESOURCES += QML.qrc
OTHER_FILES += \
    qml/generic/mcc.js \
    qml/generic/*.qml \
    qml/generic/UI/*.qml \
    Android/AndroidManifest.xml
