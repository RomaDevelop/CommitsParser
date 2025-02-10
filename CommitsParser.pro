QT += core gui widgets

CONFIG += c++17

SOURCES += \
    git.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ../include/MyQTimer.h \
    git.h \
    mainwindow.h

INCLUDEPATH += ../include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
