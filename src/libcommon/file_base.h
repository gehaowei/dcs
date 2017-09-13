/*
 * =====================================================================================
 *
 *       Filename:  file_base.h
 *
 *    Description:  �ļ���������
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
 *  Description:  �ļ���������
 *  1 ���ļ�
 *  2 �ر��ļ�
 *  3 ��ȡȫ������
 *  4 ��ȡһ��
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
		virtual std::string &getContent( std::string &content ); /* ��ȡ�ļ����� */
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
