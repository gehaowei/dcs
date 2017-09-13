/*
 * =====================================================================================
 *
 *       Filename:  FileConf.h
 *
 *    Description:  读取conf文件类
 *
 *        Version:  1.0
 *        Created:  11/19/09 12:22:35
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  killam-zhiyong.zeng (TM), zhiyong.zeng@net263.com
 *        Company:  263
 *
 * =====================================================================================
 */


#ifndef __FILE_CONF_H__
#define __FILE_CONF_H__

#include <map>
#include <vector>
#include "file_base.h"
#include <string.h>
#include <stdlib.h>

namespace DCS
{

/*
 * =====================================================================================
 *        Class:  FileConf
 *  Description:
 * =====================================================================================
 */
#define INI_PROFILE_SECTION_LEN 64
#define INI_PROFILE_KEY_LEN 64
#define INI_PROFILE_VALUE_LEN 256

typedef struct str_ini
{
	char section[ INI_PROFILE_SECTION_LEN + 1 ];
	char key[ INI_PROFILE_KEY_LEN + 1 ];
	char value[ INI_PROFILE_VALUE_LEN + 1 ];
	struct str_ini *next;
} ini_profile_t;

/*
 * ****************************************************
 * linux配置文件格式
 * ****************************************************
 */
class FileConf: public FileBase
{
	public:
		/* ====================  LIFECYCLE     ======================================= */
		FileConf(){}     /* constructor */
		FileConf( char *filename ): FileBase( filename ), m_conflist( NULL ){}
		FileConf( const std::string &filename ): FileBase( filename ), m_conflist( NULL ){}
		virtual ~FileConf()
		{
			this->free();
		}

		/* ====================  ACCESSORS     ======================================= */
		//模式 #, //, /**/
		//virtual string &filter( string &content, char *pattern );
		virtual std::string &filter( std::string &contenet, int flag = 7 );
		virtual void load( int flag = 7 );
		virtual std::map< std::string, std::string > &getMap(); //map方式获得，使用getValue取值
		virtual ini_profile_t *getList(); //list方式获得，使用getData取值
		virtual ini_profile_t *getList( const std::string &key ); //获取title后的第一个列表
		virtual char *getData( const std::string &key );
		virtual char *getData( const std::string &section, const std::string &key );
		virtual int getIntData( const std::string &section, const std::string &key );
		virtual std::string &getValue( const std::string &key );
		void setValue( const std::string &key, const std::string &value );
		void setValue( const std::string &section, const std::string &key, const std::string &value );
		void free();

		/* ====================  MUTATORS      ======================================= */

		/* ====================  OPERATORS     ======================================= */

		/* ====================  DATA MEMBERS  ======================================= */
	protected:

	private:
	    //string m_content;
	    std::map< std::string, std::string > m_conf;

	    //链表存储数据
	    ini_profile_t *m_conflist;

}; /* -----  end of class FileConf  ----- */

/*
 * ****************************************************
 * ini配置文件格式
 * ****************************************************
 */
class FileConfIni: public FileConf
{
	public:
	    FileConfIni( char *filename ): FileConf( filename ){ load(); }
	    FileConfIni( const std::string &filename ): FileConf( filename ){ load(); }
	    virtual ~FileConfIni(){}

	    virtual void load( int flag = 7 );
};

/*
 * ****************************************************
 * 列表件格式
 * 例如ems_ok.conf
 * 192.168.188.150:11079
 * ****************************************************
 */
class FileConfList: public FileConf
{
	private:
	    std::vector< std::string > m_list;

	public:
	    FileConfList( char *filename ): FileConf( filename ){}
	    FileConfList( const std::string &filename ): FileConf( filename ){}
	    virtual ~FileConfList()
	    {
	    	m_list.clear();
	    	std::vector< std::string >().swap( m_list );
	    }

	    virtual void load( int flag = 7 );
	    std::vector< std::string > &getVector(){ return m_list; }
};

}

#endif /* -----  not __FILECONF_H__  ----- */

