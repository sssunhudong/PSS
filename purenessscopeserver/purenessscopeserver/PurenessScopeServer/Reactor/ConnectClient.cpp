#include "ConnectClient.h"
#include "ClientReConnectManager.h"

CConnectClient::CConnectClient(void)
{
    m_pCurrMessage      = NULL;
    m_nIOCount          = 1;
    m_nServerID         = 0;
    m_u4SendSize        = 0;
    m_u4SendCount       = 0;
    m_u4RecvSize        = 0;
    m_u4RecvCount       = 0;
    m_u4CostTime        = 0;
    m_u4MaxPacketSize   = MAX_MSG_PACKETLENGTH;
    m_ems2s             = S2S_NEED_CALLBACK;
    m_szError[0]        = '\0';
    m_pClientMessage    = NULL;
    m_u4CurrSize        = 0;
    m_u1ConnectState    = 0;

    m_emRecvState       = SERVER_RECV_INIT;
}

CConnectClient::~CConnectClient(void)
{
}

void CConnectClient::Close()
{
    m_ThreadLock.acquire();

    if (m_nIOCount > 0)
    {
        m_nIOCount--;
    }

    m_ThreadLock.release();

    //从反应器注销事件
    if (m_nIOCount == 0)
    {
        shutdown();
        OUR_DEBUG((LM_ERROR, "[CConnectClient::Close]Close(%s:%d) OK.\n", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number()));

        //删除链接对象
        App_ClientReConnectManager::instance()->CloseByClient(m_nServerID);

        //回归用过的指针
        delete this;
    }

}

void CConnectClient::ClientClose()
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_ThreadLock);
    //将连接消息断开放在output去执行，这样就不需要同步加锁了。
    ACE_Message_Block* pMbData = App_MessageBlockManager::instance()->Create(sizeof(int));

    if (NULL == pMbData)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::ClientClose] Connectid=%d, pMbData is NULL.\n", GetServerID()));
        return;
    }

    ACE_Message_Block::ACE_Message_Type objType = ACE_Message_Block::MB_STOP;
    pMbData->msg_type(objType);

    //这里主动关闭，不在要求回调连接断开消息
    SetClientMessage(NULL);

    //将消息放入队列，让output在反应器线程发送。
    ACE_Time_Value xtime = ACE_OS::gettimeofday();

    //队列已满，不能再放进去了,就不放进去了
    if (msg_queue()->is_full() == true)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::ClientClose] Connectid=%d,putq is full(%d).\n", GetServerID(), msg_queue()->message_count()));
        App_MessageBlockManager::instance()->Close(pMbData);
        return;
    }

    if (this->putq(pMbData, &xtime) == -1)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::ClientClose] Connectid=%d,putq(%d) output errno = [%d].\n", GetServerID(), msg_queue()->message_count(), errno));
        App_MessageBlockManager::instance()->Close(pMbData);
    }
    else
    {
        m_u1ConnectState = CONNECT_CLIENT_CLOSE;
        int nWakeupRet = reactor()->schedule_wakeup(this, ACE_Event_Handler::WRITE_MASK);

        if (-1 == nWakeupRet)
        {
            OUR_DEBUG((LM_ERROR, "CConnectClient::ClientClose] Connectid=%d, nWakeupRet(%d) output errno = [%d].\n", GetServerID(), nWakeupRet, errno));
        }
    }
}

