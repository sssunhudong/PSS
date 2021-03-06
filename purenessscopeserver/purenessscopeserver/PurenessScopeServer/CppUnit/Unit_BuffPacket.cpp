#include "Unit_BuffPacket.h"

#ifdef _CPPUNIT_TEST

void CUnit_BuffPacket::setUp(void)
{
    m_pBuffPacket = new CBuffPacket();

    m_pBuffPacket->Init(DEFINE_PACKET_SIZE, 20480);
    m_pBuffPacket->SetNetSort(false);
}

void CUnit_BuffPacket::tearDown(void)
{
    delete m_pBuffPacket;
    m_pBuffPacket = NULL;
}

void CUnit_BuffPacket::Read_Write_BuffPacket(void)
{
    bool blRet    = false;
    uint8   u1Data = 1;
    uint16  u2Data = 2;
    uint32  u4Data = 4;
    uint64  u8Data = 8;
    int8    n1Data = -1;
    int16   n2Data = -2;
    int32   n4Data = -4;
    float32 f4Data = 4.0f;
    float64 f8Data = 8.0f;

    uint8   u1OutData = 0;
    uint16  u2OutData = 0;
    uint32  u4OutData = 0;
    uint64  u8OutData = 0;
    int8    n1OutData = 0;
    int16   n2OutData = 0;
    int32   n4OutData = 0;
    float32 f4OutData = 0.0f;
    float64 f8OutData = 0.0f;

    //д��
    (*m_pBuffPacket) << u1Data;
    (*m_pBuffPacket) << u2Data;
    (*m_pBuffPacket) << u4Data;
    (*m_pBuffPacket) << u8Data;
    (*m_pBuffPacket) << n1Data;
    (*m_pBuffPacket) << n2Data;
    (*m_pBuffPacket) << n4Data;
    (*m_pBuffPacket) << f4Data;
    (*m_pBuffPacket) << f8Data;

    //��ȡ
    (*m_pBuffPacket) >> u1OutData;
    (*m_pBuffPacket) >> u2OutData;
    (*m_pBuffPacket) >> u4OutData;
    (*m_pBuffPacket) >> u8OutData;
    (*m_pBuffPacket) >> n1OutData;
    (*m_pBuffPacket) >> n2OutData;
    (*m_pBuffPacket) >> n4OutData;
    (*m_pBuffPacket) >> f4OutData;
    (*m_pBuffPacket) >> f8OutData;

    if (m_pBuffPacket->GetPacketLen() - m_pBuffPacket->GetReadLen() != 0)
    {
        OUR_DEBUG((LM_INFO, "[Read_Write_String_BuffPacket]GetPacketLen=%d.\n", m_pBuffPacket->GetPacketLen()));
        CPPUNIT_ASSERT_MESSAGE("[Read_Write_String_BuffPacket]GetPacketLen is not zero.", true == blRet);
        return;
    }

    if(u1OutData == u1Data &&
       u2OutData == u2Data &&
       u4OutData == u4Data &&
       u8OutData == u8Data &&
       n1OutData == n1Data &&
       n2OutData == n2Data &&
       n4OutData == n4Data &&
       f4OutData == f4Data &&
       f8OutData == f8Data)
    {
        blRet = true;
    }

    CPPUNIT_ASSERT_MESSAGE("[Read_Write_BuffPacket]fail.", true == blRet);
}

void CUnit_BuffPacket::Read_Write_String_BuffPacket(void)
{
    bool blRet = false;
    char szData[30]    = { '\0' };

    _VCHARS_STR obj_VCHARS_STR;
    _VCHARM_STR obj_VCHARM_STR;
    _VCHARB_STR obj_VCHARB_STR;

    _VCHARS_STR obj_Ret_VCHARS_STR;
    _VCHARM_STR obj_Ret_VCHARM_STR;
    _VCHARB_STR obj_Ret_VCHARB_STR;

    sprintf_safe(szData, 30, "freeeyes");

    obj_VCHARS_STR.text = szData;
    obj_VCHARS_STR.u1Len = (uint8)ACE_OS::strlen(szData);
    obj_VCHARM_STR.text = szData;
    obj_VCHARM_STR.u2Len = (uint16)ACE_OS::strlen(szData);
    obj_VCHARB_STR.text = szData;
    obj_VCHARB_STR.u4Len = (uint32)ACE_OS::strlen(szData);

    (*m_pBuffPacket) << obj_VCHARS_STR;
    (*m_pBuffPacket) << obj_VCHARM_STR;
    (*m_pBuffPacket) << obj_VCHARB_STR;

    (*m_pBuffPacket) >> obj_Ret_VCHARS_STR;
    (*m_pBuffPacket) >> obj_Ret_VCHARM_STR;
    (*m_pBuffPacket) >> obj_Ret_VCHARB_STR;

    if (m_pBuffPacket->GetPacketLen() - m_pBuffPacket->GetReadLen() != 0)
    {
        OUR_DEBUG((LM_INFO, "[Read_Write_String_BuffPacket]GetPacketLen=%d.\n", m_pBuffPacket->GetPacketLen()));
        CPPUNIT_ASSERT_MESSAGE("[Read_Write_String_BuffPacket]GetPacketLen is not zero.", true == blRet);
        return;
    }

    if (ACE_OS::strcmp(obj_VCHARS_STR.text, obj_Ret_VCHARS_STR.text) == 0 &&
        ACE_OS::strcmp(obj_VCHARM_STR.text, obj_Ret_VCHARM_STR.text) == 0 &&
        ACE_OS::strcmp(obj_VCHARB_STR.text, obj_Ret_VCHARB_STR.text) == 0)
    {
        blRet = true;
    }

    CPPUNIT_ASSERT_MESSAGE("[Read_Write_String_BuffPacket]fail.", true == blRet);
}

void CUnit_BuffPacket::Read_Write_Binary_BuffPacket(void)
{
    bool blRet = false;
    char szData[30] = { '\0' };
    char szRetData[30] = { '\0' };

    sprintf_safe(szData, 30, "freeeyes");

    m_pBuffPacket->WriteStream(szData, ACE_OS::strlen(szData));

    if (m_pBuffPacket->GetPacketLen() != ACE_OS::strlen(szData))
    {
        CPPUNIT_ASSERT_MESSAGE("[Read_Write_Binary_BuffPacket]write len fail.", true == blRet);
        return;
    }

    uint32 u4ReadLen = ACE_OS::strlen(szData);
    m_pBuffPacket->ReadStream(szRetData, u4ReadLen);

    if (m_pBuffPacket->GetPacketLen() - m_pBuffPacket->GetReadLen() != 0)
    {
        OUR_DEBUG((LM_INFO, "[Read_Write_String_BuffPacket]GetPacketLen=%d.\n", m_pBuffPacket->GetPacketLen()));
        CPPUNIT_ASSERT_MESSAGE("[Read_Write_Binary_BuffPacket]GetPacketLen is not zero.", true == blRet);
        return;
    }

    if (ACE_OS::strcmp(szData, szRetData) == 0)
    {
        blRet = true;
    }

    CPPUNIT_ASSERT_MESSAGE("[Read_Write_String_BuffPacket]fail.", true == blRet);
}

#endif
