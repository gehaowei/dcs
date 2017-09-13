
#include "common.h"

#include <sstream>
#include "logger.h"


namespace DCS
{

void TimeWait( int millisecond  )
{
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = millisecond * 1000;

	select( 0, NULL, NULL, NULL, &timeout );
}


string & StringTrim(string &s)
{
    int i=0;
    while (s[i]==' '||s[i]==' ')//开头处为空格或者Tab，则跳过
    {
    i++;
    }
    s=s.substr(i);
    i=s.size()-1;
    while(s[i]==' '||s[i]==' ')////结尾处为空格或者Tab，则跳过
    {
    i--;
    }
    s=s.substr(0,i+1);
    return s;
}

void StringSplit( const string &str, const string &separator, vector< string > &result )
{
    result.clear();

	if( 0 == str.length() || 0 == separator.length() )
	{
		return;
	}

	std::string::size_type begin=0;
	std::string::size_type end=0;


	while( true )
	{
		end = str.find( separator , end );
		if( end == std::string::npos )
		{
			end = str.length();

            std::string oneItem = str.substr( begin, end - begin );

            if(oneItem.length() > 0)
            {
			    result.push_back( oneItem );
			}

            //DCS_LOG(DEBUG, "StringSplit(1)str:" << str << ",separator:" << separator << ",begin:" << begin << ",end:" << end );
			break;
		}
		else
		{
			result.push_back( str.substr( begin, end - begin ));

            //DCS_LOG(DEBUG, "StringSplit(2)str:" << str << ",separator:" << separator << ",begin:" << begin << ",end:" << end );
			end += separator.length();
			begin = end;
		}
	}
	return;
}

string IntToString( int num )
{
	std::stringstream ss;

	ss << num;
	return ss.str();
}

int stringToInt(const std::string &s)
{
	if(s.empty()){
		return 0;
	}
	std::stringstream ss ;
	int i;
	ss << s;
	ss >> i;
	return i;
}


int CutStringHead(const string &org, const string &find, string &head, string &tail)
{
    int retCode = 0;
    string::size_type findPos = org.find(find);
    if(string::npos == findPos)
    {
        retCode = -1;
        head = org;
        tail = "";
    }
    else
    {
        retCode = 0;
        head = org.substr(0, findPos);
        tail = org.substr(findPos+1);
    }

    return retCode;
}


size_t DecodeBase64(char *buf)
{
    size_t  i = 0;
    size_t  k = 0;
    int tmp = 0;
    unsigned char w,x,y,z;
    unsigned char *pos = NULL,a,b,c;

    w=x=y=z=a=b=c=0;


    if (!buf)
        return 0;

    static char decode64tab[256]={0};
    static int tabinit=0;

    if (!tabinit){
        for (i=0; i<64; i++)
            decode64tab[ (int)
                ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i])]=i;
        decode64tab[ (int)'=' ] = 99;
        tabinit = 1;
    }

    pos=(unsigned char *)buf;
    while (*pos)
    {
        while (*pos=='\r' ||*pos=='\n')
            pos++;
        if (!(*pos))
             break;

        w=decode64tab[(int)(unsigned char)(*pos++)];

        if (!(*pos))  break;

		while (*pos=='\r' ||*pos=='\n')
		{
            pos++;
		}

        x=decode64tab[(int)(unsigned char)(*pos++)];

        if (!(*pos))  break;

		while (*pos=='\r' ||*pos=='\n')
		{
            pos++;
		}

        y=decode64tab[(int)(unsigned char)(*pos++)];
        if (!(*pos))  break;

		while (*pos=='\r' ||*pos=='\n')
		{
            pos++;
		}

        z=decode64tab[(int)(unsigned char)(*pos++)];

        buf[k++]=(w << 2) | (x >> 4);
        if (*(pos-2) != '='){
            buf[k++]=(x << 4) | (y >> 2);
            tmp = 0;
            if (*(pos-1)  != '='){
                buf[k++]= (y << 6) | z;
                tmp = 0;
            }
            else
                 tmp = 1;
        } else
            tmp = 1;

        if (tmp)
            break;
    }
    buf[k]=0;
    return (k);
}

//---------------------------------------------------------------------------
char *EncodeBase64(const char *buf,size_t len)
{
    static const unsigned char base64tab[]=
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char *output_buf=NULL;
    unsigned char a=0,b=0,c=0;
    size_t  i=0, j=0;
    unsigned char d, e, f, g;

    d=e=f=g=0;

    if (!buf || len<1)
        return NULL;

    i=(len / 3 + (len % 3 >0 ? 1:0)) * 4+1;
    output_buf=(char *)malloc(i);

    if (!output_buf){
        //syslog("malloc err");
        return NULL;
    }

    for (j=i=0; i<len; i += 3) {
        a=buf[i];

        b= i+1 < len ? buf[i+1]:0;
        c= i+2 < len ? buf[i+2]:0;

        d=base64tab[ a >> 2 ];
        e=base64tab[ ((a & 3 ) << 4) | (b >> 4)];
        f=base64tab[ ((b & 15) << 2) | (c >> 6)];
        g=base64tab[ c & 63 ];

        if (i + 1 >= len) f='=';
        if (i + 2 >= len) g='=';

        output_buf[j++]=d;
        output_buf[j++]=e;
        output_buf[j++]=f;
        output_buf[j++]=g;
    }

    output_buf[j++]='\0';

    return (output_buf);
}


}

