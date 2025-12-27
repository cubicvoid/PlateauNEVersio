# Project Name
TARGET = PlateauTest

# Sources
CPP_SOURCES = PlateauTest.cpp
CPP_SOURCES += utilities/Utilities.cpp
CPP_SOURCES += dsp/filters/OnePoleFilters.cpp
CPP_SOURCES += dsp/delays/InterpDelay.cpp
CPP_SOURCES += Dattorro.cpp
CPP_SOURCES += src/bogaudio.cpp
CPP_SOURCES += src/Lmtr.cpp
CPP_SOURCES += src/utils.cpp
CPP_SOURCES += src/dsp/analyzer.cpp
CPP_SOURCES += src/dsp/math.cpp
CPP_SOURCES += src/dsp/signal.cpp
CPP_SOURCES += src/dsp/table.cpp
CPP_SOURCES += src/dsp/filters/filter.cpp
CPP_SOURCES += src/dsp/filters/utility.cpp

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy/
DAISYSP_DIR = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
