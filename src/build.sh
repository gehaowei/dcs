#!/bin/sh
#  author:gehw
#
DCS_VERSION=_v1.0
CUR_DIR=`pwd`

rm -rf ${CUR_DIR}/release
mkdir -p ${CUR_DIR}/release

#*********step 1:libdcs_client**********************
MODULE_NAME=libdcs_client
cd ${CUR_DIR}/release
tmpdir=${MODULE_NAME}${DCS_VERSION}
rm -rf ${tmpdir}

mkdir -p ${tmpdir}
mkdir -p ${tmpdir}/include
mkdir -p ${tmpdir}/bin
mkdir -p ${tmpdir}/lib
mkdir -p ${tmpdir}/etc

cp -rfav ${CUR_DIR}/${MODULE_NAME}/dcs_client.h ${tmpdir}/include/.
cp -rfav ${CUR_DIR}/${MODULE_NAME}/libdcs_client.so ${tmpdir}/lib/.
cp -rfav ${CUR_DIR}/support/log4cplus-1.0.4-rc7-modify/lib/liblog4cplus*so* ${tmpdir}/lib/.
cp -rfav ${CUR_DIR}/support/libmemcached-1.0.4/libmemcached/.libs/libmemcached.so* ${tmpdir}/lib/.
cp -rfav ${CUR_DIR}/support/zookeeper-3.4.5/src/c/.libs/libzookeeper_mt.so* ${tmpdir}/lib/.
#cp -rfav ${CUR_DIR}/support/ICE/lib/libIce.so* ${tmpdir}/lib/.
#cp -rfav ${CUR_DIR}/support/ICE/lib/libIceUtil.so* ${tmpdir}/lib/.
cp -rfav ${CUR_DIR}/${MODULE_NAME}/libdcs_client.a ${tmpdir}/lib/.
cp -rfav ${CUR_DIR}/${MODULE_NAME}/dcs_client.conf ${tmpdir}/etc/dcs_client.conf.example
cp -rfav ${CUR_DIR}/${MODULE_NAME}/README ${tmpdir}/.

dos2unix ${tmpdir}/README >>/dev/null 2>&1
dos2unix ${tmpdir}/etc/* >>/dev/null 2>&1

chmod -R 775 ${tmpdir}
tar zcvf ${tmpdir}.tar.gz ${tmpdir}
rm -rf ${tmpdir}
echo 
echo "build... ->release/${tmpdir}.tar.gz"
echo 

#***********step 2:monitor***********************
MODULE_NAME=monitor
cd ${CUR_DIR}/release
tmpdir=${MODULE_NAME}${DCS_VERSION}
rm -rf ${tmpdir}

mkdir -p ${tmpdir}
mkdir -p ${tmpdir}/bin
mkdir -p ${tmpdir}/etc

cp -rfav ${CUR_DIR}/${MODULE_NAME}/${MODULE_NAME} ${tmpdir}/bin/.
cp -rfav ${CUR_DIR}/${MODULE_NAME}/${MODULE_NAME}.conf ${tmpdir}/etc/${MODULE_NAME}.conf.example

dos2unix ${tmpdir}/etc/* >>/dev/null 2>&1

chmod -R 775 ${tmpdir}
tar zcvf ${tmpdir}.tar.gz ${tmpdir}
rm -rf ${tmpdir}
echo 
echo "build... ->release/${tmpdir}.tar.gz"
echo 

#***********step 3:dcs_cache_srv************************
MODULE_NAME=dcs_cache_srv
cd ${CUR_DIR}/release
tmpdir=${MODULE_NAME}${DCS_VERSION}
rm -rf ${tmpdir}

mkdir -p ${tmpdir}
mkdir -p ${tmpdir}/bin
mkdir -p ${tmpdir}/etc

cp -rfav ${CUR_DIR}/${MODULE_NAME}/${MODULE_NAME} ${tmpdir}/bin/.
cp -rfav ${CUR_DIR}/${MODULE_NAME}/${MODULE_NAME}.conf ${tmpdir}/etc/${MODULE_NAME}.conf.example

dos2unix ${tmpdir}/etc/* >>/dev/null 2>&1

chmod -R 775 ${tmpdir}
tar zcvf ${tmpdir}.tar.gz ${tmpdir}
rm -rf ${tmpdir}
echo 
echo "build... ->release/${tmpdir}.tar.gz"
echo 

#***********step 4:dcs_master_srv************************
MODULE_NAME=dcs_master_srv
cd ${CUR_DIR}/release
tmpdir=${MODULE_NAME}${DCS_VERSION}
rm -rf ${tmpdir}

mkdir -p ${tmpdir}
mkdir -p ${tmpdir}/bin
mkdir -p ${tmpdir}/etc

cp -rfav ${CUR_DIR}/${MODULE_NAME}/${MODULE_NAME} ${tmpdir}/bin/.
cp -rfav ${CUR_DIR}/${MODULE_NAME}/${MODULE_NAME}.conf ${tmpdir}/etc/${MODULE_NAME}.conf.example

dos2unix ${tmpdir}/etc/* >>/dev/null 2>&1

chmod -R 775 ${tmpdir}
tar zcvf ${tmpdir}.tar.gz ${tmpdir}
rm -rf ${tmpdir}
echo 
echo "build... ->release/${tmpdir}.tar.gz"
echo 

#************step 5:memcached****************************
MODULE_NAME=memcached
cd ${CUR_DIR}/release
tmpdir=${MODULE_NAME}${DCS_VERSION}
rm -rf ${tmpdir}

mkdir -p ${tmpdir}
mkdir -p ${tmpdir}/bin
mkdir -p ${tmpdir}/lib

cp -rfav ${CUR_DIR}/support/memcached-1.4.15/memcached ${tmpdir}/bin/.
cp -rfav ${CUR_DIR}/support/libevent-2.0.20-stable/.libs/libevent-2.0.so.5 ${tmpdir}/lib/.
cp -rfav ${CUR_DIR}/support/libevent-2.0.20-stable/.libs/libevent-2.0.so.5.1.8 ${tmpdir}/lib/.

dos2unix ${tmpdir}/etc/* >>/dev/null 2>&1

chmod -R 775 ${tmpdir}
tar zcvf ${tmpdir}.tar.gz ${tmpdir}
rm -rf ${tmpdir}
echo 
echo "build... ->release/${tmpdir}.tar.gz"
echo 


#************step 5:zookeeper****************************
cp -rfav ${CUR_DIR}/support/zookeeper-3.4.5.tar.gz ${CUR_DIR}/release/.
echo 
echo "build... ->release/zookeeper-3.4.5.tar.gz"
echo 