int CConnectClient::open(void* p)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_ThreadLock);

    msg_queue()->high_water_mark(MAX_MSG_MASK);
    msg_queue()->low_water_mark(MAX_MSG_MASK);

    //从配置文件获取数据
    m_u4MaxPacketSize  = GetXmlConfigAttribute(xmlRecvInfo)->RecvBuffSize;

    if (p != NULL)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::open]p is not NULL.\n"));
    }

    ACE_Time_Value nowait(MAX_MSG_PACKETTIMEOUT);
    m_nIOCount = 1;

    //设置链接为非阻塞模式
    if (this->peer().enable(ACE_NONBLOCK) == -1)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]this->peer().enable  = ACE_NONBLOCK error.\n"));
        sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectClient::open]this->peer().enable  = ACE_NONBLOCK error.");
        return -1;
    }

    //获得远程链接地址和端口
    if (this->peer().get_remote_addr(m_addrRemote) == -1)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectHandler::open]this->peer().get_remote_addr error.\n"));
        sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectClient::open]this->peer().get_remote_addr error.");
        return -1;
    }

    m_u4SendSize        = 0;
    m_u4SendCount       = 0;
    m_u4RecvSize        = 0;
    m_u4RecvCount       = 0;
    m_u4CostTime        = 0;
    m_atvBegin          = ACE_OS::gettimeofday();
    m_u4CurrSize        = 0;
    //申请当前的MessageBlock
    m_pCurrMessage = App_MessageBlockManager::instance()->Create(GetXmlConfigAttribute(xmlConnectServer)->Recvbuff);

    if (m_pCurrMessage == NULL)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::RecvClinetPacket] pmb new is NULL.\n"));
        return -1;
    }

    m_u1ConnectState = CONNECT_OPEN;

    App_ClientReConnectManager::instance()->SetHandler(m_nServerID, this);
    m_pClientMessage = App_ClientReConnectManager::instance()->GetClientMessage(m_nServerID);
    OUR_DEBUG((LM_INFO, "[CConnectClient::open] Connection from [%s:%d]\n", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number()));

    int nRet = this->reactor()->register_handler(this, ACE_Event_Handler::READ_MASK | ACE_Event_Handler::WRITE_MASK);

    int nWakeupRet = reactor()->cancel_wakeup(this, ACE_Event_Handler::WRITE_MASK);

    if (-1 == nWakeupRet)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::open]ConnectID=%d, nWakeupRet=%d, errno=%d.\n", GetServerID(), nWakeupRet, errno));
    }

    return nRet;
}

int CConnectClient::handle_input(ACE_HANDLE fd)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_ThreadLock);

    if (fd == ACE_INVALID_HANDLE)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::handle_input]fd == ACE_INVALID_HANDLE.\n"));
        sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectHandler::handle_input]fd == ACE_INVALID_HANDLE.");

        if (NULL != m_pClientMessage)
        {
            _ClientIPInfo objServerIPInfo;
            sprintf_safe(objServerIPInfo.m_szClientIP, MAX_BUFF_20, "%s", m_addrRemote.get_host_addr());
            objServerIPInfo.m_nPort = m_addrRemote.get_port_number();

            if(S2S_NEED_CALLBACK == m_ems2s)
            {
                m_pClientMessage->ConnectError((int)ACE_OS::last_error(), objServerIPInfo);
            }
        }

        return -1;
    }

    //判断缓冲是否为NULL
    if (m_pCurrMessage == NULL)
    {
        m_u4CurrSize = 0;
        OUR_DEBUG((LM_ERROR, "[CConnectClient::handle_input]m_pCurrMessage == NULL.\n"));
        sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectClient::handle_input]m_pCurrMessage == NULL.");

        if (NULL != m_pClientMessage)
        {
            _ClientIPInfo objServerIPInfo;
            sprintf_safe(objServerIPInfo.m_szClientIP, MAX_BUFF_20, "%s", m_addrRemote.get_host_addr());
            objServerIPInfo.m_nPort = m_addrRemote.get_port_number();

            if(S2S_NEED_CALLBACK == m_ems2s)
            {
                m_pClientMessage->ConnectError((int)ACE_OS::last_error(), objServerIPInfo);
            }
        }

        return -1;
    }

    return RecvData();
}

