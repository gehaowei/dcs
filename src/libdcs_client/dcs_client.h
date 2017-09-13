#ifndef __DCS_CLIENT_H__
#define __DCS_CLIENT_H__

#include <pthread.h>
#include <string>
using namespace std;

namespace DCS {

//分布式缓存系统客户端库接口
//单件类使用方法为DCS::DCSClient::GetInstance()->fun*(*);
class DCSClient
{
public:
    static DCSClient *GetInstance();

    //-------------------------------------------------------------------------------
    // 函数名称: Open
    // 函数功能: 初始化并启动
    // 输入参数: confFile配置文件
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int Open(const char *confFile);

    //-------------------------------------------------------------------------------
    // 函数名称: CacheSet
    // 函数功能: 写cache
    // 输入参数: key键(库内部会自动base64编码), key_len 键长度
    //      val值，val_len值长度,  expire缓存过期时间(单位秒，缺省为0表示永不过期)
    //      const int consistency=0 数据一致性要求 如果为1则全写完才返回
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int CacheSet(const char *key, const size_t key_len, const void *val, const size_t val_len, const time_t expire=0, const int consistency=0);

    //-------------------------------------------------------------------------------
    // 函数名称: CacheGet
    // 函数功能: 取cache
    // 输入参数: key键(库内部会自动base64编码), key_len 键长度
    //      value返回数据(应用传入指针地址，库创建内存并赋值指针，应用需自行释放内存(C方式:free))
    //      retlen数据的长度(size_t对象由应用创建，传入size_t对象地址，库对size_t对象进行赋值)
    //      const int consistency=0 数据一致性要求 如果为1则全读完才返回成功
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int CacheGet(const char *key, const size_t key_len, void **value, size_t *retlen, const int consistency=0);

    //-------------------------------------------------------------------------------
    // 函数名称: CacheDel
    // 函数功能: 删cache
    // 输入参数: key键(库内部会自动base64编码), key_len 键长度
    //      const int consistency=0 数据一致性要求 如果为1则全删完才返回成功
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int CacheDel(const char *key, const size_t key_len, const int consistency=0);

    //-------------------------------------------------------------------------------
    // 函数名称: Close
    // 函数功能: 关闭客户端库
    // 输入参数: 无
    // 输出参数: 无
    // 返回值  : 返回0
    //-------------------------------------------------------------------------------
    int Close();

    string GetMGInfo(const char *key, const size_t key_len);

public:
    ~DCSClient();

protected:
    DCSClient();

private:
    string m_module_name;
	static DCSClient* m_Instance;
	static pthread_mutex_t m_Mutex;

};

}



#endif
