######################################################################
# Automatically generated by qmake (2.01a) Tue Apr 21 18:41:54 2009
# Manually edited since then! :)
######################################################################

TEMPLATE = app
TARGET = SpikeGLX

DESTDIR = Y:/__billKarsh__/SPIKEGL/SpikeGLX53
#DESTDIR = Y:/__billKarsh__/SPIKEGL/DEBUG53

#DESTDIR = Y:/__billKarsh__/SPIKEGL/SpikeGLX54
#DESTDIR = Y:/__billKarsh__/SPIKEGL/DEBUG54

#DESTDIR = Y:/__billKarsh__/SPIKEGL/SpikeGLX55
#DESTDIR = Y:/__billKarsh__/SPIKEGL/DEBUG55

#DESTDIR = Y:/__billKarsh__/SPIKEGL/SpikeGLX56mingw
#DESTDIR = Y:/__billKarsh__/SPIKEGL/DEBUG56mingw

#DESTDIR = Y:/__billKarsh__/SPIKEGL/SpikeGLX56MSVC
#DESTDIR = Y:/__billKarsh__/SPIKEGL/DEBUG56MSVC

#DESTDIR = Y:/__billKarsh__/SPIKEGL/SpikeGLX56-64
#DESTDIR = Y:/__billKarsh__/SPIKEGL/DEBUG56-64

DEPENDPATH  += $$PWD
INCLUDEPATH += $$PWD

QT += opengl network svg

# Our sources
SRC_SGLX = \
    Src-analog_out \
    Src-datafile \
    Src-filters \
    Src-gates \
    Src-graphs \
    Src-gui_tools \
    Src-main \
    Src-params \
    Src-remote \
    Src-run \
    Src-triggers \
    Src-verify
for(dir, SRC_SGLX) {
    INCLUDEPATH += $$PWD/$$dir
    include($$dir/$$dir".pri")
}

# 3rd party
SRC_ALIEN = \
    Samplerate
for(dir, SRC_ALIEN) {
    INCLUDEPATH += $$PWD/$$dir
    include($$dir/$$dir".pri")
}

# Resources
RSRC = \
    Forms \
    Resources
for(dir, RSRC) {
    include($$dir/$$dir".pri")
}

# Docs
OTHER_FILES += \
    Agenda.txt \
    LICENSE.txt \
    README.md

win32 {
# Note: Psapi.dll supports GetProcessMemoryInfo in CniAcqDmx.
# Note: Simulate IMEC:  comment out:
#   "LIBS       += -llibNeuropix_basestation_api"
#   "DEFINES    += HAVE_IMEC"
# Note: Simulate NIDAQ: comment out:
#   "LIBS       += -lNIDAQmx"
#   "DEFINES    += HAVE_NIDAQmx"
# Note: Switch QGLWidget to QOpenGLWidget: enable:
#   "DEFINES    += OPENGL54"
# Note: "QMAKE_LFLAGS += -Wl,--large-address-aware" for MinGW 32-bit projects

    CONFIG          += embed_manifest_exe
    QMAKE_LIBDIR    += $${_PRO_FILE_PWD_}/IMEC $${_PRO_FILE_PWD_}/NI
    LIBS            += -lWS2_32 -lUser32
    LIBS            += -lopengl32 -lglu32
    LIBS            += -lPsapi
#    LIBS            += -llibNeuropix_basestation_api
#    DEFINES         += HAVE_IMEC
    LIBS            += -lNIDAQmx
    DEFINES         += HAVE_NIDAQmx
#    DEFINES         += OPENGL54
    DEFINES         += _CRT_SECURE_NO_WARNINGS WIN32
    QMAKE_LFLAGS    += -Wl,--large-address-aware
}

unix {
    CONFIG          += debug warn_on
#   QMAKE_CFLAGS    += -Wall -Wno-return-type
#   QMAKE_CXXFLAGS  += -Wall -Wno-return-type
# Enable these for profiling!
#   QMAKE_CFLAGS    += -pg
#   QMAKE_CXXFLAGS  += -pg
#   QMAKE_LFLAGS    += -pg
}

macx {
    LIBS    += -framework CoreServices
    DEFINES += MACX
}


