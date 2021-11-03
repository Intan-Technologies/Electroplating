#-------------------------------------------------
#
# Project created by QtCreator 2017-04-18T13:48:56
#
#-------------------------------------------------

TEMPLATE      = app

QT            += widgets multimedia

CONFIG        += static

macx:{
QMAKE_RPATHDIR += /users/intan/Qt/5.7/clang_64/lib
QMAKE_RPATHDIR += /users/intan/downloads/
}


SOURCES += main.cpp\
        mainwindow.cpp \
    configurationwindow.cpp \
    globalconfigurationwindow.cpp \
    okFrontPanelDLL.cpp \
    rhd2000evalboard.cpp \
    rhd2000datablock.cpp \
    rhd2000registers.cpp \
    oneelectrode.cpp \
    dataprocessor.cpp \
    rhd2000config.cpp \
    signalchannel.cpp \
    signalgroup.cpp \
    signalsources.cpp \
    signalprocessor.cpp \
    boardcontrol.cpp \
    saveformat.cpp \
    streams.cpp \
    impedancemeasurecontroller.cpp \
    common.cpp \
    electroplatingboardcontrol.cpp \
    significantround.cpp \
    impedanceplot.cpp

HEADERS  += mainwindow.h \
    configurationwindow.h \
    configurationparameters.h \
    globalparameters.h \
    globalconfigurationwindow.h \
    okFrontPanelDLL.h \
    rhd2000evalboard.h \
    rhd2000datablock.h \
    rhd2000registers.h \
    oneelectrode.h \
    globalconstants.h \
    settings.h \
    dataprocessor.h \
    electrodeimpedance.h \
    rhd2000config.h \
    signalchannel.h \
    signalgroup.h \
    signalsources.h \
    signalprocessor.h \
    boardcontrol.h \
    saveformat.h \
    streams.h \
    impedancemeasurecontroller.h \
    common.h \
    electroplatingboardcontrol.h \
    significantround.h \
    impedanceplot.h


macx: {
LIBS += -L$$PWD/../../../Downloads/ -lokFrontPanel
INCLUDEPATH += $$PWD/../../../Downloads
DEPENDPATH += $$PWD/../../../Downloads
}
