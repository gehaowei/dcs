/*
 * =====================================================================================
 *
 *       Filename:  file_conf.cpp
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  02/24/10 09:46:26
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  killam-zhiyong.zeng (TM), zhiyong.zeng@net263.com
 *        Company:  263
 *
 * =====================================================================================
 */

#include "file_conf.h"
#include "common.h"
#include <iostream>

namespace DCS
{

void FileConfIni::load( int flag )
{
	std::string content;

	this->getContent( content );
	this->filter( content, flag );

	std::string::size_type begin = 0;
	std::string::size_type middle = 0;
	std::string::size_type end = 0;
	std::string::size_type nextbegin = 0;
	std::string curtitle;
	std::string subcontent;

    while( 1 )
	{
	    begin = content.find( "[", begin );
	    end = content.find( "]", begin );
	    if( begin == std::string::npos || end == std::string::npos )
		{ //匹配方括号有问题
		    break;
		}

		curtitle = content.substr( begin + 1, end - begin - 1 );

		//获取中间数据块
		if( ( nextbegin = content.find( "[", end ) ) == std::string::npos )
		{
			subcontent = content.substr( end + 1, content.length() - end - 1 );
		}
		else
		{
			subcontent = content.substr( end + 1, nextbegin - end - 1 );
		}

	    std::string::size_type content_begin = 0;
	    std::string::size_type content_middle = 0;
	    std::string::size_type content_end = 0;
		while( ( content_end = subcontent.find( "\n", content_begin ) ) != std::string::npos )
		{
			content_middle = subcontent.find( "=", content_begin );

			//查询相关的key=value
			if( content_middle > 0 && content_middle < content_end )
			{
				this->setValue( curtitle + "." + subcontent.substr( content_begin, content_middle - content_begin ),
				                subcontent.substr( content_middle + 1, content_end - content_middle - 1 ));
				this->setValue( curtitle, subcontent.substr( content_begin, content_middle - content_begin ),
				                subcontent.substr( content_middle + 1, content_end - content_middle - 1 ));
			}
			content_begin = ++content_end;
		}

		if( subcontent.length() > content_begin )
		{
			content_middle = subcontent.find( "=", content_begin );
			if( content_middle > 0 && content_middle < subcontent.length() )
			{
				this->setValue( curtitle + "." + subcontent.substr( content_begin, content_middle - content_begin ),
				                subcontent.substr( content_middle + 1, subcontent.length() - content_middle - 1 ));
				this->setValue( curtitle, subcontent.substr( content_begin, content_middle - content_begin ),
				                subcontent.substr( content_middle + 1, subcontent.length() - content_middle - 1 ));
			}
		}
		if(( begin = nextbegin ) == std::string::npos )
		{
			break;
		}
	}
}

void FileConf::free()
{
	ini_profile_t *node = NULL;

	//node = ( m_conflist != NULL? m_conflist->next: NULL );

	while( this->m_conflist != NULL )
	{
		node = this->m_conflist->next;
		FREE( this->m_conflist );
		this->m_conflist = node;
	}

	//std::cout << "free OK" << std::endl;
}

std::string &FileConf::filter( std::string &content, int flag )
{
	//#到结尾删除，//到结尾删除，/**/中间删除
	std::string::size_type pos = 0;
	std::string::size_type end = 0;

    if( ( flag & 0x0001 ) != 0x0000 )
	{
		while( ( pos = content.find( "#", pos ) ) != std::string::npos )
		{
			end = content.find( "\n", pos );
			if( end == std::string::npos )
			{
				content.replace( pos, content.length() - pos + 1, "" );
			}
			else
			{
				if( pos == 0 || content[ pos - 1 ] == '\n' )
				{
					content.replace( pos, end - pos + 1, "" );
				}
				else
				{
					content.replace( pos, end - pos, "" );
				}
			}
			//pos ++;
		}
	}

    if( ( flag & 0x0010 ) != 0x0000 )
	{
		pos = 0;
		while( ( pos = content.find( "//", pos ) ) != std::string::npos )
		{
			end = content.find( "\n", pos );
			if( end == std::string::npos )
			{
				content.replace( pos, content.length() - pos + 1, "" );
			}
			else
			{
				if( pos == 0 || content[ pos - 1 ] == '\n' )
				{
					content.replace( pos, end - pos + 1, "" );
				}
				else
				{
					content.replace( pos, end - pos, "" );
				}
			}
			//pos ++;
		}
	}

    if( ( flag & 0x0100 ) != 0x0000 )
	{
		pos = 0;
		while( ( pos = content.find( "/*", pos ) ) != std::string::npos )
		{
			end = content.find( "*/", pos );
			if( end == std::string::npos )
			{
				content.replace( pos, content.length() - pos + 2, "" );
			}
			else
			{
				content.replace( pos, end - pos + 2, "" );
			}
			//pos += 2;
		}
	}

	return content;
}

void FileConf::load( int flag )
{
	std::string content;

	this->getContent( content );
	this->filter( content, flag );

	std::string::size_type begin = 0; //配置项开始位置
	std::string::size_type end = 0;   //配置项结束位置
	std::string::size_type middle = 0;//中间=位置

	while( ( end = content.find( "\n", begin ) ) != std::string::npos )
	{
		middle = content.find( "=", begin );

		//查询相关的key=value
		if( middle > 0 && middle < end )
		{
			this->setValue( content.substr( begin, middle - begin ), content.substr( middle + 1, end - middle - 1 ));
		}
		begin = ++end;
	}

	if( content.length() > begin )
	{
		middle = content.find( "=", begin );
		if( middle > 0 && middle < content.length() )
		{
			this->setValue( content.substr( begin, middle - begin ), content.substr( middle + 1, content.length() - middle - 1 ));
		}
	}
}

std::map< std::string, std::string > &FileConf::getMap()
{
	return m_conf;
}

std::string &FileConf::getValue( const std::string &key )
{
	//std::map< std::string, std::string > p = m_conf.find( key );

	//return ( p == m_conf.end()? std::string( "" ): p->second );
	return m_conf[ key ];
}

void FileConf::setValue( const std::string &key, const std::string &value )
{
	m_conf[ key ] = value;
}

ini_profile_t *FileConf::getList( const std::string &section )
{
	ini_profile_t *node = m_conflist;

	while( node != NULL )
	{
		if( section.compare( node->section ) == 0 )
		{
			return node;
		}
		node = node->next;
	}

	return NULL;
}

ini_profile_t *FileConf::getList()
{
	return m_conflist;
}

char *FileConf::getData( const std::string &key )
{
	ini_profile_t *node = m_conflist;

	while( node )
	{
		if( key.compare( node->key ) == 0 )
		{
			return node->value;
		}
		node = node->next;
	}
	return NULL;
}

char *FileConf::getData( const std::string &section, const std::string &key )
{
	ini_profile_t *node = m_conflist;

	while( node )
	{
		if( section.compare( node->section ) == 0 && key.compare( node->key ) == 0 )
		{
			return node->value;
		}
		node = node->next;
	}
	return NULL;
}
int FileConf::getIntData( const std::string &section, const std::string &key )
{
	ini_profile_t *node = m_conflist;

	while( node )
	{
		if( section.compare( node->section ) == 0 && key.compare( node->key ) == 0 )
		{
			return atoi( node->value );
		}
		node = node->next;
	}

	return -1;
}

void FileConf::setValue( const std::string &section, const std::string &key, const std::string &value )
{
	if( key.empty() )
	{
		return;
	}

	ini_profile_t *node = NULL;

	MALLOC( node, sizeof( ini_profile_t ), ini_profile_t );

	strncpy( node->key, key.c_str(), INI_PROFILE_KEY_LEN );
	if( !value.empty() )
	{
		strncpy( node->value, value.c_str(), INI_PROFILE_VALUE_LEN );
	}

	if( !section.empty() )
	{ //有section需要分配section空间
		strncpy( node->section, section.c_str(), INI_PROFILE_SECTION_LEN );
	}

	if( m_conflist == NULL )
	{
		m_conflist = node;
	}
	else
	{
		//ini_profile_t *sec = m_conflist->next;
		node->next = m_conflist->next;
		m_conflist->next = node;
	}
}

void FileConfList::load( int flag )
{
	std::string content;

	this->getContent( content );
	this->filter( content, flag );

	//std::cout << "content=" << content << std::endl;

	std::string tmp; //保存每一行数据
	std::string::size_type pos = 0;
	std::string::size_type begin = 0;

	while( 1 )
	{
		//一行行读取数据
		if( ( pos = content.find( '\n', pos ) ) != std::string::npos )
		{
			if( begin < pos )
			{
				tmp = content.substr( begin, pos - begin );
				StringTrim( tmp );
				if( !tmp.empty() )
				{
					m_list.push_back( tmp );
				}
			}
		}
		else
		{
			if( begin < content.length() )
			{
				tmp = content.substr( begin );
				StringTrim( tmp );
				if( !tmp.empty() )
				{
					m_list.push_back( tmp );
				}
			}
			break;
		}
		begin = ++ pos;
	}
}

}