int CConnectClient::RecvData()
{
    ACE_Time_Value nowait(MAX_MSG_PACKETTIMEOUT);

    int nCurrCount = (uint32)m_pCurrMessage->size();

    if (nCurrCount < 0)
    {
        //如果剩余字节为负，说明程序出了问题
        OUR_DEBUG((LM_ERROR, "[CConnectClient::handle_input][%d] nCurrCount < 0 m_u4CurrSize = %d.\n", GetServerID(), m_u4CurrSize));
        m_u4CurrSize = 0;

        _ClientIPInfo objServerIPInfo;
        sprintf_safe(objServerIPInfo.m_szClientIP, MAX_BUFF_20, "%s", m_addrRemote.get_host_addr());
        objServerIPInfo.m_nPort = m_addrRemote.get_port_number();

        if(S2S_NEED_CALLBACK == m_ems2s)
        {
            m_pClientMessage->ConnectError((int)ACE_OS::last_error(), objServerIPInfo);
        }

        return -1;
    }

    int nDataLen = (int)this->peer().recv(m_pCurrMessage->wr_ptr(), nCurrCount, MSG_NOSIGNAL, &nowait);

    if (nDataLen <= 0)
    {
        m_u4CurrSize = 0;
        uint32 u4Error = (uint32)errno;
        OUR_DEBUG((LM_ERROR, "[CConnectClient::handle_input] ConnectID = %d, recv data is error nDataLen = [%d] errno = [%d].\n", GetServerID(), nDataLen, u4Error));
        sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectClient::handle_input] ConnectID = %d, recv data is error[%d].\n", GetServerID(), nDataLen);

        _ClientIPInfo objServerIPInfo;
        sprintf_safe(objServerIPInfo.m_szClientIP, MAX_BUFF_20, "%s", m_addrRemote.get_host_addr());
        objServerIPInfo.m_nPort = m_addrRemote.get_port_number();

        if(S2S_NEED_CALLBACK == m_ems2s)
        {
            m_pClientMessage->ConnectError((int)ACE_OS::last_error(), objServerIPInfo);
        }

        return -1;
    }

    //如果是DEBUG状态，记录当前接受包的二进制数据
    Output_Debug_Data(m_pCurrMessage, LOG_SYSTEM_DEBUG_SERVERRECV);

    m_pCurrMessage->wr_ptr(nDataLen);

    Dispose_Recv_Data(m_pCurrMessage);

    m_emRecvState = SERVER_RECV_END;

    if (m_pCurrMessage->length() > 0)
    {
        //将缓冲没处理的数据，放入下次缓冲
        ACE_Message_Block* pmbSave = App_MessageBlockManager::instance()->Create((uint32)m_pCurrMessage->length());

        if (NULL != pmbSave)
        {
            memcpy_safe(pmbSave->wr_ptr(), (uint32)m_pCurrMessage->length(), m_pCurrMessage->rd_ptr(), (uint32)m_pCurrMessage->length());
            pmbSave->wr_ptr(m_pCurrMessage->length());
            m_pCurrMessage->reset();
            memcpy_safe(m_pCurrMessage->wr_ptr(), (uint32)pmbSave->length(), pmbSave->rd_ptr(), (uint32)pmbSave->length());
            m_pCurrMessage->wr_ptr(pmbSave->length());
            App_MessageBlockManager::instance()->Close(pmbSave);
        }
        else
        {
            m_pCurrMessage->reset();
        }
    }
    else
    {
        m_pCurrMessage->reset();
    }


    return 0;
}

int CConnectClient::SendMessageGroup(uint16 u2CommandID, ACE_Message_Block* pmblk)
{
    //组织数据
    return Make_Common_Dispose_Client_WorkTread_Message(u2CommandID, (uint32)GetServerID(), pmblk, m_addrRemote);
}

int CConnectClient::Dispose_Recv_Data(ACE_Message_Block* pCurrMessage)
{
    //接收数据，返回给逻辑层，自己不处理整包完整性判定
    _ClientIPInfo objServerIPInfo;
    sprintf_safe(objServerIPInfo.m_szClientIP, MAX_BUFF_20, "%s", m_addrRemote.get_host_addr());
    objServerIPInfo.m_nPort = m_addrRemote.get_port_number();

    uint16 u2CommandID = 0;
    ACE_Message_Block* pRecvFinish = NULL;

    m_atvRecv = ACE_OS::gettimeofday();
    m_emRecvState = SERVER_RECV_BEGIN;
    EM_PACKET_ROUTE em_PacketRoute = PACKET_ROUTE_SELF;

    while (true)
    {
        bool blRet = m_pClientMessage->Recv_Format_data(pCurrMessage, App_MessageBlockManager::instance(), u2CommandID, pRecvFinish, em_PacketRoute);

        if (false == blRet)
        {
            break;
        }
        else
        {
            if (PACKET_ROUTE_SELF == em_PacketRoute)
            {
                //直接调用插件内注册的对象处理数据
                Recv_Common_Dispose_Client_Message(u2CommandID, pRecvFinish, objServerIPInfo, m_pClientMessage);
            }
            else
            {
                //将数据放回到消息线程
                SendMessageGroup(u2CommandID, pRecvFinish);
            }
        }
    }

    return 0;
}

