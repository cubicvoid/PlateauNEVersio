# Project Name
TARGET = Campestria

# Sources
CPP_SOURCES = src/main.cpp
CPP_SOURCES += lib/ValleyRackFree/utilities/Utilities.cpp
CPP_SOURCES += lib/ValleyRackFree/Plateau/Dattorro.cpp
CPP_SOURCES += lib/ValleyRackFree/dsp/filters/OnePoleFilters.cpp
CPP_SOURCES += lib/ValleyRackFree/dsp/delays/InterpDelay.cpp
CPP_SOURCES += lib/Bogaudio/bogaudio.cpp
CPP_SOURCES += lib/Bogaudio/Lmtr.cpp
CPP_SOURCES += lib/Bogaudio/utils.cpp
CPP_SOURCES += lib/Bogaudio/dsp/analyzer.cpp
CPP_SOURCES += lib/Bogaudio/dsp/math.cpp
CPP_SOURCES += lib/Bogaudio/dsp/signal.cpp
CPP_SOURCES += lib/Bogaudio/dsp/table.cpp
CPP_SOURCES += lib/Bogaudio/dsp/filters/filter.cpp
CPP_SOURCES += lib/Bogaudio/dsp/filters/utility.cpp

# Include paths
C_INCLUDES += \
	-I. \
	-Ilib

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy/
DAISYSP_DIR = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

#-Idsp \
#-Idsp/pvoc \
#-Idsp/fx \
#-Ibootloader \
#-Iresources \

