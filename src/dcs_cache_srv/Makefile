#------------------------------------------------------------------------------
#作    者：葛浩微
#版本说明：无
#备    注：无
#------------------------------------------------------------------------------
PROJECT_DIR ?= $(shell pwd)/..
include ../Makefile.inc

TARGET=$(shell basename `pwd`)

TARGET_CPPS := $(shell ls *.cpp)
TARGET_OBJS=$(TARGET_CPPS:.cpp=.o)

.SUFFIXES:              #删除默认的后缀
.SUFFIXES: .cpp .o .h   #定义自己的后缀

all:${TARGET}

${TARGET}:${TARGET_OBJS} ${DCS_INNER_LIB}
	$(CXX) -o $@ $^ ${LDFLAGS}

%.o:%.cpp %.h ${DCS_DEPENDENSE_INC}
	$(CXX) -c $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	-rm -rf ${TARGET} ${TARGET_OBJS}
