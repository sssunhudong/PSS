#ifndef _REAACTORUDPHANDLER_H
#define _REAACTORUDPHANDLER_H

#include "ace/ACE.h"
#include "ace/OS_main.h"
#include "ace/OS_NS_string.h"
#include "ace/Event_Handler.h"
#include "ace/INET_Addr.h"
#include "ace/Reactor.h"
#include "ace/SOCK_Dgram.h"

#include "BaseHander.h"
#include "CommandAccount.h"

class CReactorUDPHander : public ACE_Event_Handler
{
public:
    CReactorUDPHander(void);
    ~CReactorUDPHander(void);

    virtual ACE_HANDLE get_handle(void) const;
    virtual int handle_input(ACE_HANDLE fd = ACE_INVALID_HANDLE);
    virtual int handle_close(ACE_HANDLE handle, ACE_Reactor_Mask close_mask);

    void SetPacketParseInfoID(uint32 u4PacketParseInfoID);                    //设置对应的m_u4PacketParseInfoID
    int  OpenAddress(const ACE_INET_Addr& AddrRemote);
    int  OpenAddress(const ACE_INET_Addr& AddrRemote, ACE_Reactor* pReactor);

    int  Run_Open(ACE_Reactor* pReactor);
    void Close();
    bool SendMessage(char*& pMessage, uint32 u4Len, const char* szIP, int nPort, bool blHead = true, uint16 u2CommandID = 0, bool blDlete = true);
    _ClientConnectInfo GetClientConnectInfo();
    void GetCommandData(uint16 u2CommandID, _CommandData& objCommandData);    //获得指定命令统计信息
    void GetFlowInfo(uint32& u4FlowIn, uint32& u4FlowOut);                    //得到所有的出口流量
    void SetRecvSize(uint32 u4RecvSize);                                      //设置接收数据包最大尺寸
    uint32 GetRecvSize();                                                     //得到数据包最大尺寸

private:
    bool CheckMessage(const char* pData, uint32 u4Len);              //这里解析数据包并放入数据队列
    int  Init_Open_Address(const ACE_INET_Addr& AddrRemote);         //初始化UDP连接对象
    void SaveSendInfo(uint32 u4Len);                                 //记录发送信息

    ACE_SOCK_Dgram          m_skRemote;
    ACE_INET_Addr           m_addrRemote;                   //数据发送方的IP信息
    ACE_INET_Addr           m_addrLocal;                    //监听方的IP信息
    CPacketParse*           m_pPacketParse;                 //数据包解析类
    char*                   m_pRecvBuff;                    //接收数据缓冲指针

    ACE_Time_Value          m_atvInput;                     //接收包的时间
    ACE_Time_Value          m_atvOutput;                    //发送包的时间
    uint32                  m_u4RecvPacketCount;            //接收数据包的数量
    uint32                  m_u4SendPacketCount;            //发送数据包的数量
    uint32                  m_u4RecvSize;                   //接收数据的总大小
    uint32                  m_u4SendSize;                   //发送数据的总大小
    uint32                  m_u4MaxRecvSize;                //最大接收数据包尺寸
    CCommandAccount         m_CommandAccount;               //数据包统计
    uint32                  m_u4PacketParseInfoID;          //对应处理packetParse的模块ID
};
#endif
