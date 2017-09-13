/*
 * =====================================================================================
 *
 *       Filename:  file_base.h
 *
 *    Description:  文件操作基类
 *
 *        Version:  1.0
 *        Created:  11/19/09 10:15:24
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  killam-zhiyong.zeng (TM), zhiyong.zeng@net263.com
 *        Company:  263
 *
 * =====================================================================================
 */

#ifndef __FILE_BASE_H__

#define	__FILE_BASE_H__ /*  */

#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/file.h>
#include <assert.h>

namespace DCS
{

/*
 * =====================================================================================
 *        Class:  FileBase
 *  Description:  文件基本操作
 *  1 打开文件
 *  2 关闭文件
 *  3 读取全部数据
 *  4 读取一行
 * =====================================================================================
 */
class FileBase
{
	public:
		/* ====================  LIFECYCLE     ======================================= */
		FileBase(){}                             /* constructor */
		FileBase( char *filename )
		{
			assert( filename != NULL );
			m_filename = filename;
		}
		FileBase( const std::string &filename )
		{
			assert( !filename.empty() );
			m_filename = filename;
		}
		virtual ~FileBase(){}

		/* ====================  ACCESSORS     ======================================= */

		/* ====================  MUTATORS      ======================================= */
		virtual std::string &getContent( std::string &content ); /* 获取文件内容 */
		virtual char *getContent( char **content );
		virtual int overwrite( const std::string &content );

		/* ====================  OPERATORS     ======================================= */

		/* ====================  DATA MEMBERS  ======================================= */
	protected:

	private:
	    std::string m_filename;

}; /* -----  end of class FileBase  ----- */

}

#endif /* -----  not __FILEBASE_H__  ----- */
