#基本编译选项
CXX=g++
CC=gcc

#ICE_INCLUDES=-I../support/ICE/include 
#ICE_LIBS=-L../support/ICE/lib -lIce -lIceUtil

LOGGER_INCLUDES=-I../support/log4cplus-1.0.4-rc7-modify/include
LOGGER_LIBS=-L../support/log4cplus-1.0.4-rc7-modify/lib -llog4cplus

DCS_INCLUDES=-I. -I../libcommon
DCS_LIBS=../libcommon/libcommon.a

LIBMEM_INCLUDES=-I../libmemcache_client -I../support/libmemcached-1.0.4
LIBMEM_LIBS=../libmemcache_client/libmemcache_client.a -L../support/libmemcached-1.0.4/libmemcached/.libs -lmemcached

LIBZK_CLIENT_INCLUDES=-I../libzookeeper_client -I../support/zookeeper-3.4.5/src/c/include -I../support/zookeeper-3.4.5/src/c/generated
LIBZK_CLIENT_LIBS=../libzookeeper_client/libzookeeper_client.a -L../support/zookeeper-3.4.5/src/c/.libs -lzookeeper_mt

CFLAGS=-g -O2 -Wall -Wno-unused -fPIC -D__LINUX__  ${DCS_INCLUDES} ${LOGGER_INCLUDES} \
 ${ICE_INCLUDES}  ${LIBMEM_INCLUDES} ${LIBZK_CLIENT_INCLUDES}
LDFLAGS=-rdynamic -ldl -lpthread -lrt

#本项目依赖的所有库，均静态连接到目标
DCS_SUPPORT_A=../libmemcache_client/libmemcache_client.a ../libzookeeper_client/libzookeeper_client.a \
    ../libcommon/libcommon.a \
    ../support/log4cplus-1.0.4-rc7-modify/lib/liblog4cplus.a \
    ../support/libmemcached-1.0.4/libmemcached/.libs/libmemcached.a \
    ../support/zookeeper-3.4.5/src/c/.libs/libzookeeper_mt.a 
#    ../support/ICE/lib/libIce.a ../support/ICE/lib/libIceUtil.a ../support/ICE/lib/libbz2.a

DCS_SUPPORT_SO=../libmemcache_client/libmemcache_client.a ../libzookeeper_client/libzookeeper_client.a \
    ../libcommon/libcommon.a \
    ../support/log4cplus-1.0.4-rc7-modify/lib/liblog4cplus.so \
    ../support/libmemcached-1.0.4/libmemcached/.libs/libmemcached.so \
    ../support/zookeeper-3.4.5/src/c/.libs/libzookeeper_mt.so
#    ../support/ICE/lib/libIce.so ../support/ICE/lib/libIceUtil.so

DCS_INNER_LIB=${DCS_SUPPORT_A}
    
DCS_DEPENDENSE_INC=../libcommon/*.h ../libmemcache_client/*.h ../libzookeeper_client/*.h 


RM=rm -f
ECHO=echo
AR=ar

define BUILD_LIBRARY
$(if $(wildcard $@),@$(RM) $@)
$(if $(wildcard ar.mac),@$(RM) ar.mac)
$(if $(filter lib%.a, $^),
@$(ECHO) CREATE $@ > ar.mac
@$(ECHO) SAVE >> ar.mac
@$(ECHO) END >> ar.mac
@$(AR) -M < ar.mac
)
$(if $(filter %.o,$^),$(AR) -q $@ $(filter %.o, $^))
$(if $(filter %.a, $^),
@$(ECHO) OPEN $@ > ar.mac
$(foreach LIB, $(filter %.a, $^),
$(ECHO) ADDLIB $(LIB) >> ar.mac
)
@$(ECHO) SAVE >> ar.mac
@$(ECHO) END >> ar.mac
@$(AR) -M < ar.mac
@$(RM) ar.mac
)
endef

    