void CConnectClient::Output_Debug_Data(ACE_Message_Block* pMbData, int nLogType)
{
    char szPacketDebugData[MAX_BUFF_1024] = { '\0' };

    if (GetXmlConfigAttribute(xmlServerType)->Debug == DEBUG_ON)
    {
        int nDataLen = (int)pMbData->length();
        char szLog[10] = { '\0' };
        uint32 u4DebugSize = 0;
        bool blblMore = false;

        if ((uint32)nDataLen >= MAX_BUFF_200)
        {
            u4DebugSize = MAX_BUFF_200 - 1;
            blblMore = true;
        }
        else
        {
            u4DebugSize = (uint32)nDataLen;
        }

        char* pData = pMbData->rd_ptr();

        for (uint32 i = 0; i < u4DebugSize; i++)
        {
            sprintf_safe(szLog, 10, "0x%02X ", (unsigned char)pData[i]);
            sprintf_safe(szPacketDebugData + 5 * i, MAX_BUFF_1024 - 5 * i, "%s", szLog);
        }

        szPacketDebugData[5 * u4DebugSize] = '\0';

        if (blblMore == true)
        {
            AppLogManager::instance()->WriteLog(nLogType, "[%s:%d]%s.(数据包过长只记录前200字节)", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szPacketDebugData);
        }
        else
        {
            AppLogManager::instance()->WriteLog(nLogType, "[%s:%d]%s.", m_addrRemote.get_host_addr(), m_addrRemote.get_port_number(), szPacketDebugData);
        }
    }
}

int CConnectClient::handle_close(ACE_HANDLE h, ACE_Reactor_Mask mask)
{
    if (h == ACE_INVALID_HANDLE)
    {
        OUR_DEBUG((LM_DEBUG, "[CConnectClient::handle_close] h is NULL mask=%d.\n", GetServerID(), (int)mask));
    }

    Close();
    return 0;
}

int CConnectClient::handle_output(ACE_HANDLE fd /*= ACE_INVALID_HANDLE*/)
{
    if (fd == ACE_INVALID_HANDLE)
    {
        m_u4CurrSize = 0;
        OUR_DEBUG((LM_ERROR, "[CConnectClient::handle_output]fd == ACE_INVALID_HANDLE.\n"));
        return -1;
    }

    ACE_Message_Block* pmbSendData = NULL;
    ACE_Time_Value nowait(ACE_OS::gettimeofday());

    while (-1 != this->getq(pmbSendData, &nowait))
    {
        if (NULL == pmbSendData)
        {
            OUR_DEBUG((LM_INFO, "[CConnectClient::handle_output]ConnectID=%d pmbSendData is NULL.\n", GetServerID()));
            break;
        }

        //如果是断开指令，则执行连接断开操作
        if (pmbSendData->msg_type() == ACE_Message_Block::MB_STOP)
        {
            return -1;
        }

        //发送数据
        char* pData = pmbSendData->rd_ptr();

        if (NULL == pData)
        {
            OUR_DEBUG((LM_ERROR, "[CConnectClient::SendData] ConnectID = %d, pData is NULL.\n", GetServerID()));
            App_MessageBlockManager::instance()->Close(pmbSendData);
            return -1;
        }

        int nSendLen = (int)pmbSendData->length();   //发送数据的总长度
        int nIsSendSize = 0;

        //循环发送，直到数据发送完成。
        while (true)
        {
            if (nSendLen <= 0)
            {
                OUR_DEBUG((LM_ERROR, "[CConnectClient::SendData] ConnectID = %d, nCurrSendSize error is %d.\n", GetServerID(), nSendLen));
                App_MessageBlockManager::instance()->Close(pmbSendData);
                return -1;
            }

            int nDataLen = (int)this->peer().send(pmbSendData->rd_ptr(), nSendLen - nIsSendSize, &nowait);

            if (nDataLen <= 0)
            {
                _ClientIPInfo objServerIPInfo;
                sprintf_safe(objServerIPInfo.m_szClientIP, MAX_BUFF_20, "%s", m_addrRemote.get_host_addr());
                objServerIPInfo.m_nPort = m_addrRemote.get_port_number();
                m_pClientMessage->ConnectError((int)ACE_OS::last_error(), objServerIPInfo);

                OUR_DEBUG((LM_ERROR, "[CConnectClient::SendData] ConnectID = %d, error = %d.\n", GetServerID(), errno));
                App_MessageBlockManager::instance()->Close(pmbSendData);
                return -1;
            }
            else if (nDataLen + nIsSendSize >= nSendLen)  //当数据包全部发送完毕，清空。
            {
                App_MessageBlockManager::instance()->Close(pmbSendData);
                m_u4SendSize += (uint32)nSendLen;
                m_u4SendCount++;
                break;
            }
            else
            {
                pmbSendData->rd_ptr(nDataLen);
                nIsSendSize += nDataLen;
            }
        }

    }

    int nWakeupRet = reactor()->cancel_wakeup(this, ACE_Event_Handler::WRITE_MASK);

    if (-1 == nWakeupRet)
    {
        OUR_DEBUG((LM_INFO, "[CConnectClient::handle_output]ConnectID=%d,nWakeupRet=%d, errno=%d.\n", GetServerID(), nWakeupRet, errno));
    }

    return 0;
}

