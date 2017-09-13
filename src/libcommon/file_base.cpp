/*
 * =====================================================================================
 *
 *       Filename:  file_base.cpp
 *
 *    Description:  file_base realize
 *
 *        Version:  1.0
 *        Created:  11/19/09 10:29:12
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  killam-zhiyong.zeng (TM), zhiyong.zeng@net263.com
 *        Company:  263
 *
 * =====================================================================================
 */


#include "file_base.h"
#include "common.h"

#define LIB_FILE_OPEN_ERROR -1
#define LIB_FILE_LOCK_ERROR -2

namespace DCS
{

int FileBase::overwrite( const std::string &content )
{
	int i = 0;
	FILE *fp = NULL;

	if( ( fp = fopen( m_filename.c_str(), "r+" ) ) == NULL )
	{
		return LIB_FILE_OPEN_ERROR;
	}

	while( flock( fileno( fp ), LOCK_EX ) != 0 )
	{
		if( ++ i > 5 )
		{
			return LIB_FILE_LOCK_ERROR;
		}
		sleep( 1 );
	}

	fseek( fp, 0L, SEEK_SET );
	fwrite( content.c_str(), content.length(), 1, fp );
	fseek( fp, 0L, SEEK_CUR );
	ftruncate( fileno( fp ), content.length() );
	fflush( fp );

	flock( fileno( fp ), LOCK_UN );
	fclose( fp );

	return 0;
}

char *FileBase::getContent( char **content )
{
	FILE *fp = fopen( m_filename.c_str(), "r+" );
	int i = 0;

	if( fp == NULL )
	{
		return *content; //file open error!
	}

	while( flock( fileno( fp ), LOCK_EX ) != 0 )
	{
		if( ++ i > 5 )
		{
			return *content;
		}
		sleep( 1 );
	}

	struct stat fsbuf = { 0 };
	int fd = fileno( fp );
	char *tmp = NULL;

	if( fstat( fd, &fsbuf ) == -1 )
	{
		flock( fileno( fp ), LOCK_UN );
		fclose( fp );
		return *content; //file stat error!
	}

	if(( tmp = ( char * )malloc( sizeof( char ) * ( fsbuf.st_size + 1 ))) == NULL )
	{
		flock( fileno( fp ), LOCK_UN );
		fclose( fp );
		return *content;
	}
	memset( tmp, 0x00, sizeof( char ) * ( fsbuf.st_size + 1 ) );

	fread( tmp, fsbuf.st_size, 1, fp );

	*content = tmp;

	flock( fileno( fp ), LOCK_UN );
	fclose( fp );

	return *content;
}

std::string &FileBase::getContent( std::string &content )
{
	FILE *fp = fopen( m_filename.c_str(), "r+" );
	int i = 0;

	if( fp == NULL )
	{
		return content; //file open error!
	}

	struct stat fsbuf = { 0 };
	int fd = fileno( fp );
	char *tmp = NULL;

	while( flock( fileno( fp ), LOCK_EX ) != 0 )
	{
		if( ++ i > 5 )
		{
			return content;
		}
		sleep( 1 );
	}

	if( fstat( fd, &fsbuf ) == -1 )
	{
		flock( fileno( fp ), LOCK_UN );
		fclose( fp );
		return content; //file stat error!
	}

	if(( tmp = ( char * )malloc( sizeof( char ) * ( fsbuf.st_size + 1 ))) == NULL )
	{
		flock( fileno( fp ), LOCK_UN );
		fclose( fp );
		return content;
	}
	memset( tmp, 0x00, sizeof( char ) * ( fsbuf.st_size + 1 ) );

	fread( tmp, fsbuf.st_size, 1, fp );

	content = tmp;
	FREE( tmp );

	flock( fileno( fp ), LOCK_UN );
	fclose( fp );

	return content;
}		/* -----  end of method FileBase::getContent  ----- */

}

