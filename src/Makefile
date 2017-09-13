#------------------------------------------------------------------------------
#作    者：葛浩微
#版本说明：
#备    注：无
#------------------------------------------------------------------------------

#子目录列表
SUBDIRS=libcommon libmemcache_client libzookeeper_client libdcs_client monitor dcs_cache_srv dcs_master_srv

#tagets
all:
	mkdir -p bin cfg
	list='$(SUBDIRS)';\
	for subdir in $$list; do \
		if [ -d $$subdir ]; then \
			cd $$subdir && make; \
			cd -;\
		fi; \
	done; \

clean:
	list='$(SUBDIRS)';\
	for subdir in $$list; do \
		if [ -d $$subdir ]; then \
			cd $$subdir && make clean; \
			cd -;\
		fi; \
	done; \

build:
	./build.sh