void CConnectClient::SetClientMessage(IClientMessage* pClientMessage)
{
    m_pClientMessage = pClientMessage;
}

void CConnectClient::SetServerID(int nServerID)
{
    m_nServerID = nServerID;
}

int CConnectClient::GetServerID()
{
    return m_nServerID;
}

bool CConnectClient::SendData(ACE_Message_Block* pmblk)
{
    ACE_Guard<ACE_Recursive_Thread_Mutex> guard(m_ThreadLock);

    if (CONNECT_CLIENT_CLOSE == m_u1ConnectState || CONNECT_SERVER_CLOSE == m_u1ConnectState)
    {
        //连接已经进入关闭流程，不在接受发送数据。
        App_MessageBlockManager::instance()->Close(pmblk);
        return false;
    }

    if (NULL == pmblk)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::SendData] ConnectID = %d, get_handle() == ACE_INVALID_HANDLE.\n", GetServerID()));
        return false;
    }

    //如果是DEBUG状态，记录当前接受包的二进制数据
    Output_Debug_Data(pmblk, LOG_SYSTEM_DEBUG_CLIENTSEND);

    ACE_Time_Value     nowait(MAX_MSG_PACKETTIMEOUT);

    if (get_handle() == ACE_INVALID_HANDLE)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::SendData] ConnectID = %d, get_handle() == ACE_INVALID_HANDLE.\n", GetServerID()));
        sprintf_safe(m_szError, MAX_BUFF_500, "[CConnectClient::SendData] ConnectID = %d, get_handle() == ACE_INVALID_HANDLE.\n", GetServerID());
        App_MessageBlockManager::instance()->Close(pmblk);
        return false;
    }

    //将消息放入队列，让output在反应器线程发送。
    ACE_Time_Value xtime = ACE_OS::gettimeofday();

    //队列已满，不能再放进去了,就不放进去了
    if (msg_queue()->is_full() == true)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::SendMessage] Connectid=%d,putq is full(%d).\n", GetServerID(), msg_queue()->message_count()));
        App_MessageBlockManager::instance()->Close(pmblk);
        return false;
    }

    if (this->putq(pmblk, &xtime) == -1)
    {
        OUR_DEBUG((LM_ERROR, "[CConnectClient::SendMessage] Connectid=%d,putq(%d) output errno = [%d].\n", GetServerID(), msg_queue()->message_count(), errno));
        App_MessageBlockManager::instance()->Close(pmblk);
    }
    else
    {
        int nWakeupRet = reactor()->schedule_wakeup(this, ACE_Event_Handler::WRITE_MASK);

        if (-1 == nWakeupRet)
        {
            OUR_DEBUG((LM_ERROR, "[CConnectClient::SendMessage] Connectid=%d, nWakeupRet(%d) output errno = [%d].\n", GetServerID(), nWakeupRet, errno));
        }
    }

    return true;
}

_ClientConnectInfo CConnectClient::GetClientConnectInfo()
{
    _ClientConnectInfo ClientConnectInfo;
    ClientConnectInfo.m_blValid       = true;
    ClientConnectInfo.m_addrRemote    = m_addrRemote;
    ClientConnectInfo.m_u4AliveTime   = (uint32)(ACE_OS::gettimeofday().sec() - m_atvBegin.sec());
    ClientConnectInfo.m_u4AllRecvSize = m_u4RecvSize;
    ClientConnectInfo.m_u4RecvCount   = m_u4RecvCount;
    ClientConnectInfo.m_u4AllSendSize = m_u4SendSize;
    ClientConnectInfo.m_u4SendCount   = m_u4SendCount;
    ClientConnectInfo.m_u4ConnectID   = m_nServerID;
    ClientConnectInfo.m_u4BeginTime   = (uint32)m_atvBegin.sec();
    return ClientConnectInfo;
}

bool CConnectClient::GetTimeout(ACE_Time_Value tvNow)
{
    ACE_Time_Value tvIntval(tvNow - m_atvRecv);

    if(m_emRecvState == SERVER_RECV_BEGIN && tvIntval.sec() > SERVER_RECV_TIMEOUT)
    {
        //接收数据处理已经超时，在这里打印出来
        OUR_DEBUG((LM_DEBUG,"[CConnectClient::GetTimeout]***(%d)recv dispose is timeout(%d)!***.\n", m_nServerID, tvIntval.sec()));
        return false;
    }
    else
    {
        return true;
    }
}


