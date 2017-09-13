#------------------------------------------------------------------------------
#作    者：葛浩微
#版本说明：无
#备    注：无
#------------------------------------------------------------------------------
PROJECT_DIR ?= $(shell pwd)/..
include ../Makefile.inc

TARGET_A=$(shell basename `pwd`).a
TARGET_SO=$(shell basename `pwd`).so

TARGET_CPPS := $(shell ls *.cpp)
TARGET_OBJS=$(TARGET_CPPS:.cpp=.o)

.SUFFIXES:              #删除默认的后缀
.SUFFIXES: .cpp .o .h   #定义自己的后缀

all:${TARGET_A} ${TARGET_SO}

${TARGET_A}:${TARGET_OBJS} ${DCS_INNER_LIB}
	${BUILD_LIBRARY}

${TARGET_SO}:${TARGET_OBJS} ${DCS_SUPPORT_SO}
	$(CXX) -shared -fPIC -o $@ $^ ${LDFLAGS}

%.o:%.cpp %.h ${DCS_DEPENDENSE_INC}
	$(CXX) -c $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	-rm -rf ${TARGET_A} ${TARGET_SO} ${TARGET} ${TARGET_OBJS}
