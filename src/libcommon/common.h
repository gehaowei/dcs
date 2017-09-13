#ifndef __COMMON_H__
#define __COMMON_H__

#include <string>
#include <vector>
using namespace std;
#include <string.h>
#include <stdlib.h>

#define FREE( val ) \
       do{ \
       	   if( val != NULL ) \
		   { \
		       ::free( val ); \
		       val = NULL; \
		   } \
         }while( 0 );

#define MALLOC( val, len, type ) \
      do{ \
      	  assert(( val = ( type * )malloc( len )) != NULL ); \
      	  memset( val, 0x00, len ); \
	  }while( 0 );

namespace DCS
{

//等待(ms)
void TimeWait( int millisecond );

//
string & StringTrim(string &s);

void StringSplit( const string &str, const string &separator, vector< string > &result );

string IntToString( int num );
int stringToInt(const std::string &s);


//按条件拆取字符
//如:传入org="123:abc:xyz", find=":",执行完毕后返回0 head=>"123", tail=>"abc:xyz"
//如:传入org="123:abc:xyz", find="&",执行完毕后返回-1 head=>"123:abc:xyz", tail=>""
int CutStringHead(const string &org, const string &find, string &head, string &tail);

char *EncodeBase64(const char *buf,size_t len);

size_t DecodeBase64(char *buf);


}

#endif
