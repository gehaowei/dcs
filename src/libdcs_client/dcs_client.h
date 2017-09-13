#ifndef __DCS_CLIENT_H__
#define __DCS_CLIENT_H__

#include <pthread.h>
#include <string>
using namespace std;

namespace DCS {

//�ֲ�ʽ����ϵͳ�ͻ��˿�ӿ�
//������ʹ�÷���ΪDCS::DCSClient::GetInstance()->fun*(*);
class DCSClient
{
public:
    static DCSClient *GetInstance();

    //-------------------------------------------------------------------------------
    // ��������: Open
    // ��������: ��ʼ��������
    // �������: confFile�����ļ�
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int Open(const char *confFile);

    //-------------------------------------------------------------------------------
    // ��������: CacheSet
    // ��������: дcache
    // �������: key��(���ڲ����Զ�base64����), key_len ������
    //      valֵ��val_lenֵ����,  expire�������ʱ��(��λ�룬ȱʡΪ0��ʾ��������)
    //      const int consistency=0 ����һ����Ҫ�� ���Ϊ1��ȫд��ŷ���
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int CacheSet(const char *key, const size_t key_len, const void *val, const size_t val_len, const time_t expire=0, const int consistency=0);

    //-------------------------------------------------------------------------------
    // ��������: CacheGet
    // ��������: ȡcache
    // �������: key��(���ڲ����Զ�base64����), key_len ������
    //      value��������(Ӧ�ô���ָ���ַ���ⴴ���ڴ沢��ֵָ�룬Ӧ���������ͷ��ڴ�(C��ʽ:free))
    //      retlen���ݵĳ���(size_t������Ӧ�ô���������size_t�����ַ�����size_t������и�ֵ)
    //      const int consistency=0 ����һ����Ҫ�� ���Ϊ1��ȫ����ŷ��سɹ�
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int CacheGet(const char *key, const size_t key_len, void **value, size_t *retlen, const int consistency=0);

    //-------------------------------------------------------------------------------
    // ��������: CacheDel
    // ��������: ɾcache
    // �������: key��(���ڲ����Զ�base64����), key_len ������
    //      const int consistency=0 ����һ����Ҫ�� ���Ϊ1��ȫɾ��ŷ��سɹ�
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int CacheDel(const char *key, const size_t key_len, const int consistency=0);

    //-------------------------------------------------------------------------------
    // ��������: Close
    // ��������: �رտͻ��˿�
    // �������: ��
    // �������: ��
    // ����ֵ  : ����0
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
