#include "StdAfx.h"
#include "DataPack.h"
#include "DataParser.h"
#include "../Tools/LogTool.h"
#include "../TerminalConfig/TerminalConfig.h"
#include "../GPSData/GPSData.h"
#include "../SQLite3/SQLite3Class.h"
#include "../MsgProc/MsgProc.h"
#include "../DataParser/ComManager.h"
//初始化静态成员变量
CDataPack*	CDataPack::m_pInstance = NULL;

extern void TTSSpeaking(CString csText);
BYTE pAuhenBuf[64];

CDataPack::CDataPack(void)
{
	memset(m_databuf, NULL, sizeof(m_databuf));
	memset(&m_stMsgHead, NULL, sizeof(m_stMsgHead));
	m_dwDataLength = 0;
	m_hMutex		= NULL;
	m_bIsSubing = false;
	m_wsendcount = 0;
}

CDataPack::~CDataPack(void)
{
	if(NULL != m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = NULL;
	}
}

/************************************************************************
* 函数名:	Instance
* 描  述: 
* 入  参:
* 出  参: 
* 返  回: 
* 备  注:	singleton
************************************************************************/
CDataPack* CDataPack::Instance()
{
	if(NULL == m_pInstance)
	{
		m_pInstance = new CDataPack;
	}
	return m_pInstance;
}

/************************************************************************
* 函数名:	BuildFullMsg
* 描  述:	生成消息结构
* 入  参:	1. dwMsgID:			消息ID
*			2. pcMsgBody:		消息体buffer地址
*			3. wBodyLength:		消息体buffer长度
*			4. bSubPack:		分包：FALSE 无分包 TRUE 有分包，默认FALSE
*			5. wMsgPackNum:		消息总包数，默认0
*			6. wMsgPackIndex:	包序号，默认0
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
VOID CDataPack::BuildFullMsg(WORD wMsgID, PBYTE pcMsgBody, WORD wBodyLength, BOOL bSubPack, WORD wMsgPackNum, WORD wMsgPackIndex)
{

	WaitForSingleObject(m_hMutex,INFINITE);

	//0.初始化消息buffer
	memset(m_databuf, NULL, sizeof(m_databuf));
	m_dwDataLength = 0;

	//1.填充消息头结构体
	m_stMsgHead.wMsgID						= wMsgID;		//消息ID
	m_stMsgHead.stProperty.wMsgBodyLength	= wBodyLength;	//消息体buffer长度
	m_stMsgHead.stProperty.ucSubPack		= bSubPack;		//分包
	m_stMsgHead.wMsgSerialNum++;							//消息流水号自增1
	//if( m_stMsgHead.stProperty.ucSubPack )				//如果有分包，则填充分包结构体
	{
		m_stMsgHead.stPackItem.wMsgPackNum	= wMsgPackNum;	//消息总包数
		m_stMsgHead.stPackItem.wMsgPackIndex= wMsgPackIndex;//包序号
	}

	//2.生成消息头
	BuildHead();

	//3.生成消息体
	WORD wOffset = 0;
	m_databuf[wOffset++]	= 0x7E;		//头标识位
	//跳过消息头部分buffer
	if( m_stMsgHead.stProperty.ucSubPack )
		wOffset += 16;	//有消息分包项的消息头长度
	else
		wOffset += 12;	//无消息分包项的消息头长度
	//消息体
	memcpy(&m_databuf[wOffset], pcMsgBody, m_stMsgHead.stProperty.wMsgBodyLength);

	wOffset += m_stMsgHead.stProperty.wMsgBodyLength;

	//4.计算校验和
	m_databuf[wOffset++]	= CalcXor(&m_databuf[1], wOffset-1);
	m_databuf[wOffset++]	= 0x7E;		//尾标识位
	m_dwDataLength			= wOffset;	//消息buffer的长度

	//5.标识位转义
	DataCoding();

	int nSendResult = CComManager::Instance()->SendData(m_databuf, m_dwDataLength);


	ReleaseMutex(m_hMutex);

	if (m_wsendcount > 5)
	{
		m_wsendcount = 0;
		CMsgProc::Instance()->SendServerMsg(10054, 0, FALSE);
	}

	m_wsendcount++;
}

/************************************************************************
* 函数名:	BuildHead
* 描  述:	生成消息头部分的buffer
* 入  参:
* 出  参: 
* 返  回:	消息头的长度
* 备  注: 
************************************************************************/
WORD CDataPack::BuildHead()
{
	WORD wOffset = 1;	//消息头从第2字节开始

	//1.消息ID
	m_databuf[wOffset++]	= m_stMsgHead.wMsgID >> 8;
	m_databuf[wOffset++]	= m_stMsgHead.wMsgID & 0x00FF;

	WORD wSubpack  = m_stMsgHead.stProperty.ucSubPack;
	WORD wEncry = m_stMsgHead.stProperty.ucEncryptionType;

	wSubpack = (wSubpack<<13)&0x2000;
	wEncry = (wEncry<<10) & 0x1C00;
	//2.消息体属性(分包，数据加密方式，消息体长度)
	WORD wMsgProperty = 0;
	wMsgProperty = ( m_stMsgHead.stProperty.wMsgBodyLength & 0x03FF ) + wEncry + wSubpack;			//0~9 bit	消息体长度  加密方式 分包标志
	

	m_databuf[wOffset++]	= wMsgProperty >> 8;
	m_databuf[wOffset++]	= wMsgProperty & 0x00FF;

	//3.终端手机号，例："13439004154" -> 0x01 34 39 00 41 54
	PBYTE pNum = m_stMsgHead.ucPhoneNumber;
	//检查电话号码合法性，11位全部是否都为数字
	if( IsDigitalStr(pNum, 11) )
	{
		m_databuf[wOffset++]	= pNum[0] - '0';
		m_databuf[wOffset++]	= ( (pNum[1] - '0') << 4 ) + ( (pNum[2] - '0') & 0x00FF );
		m_databuf[wOffset++]	= ( (pNum[3] - '0') << 4 ) + ( (pNum[4] - '0') & 0x00FF );
		m_databuf[wOffset++]	= ( (pNum[5] - '0') << 4 ) + ( (pNum[6] - '0') & 0x00FF );
		m_databuf[wOffset++]	= ( (pNum[7] - '0') << 4 ) + ( (pNum[8] - '0') & 0x00FF );
		m_databuf[wOffset++]	= ( (pNum[9] - '0') << 4 ) + ( (pNum[10] - '0') & 0x00FF );
	}
	//如果号码异常，则全部置为0
	else
	{
		memset(&m_databuf[wOffset], NULL, 6);
		wOffset	+= 6;
	}

	//4.消息流水号
	m_databuf[wOffset++]	= m_stMsgHead.wMsgSerialNum >> 8;
	m_databuf[wOffset++]	= m_stMsgHead.wMsgSerialNum & 0x00FF;

	//5.消息包封装项
	if( m_stMsgHead.stProperty.ucSubPack )				//如果有分包，则填充分包结构体
	{
		m_databuf[wOffset++]	= m_stMsgHead.stPackItem.wMsgPackNum >> 8;		//消息总包数
		m_databuf[wOffset++]	= m_stMsgHead.stPackItem.wMsgPackNum & 0x00FF;
		m_databuf[wOffset++]	= m_stMsgHead.stPackItem.wMsgPackIndex >> 8;	//包序号
		m_databuf[wOffset++]	= m_stMsgHead.stPackItem.wMsgPackIndex & 0x00FF;
	}

	return wOffset-1;	//返回消息头的长度
}

/************************************************************************
* 函数名:	InsertChar
* 描  述:	为对消息转义处理，在指定位置插入字符(0x7E->0x7D02	0x7D->0x7D01)
* 入  参:	1. nIndex:	插入字符位置
*			2. cValue:	插入字符值
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
VOID CDataPack::InsertChar(INT nIndex, BYTE cValue)
{
	//移位
	for(int j=m_dwDataLength; j>nIndex; j--)
	{
		m_databuf[j] = m_databuf[j-1];
	}
	//插入
	m_databuf[nIndex] = cValue;
}

/************************************************************************
* 函数名:	Int2Str
* 描  述:	大端模式(big-endian)的网络字节序传送字和双字
* 入  参:	1. pcMsgBody:	保存数据的buffer
*			2. wData:		
* 出  参: 
* 返  回: 
* 备  注:	先传递高八位，再传低八位
************************************************************************/
WORD CDataPack::Int2Str(PBYTE pcMsgBody, WORD wData)
{
	pcMsgBody[0]	= (BYTE)((wData & 0xFF00) >> 8);
	pcMsgBody[1]	= (BYTE)(wData & 0x00FF);
	return 2;
}

/************************************************************************
* 函数名:	Int2Str
* 描  述:	大端模式(big-endian)的网络字节序传送字和双字
* 入  参:	1. pcMsgBody:	保存数据的buffer
*			2. dwData:		
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
WORD CDataPack::Int2Str(PBYTE pcMsgBody, DWORD dwData)
{
	pcMsgBody[0]	= (BYTE)((dwData & 0xFF000000) >> 24);
	pcMsgBody[1]	= (BYTE)((dwData & 0x00FF0000) >> 16);
	pcMsgBody[2]	= (BYTE)((dwData & 0x0000FF00) >> 8);
	pcMsgBody[3]	= (BYTE)(dwData & 0x000000FF);
	return 4;
}

/************************************************************************
* 函数名:	IsDigitalStr
* 描  述:	检查是否为数字字符串
* 入  参:	1. pcMsgBody:	保存数据的buffer
*			2. dwData:		
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::IsDigitalStr(PBYTE pBuffer, DWORD dwLen)
{
	for(DWORD i=0; i<dwLen; i++)
	{
		if( isdigit(pBuffer[i]) == 0 )
			return FALSE;
	}
	return TRUE;
}

/************************************************************************
* 函数名:	PackParam
* 描  述:	字符串拷贝
* 入  参:	1.dwParamID:	参数ID
			2. pcMsgBody:	保存数据的buffer
*			3. wData:		WORD参数
* 出  参: 
* 返  回:	WORD：打包参数的ID，长度，和值的总长度
* 备  注: 
************************************************************************/
WORD CDataPack::PackParam(DWORD dwParamID, PBYTE pcMsgBody, WORD wData)
{
	WORD wOffset	= 0;
	wOffset	+= Int2Str(pcMsgBody, dwParamID);		//参数ID
	pcMsgBody[wOffset++]	= sizeof(wData);		//参数长度
	wOffset	+= Int2Str(&pcMsgBody[wOffset], wData);	//参数值
	
	return wOffset;
}

/************************************************************************
* 函数名:	PackParam
* 描  述:	字符串拷贝
* 入  参:	1.dwParamID:	参数ID
			2. pcMsgBody:	保存数据的buffer
*			3. wData:		DWORD参数
* 出  参: 
* 返  回:	WORD：打包参数的ID，长度，和值的总长度
* 备  注: 
************************************************************************/
WORD CDataPack::PackParam(DWORD dwParamID, PBYTE pcMsgBody, DWORD dwData)
{
	WORD wOffset	= 0;
	wOffset	+= Int2Str(pcMsgBody, dwParamID);			//参数ID
	pcMsgBody[wOffset++]	= sizeof(dwData);			//参数长度
	wOffset	+= Int2Str(&pcMsgBody[wOffset], dwData);	//参数值

	return wOffset;
}

/************************************************************************
* 函数名:	PackParam
* 描  述:	字符串拷贝
* 入  参:	1.dwParamID:	参数ID
			2. pcMsgBody:	保存数据的buffer
*			3. pszStr:		字符串参数
* 出  参: 
* 返  回:	WORD：打包参数的ID，长度，和值的总长度
* 备  注: 
************************************************************************/
WORD CDataPack::PackParam(DWORD dwParamID, PBYTE pcMsgBody, PCSTR pszStr)
{
	WORD wOffset	= 0;
	WORD wStrLen	= strlen(pszStr);
	wOffset	+= Int2Str(pcMsgBody, dwParamID);		//参数ID
	pcMsgBody[wOffset++]	= (BYTE)wStrLen;		//参数长度
	memcpy(&pcMsgBody[wOffset], pszStr, wStrLen);	//参数值
	wOffset += wStrLen;

	return wOffset;
}

/************************************************************************
* 函数名:	PackPosInfo
* 描  述:	打包位置信息，包括位置基本信息和位置附加信息项列表
* 入  参:	1. pcPosInfo:	位置基本信息和位置附加信息项列表存放在此buffer中
* 出  参: 
* 返  回:	WORD: buffer长度
* 备  注:	
************************************************************************/
WORD CDataPack::PackPosInfo(PBYTE pcPosInfo)
{
	//检查超速报警，进出区域，进出路线，设置状态位
	CGPSData::Instance()->CheckState();

	GetLocalTime(&st);
	
	g_alarm_state.unState.gps_state.cFix            = g_pGpsPosInfo->rmc_data.pos_state.cFix;
	g_alarm_state.unState.gps_state.cLatitudeType   = g_pGpsPosInfo->rmc_data.pos_state.cLatitudeType;
	g_alarm_state.unState.gps_state.cLongitudeType  = g_pGpsPosInfo->rmc_data.pos_state.cLongitudeType;

	Int2Str(&pcPosInfo[0], (DWORD)(g_alarm_state.unAlarm.dwAlarmFlag));
	Int2Str(&pcPosInfo[4], (DWORD)(g_alarm_state.unState.dwGPSState));//状态
	Int2Str(&pcPosInfo[8], (DWORD)(g_pGpsPosInfo->rmc_data.dbLatitude*1e6) );//纬度
	Int2Str(&pcPosInfo[12],(DWORD)(g_pGpsPosInfo->rmc_data.dbLongitude*1e6) ); //经度
	Int2Str(&pcPosInfo[16],(WORD)(g_pGpsPosInfo->rmc_data.dbAltitude+0.5) ); //高程
	Int2Str(&pcPosInfo[18],(WORD)(g_pGpsPosInfo->rmc_data.dbSpeed * 10) );//速度
	Int2Str(&pcPosInfo[20],(WORD)g_pGpsPosInfo->rmc_data.dbAzimuth);//方向

	pcPosInfo[22] = HEX2BCD(st.wYear);
	pcPosInfo[23] = HEX2BCD(st.wMonth);
	pcPosInfo[24] = HEX2BCD(st.wDay);
	pcPosInfo[25] = HEX2BCD(st.wHour);
	pcPosInfo[26] = HEX2BCD(st.wMinute);
	pcPosInfo[27] = HEX2BCD(st.wSecond);

	//位置附加信息项列表
	//1.里程
	pcPosInfo[28]	= ID_Mileage;			//附加信息ID
	pcPosInfo[29]	= sizeof(DWORD);		//附加信息长度
	Int2Str(&pcPosInfo[30], g_alarm_state.gps_extra_info.dwMileage/3600);	

	//2.油量
	pcPosInfo[34]	= ID_OilMass;			//附加信息ID
	pcPosInfo[35]	= sizeof(DWORD);		//附加信息长度
	Int2Str(&pcPosInfo[36], g_alarm_state.gps_extra_info.dwOilMass);	

	//3.行驶记录功能获取的速度
	pcPosInfo[40]	= ID_Speed;				//附加信息ID
	pcPosInfo[41]	= sizeof(WORD);			//附加信息长度
	Int2Str(&pcPosInfo[42], g_alarm_state.gps_extra_info.wSpeed);

	//4.超速报警附加信息
	pcPosInfo[44]	= ID_AlarmOverSpeed;	//附加信息ID
	pcPosInfo[45]	= 1;				//附加信息长度
	pcPosInfo[46]	= enNO_GIVEN_POS;	//位置类型:	0:无特定位置

	//5.进出区域/线路报警附加信息
	pcPosInfo[47]	= ID_AlarmInOutRegionLine;	//附加信息ID
	pcPosInfo[48]	= 6;						//附加信息长度
	pcPosInfo[59]	= g_alarm_state.gps_extra_info.inout_region_line.enPosType;	//位置类型:	1:圆形区域 2:矩形区域 3:多边形区域 4:路段
	Int2Str(&pcPosInfo[50], g_alarm_state.gps_extra_info.inout_region_line.dwRegionLineID);	//区域或路段ID	
	pcPosInfo[54]	= g_alarm_state.gps_extra_info.inout_region_line.cDirection;	//方向：0:进 1:出

	//6.路段行驶时间不足/过长报警附加信息
	pcPosInfo[55]	= ID_AlarmDriveTime;		//附加信息ID
	pcPosInfo[56]	= 7;						//附加信息长度
	Int2Str(&pcPosInfo[57],g_alarm_state.gps_extra_info.drive_time.dwLineID);		//路段ID
	Int2Str(&pcPosInfo[61], g_alarm_state.gps_extra_info.drive_time.wDriveTime);	//路段行驶时间
	pcPosInfo[63] = g_alarm_state.gps_extra_info.drive_time.cResult;						//结果  0:不足  1:过长


	return 64;
}

//打包纯GPS位置信息
WORD CDataPack::PackGPSPosInfo(PBYTE pGpsInfo)
{
	GetLocalTime(&st);
	g_alarm_state.unState.gps_state.cFix            = g_pGpsPosInfo->rmc_data.pos_state.cFix;
	g_alarm_state.unState.gps_state.cLatitudeType   = g_pGpsPosInfo->rmc_data.pos_state.cLatitudeType;
	g_alarm_state.unState.gps_state.cLongitudeType  = g_pGpsPosInfo->rmc_data.pos_state.cLongitudeType;

	Int2Str(&pGpsInfo[0], (DWORD)(g_alarm_state.unAlarm.dwAlarmFlag));
	Int2Str(&pGpsInfo[4], (DWORD)(g_alarm_state.unState.dwGPSState));//状态
	Int2Str(&pGpsInfo[8], (DWORD)(g_pGpsPosInfo->rmc_data.dbLatitude*1e6) );//纬度
	Int2Str(&pGpsInfo[12],(DWORD)(g_pGpsPosInfo->rmc_data.dbLongitude*1e6) ); //经度
	Int2Str(&pGpsInfo[16],(WORD)(g_pGpsPosInfo->rmc_data.dbAltitude+0.5) ); //高程
	Int2Str(&pGpsInfo[18],(WORD)(g_pGpsPosInfo->rmc_data.dbSpeed * 10) );//速度
	Int2Str(&pGpsInfo[20],(WORD)g_pGpsPosInfo->rmc_data.dbAzimuth);//方向

	pGpsInfo[22] = HEX2BCD(st.wYear);
	pGpsInfo[23] = HEX2BCD(st.wMonth);
	pGpsInfo[24] = HEX2BCD(st.wDay);
	pGpsInfo[25] = HEX2BCD(st.wHour);
	pGpsInfo[26] = HEX2BCD(st.wMinute);
	pGpsInfo[27] = HEX2BCD(st.wSecond);

	return 28;
}

/************************************************************************
* 函数名:	DataCoding
* 描  述:	标识位转义，将源pcBuffer里面的相应字符转换:0x7E->0x7D02	0x7D->0x7D01
* 入  参:	1. pcBuffer:	目标协议消息字符串
*			2. nBufferLen:	目标协议消息字符串的长度
* 出  参: 
* 返  回:	INT:	返回转换后的buffer长度，小于源长度，-1代表操作失败
* 备  注:	0x30 7E 08 7D 55 -> 0x30 7D 02 08 7D 01 55
************************************************************************/
VOID CDataPack::DataCoding()
{
	if(0 == m_dwDataLength)
		return;

	WORD i = 0;
	//0x7D->0x7D01
	for(i=1; (i<(m_dwDataLength-1)) && (i<(MSG_BUFFER_LEN-1)); i++)
	{
		if(0x7D == m_databuf[i])
		{
			InsertChar(i+1, 0x01);
			m_dwDataLength++;
		}
	}
	//0x7E->0x7D01
	for(i=1; (i<(m_dwDataLength-1)) && (i<(MSG_BUFFER_LEN-1)); i++)
	{
		if(0x7E == m_databuf[i])
		{
			m_databuf[i] = 0x7D;
			InsertChar(i+1, 0x02);
			m_dwDataLength++;
		}
	}
}

/************************************************************************
* 函数名:	SetMsgProperty
* 描  述:	设置消息头属性
* 入  参:	
* 出  参:	
* 返  回:	
* 备  注:	电话号码，数据加密方式
************************************************************************/
VOID CDataPack::SetMsgProperty(LPCSTR pcPhoneNumber, BYTE cEncryptionType)
{
	//电话号码
	strcpy((PSTR)m_stMsgHead.ucPhoneNumber, pcPhoneNumber);

	//数据加密方式	0:不加密	1:RSA加密
	m_stMsgHead.stProperty.ucEncryptionType	= cEncryptionType;
}

/************************************************************************
* 函数名:	PackCOMMON_ACK
* 描  述:	1.终端通用应答0x0001
* 入  参:	1.wAckMsgID:		应答消息ID，对应的平台消息ID
*			2.cResult:			结果，0：成功/确认；1：失败；2消息有误；3：不支持
* 出  参:	
* 返  回:	
* 备  注:	
************************************************************************/
BOOL CDataPack::PackCOMMON_ACK(enResult Result)
{
	//打包消息体
	BYTE	cMsgBody[16];
	WORD	wOffset	= 0;
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	//获取消息头结构体
	const PMSG_HEAD	p_msg_head	= CDataParser::Instance()->GetMsgHead();

	//应答流水号
	wOffset += Int2Str(&cMsgBody[wOffset], p_msg_head->wMsgSerialNum);
	//应答消息ID
	wOffset += Int2Str(&cMsgBody[wOffset], p_msg_head->wMsgID);
	cMsgBody[wOffset++]	= (BYTE)Result;		//结果

	CLogTool::Instance()->WriteLogFile("Terminal common ack(0x0001)--MsgID:0x%04X, SerialNum:0x%04X, Result:%d", 
		p_msg_head->wMsgID, p_msg_head->wMsgSerialNum, Result);
	//打包消息并发送
	BuildFullMsg(MSG_TMN_COMMON_ACK, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackHEARTBEAT
* 描  述:	3.终端心跳0x0002
* 入  参:	
* 出  参:	
* 返  回:	
* 备  注:	心跳
************************************************************************/
BOOL CDataPack::PackHEARTBEAT()
{
	BYTE	cMsgBody[4];
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	CLogTool::Instance()->WriteLogFile("Heart Beat!");

	if (m_bIsSubing)
	{
		return FALSE;
	}


	//打包消息并发送
	BuildFullMsg(MSG_TMN_HEARTBEAT, cMsgBody, 0);
	return TRUE;
}

BOOL CDataPack::PackTimeRequest()
{
	static bool bjust = false;

	if (bjust)
	{
		return TRUE;
	}

	BYTE	cMsgBody[4];
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	CLogTool::Instance()->WriteLogFile("TimeRequest!");


	//打包消息并发送
	BuildFullMsg(MSG_TMN_TIME_ADJUST_REQUEST, cMsgBody, 0);

	bjust = true;

	return TRUE;
}

/************************************************************************
* 函数名:	PackREGISTER
* 描  述:	4.终端注册0x0100
* 入  参:	1.wProvinceID:			省域ID，2byte
*			2.wCityID:				市县域ID，2byte
*			3.pcManufacturerID:		制造商ID，5byte
*			4.pcTerminalType:		终端型号，8byte
*			5.pcTerminalID:			终端ID，7byte
*			6.cNumberPlateColor:	车牌颜色，1byte，1 蓝   2 黄   3 黑   4 白   9 其他
*			7.pcPlateNumber:		车牌号码，wNumberLength byte
*			8.wNumberLength:		车牌号码长度
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackREGISTER(WORD wProvinceID, WORD wCityID, LPCSTR pcManufacturerID, LPCSTR pcTerminalType, 
							 LPCSTR pcTerminalID, BYTE cNumberPlateColor, LPCSTR pcPlateNumber, WORD wNumberLength)
{
	//打包消息体
	BYTE	cMsgBody[128]	= {0};
	WORD	wOffset		= 0;
	cMsgBody[wOffset++]	= wProvinceID >> 8;				//省域ID，2byte
	cMsgBody[wOffset++]	= wProvinceID & 0x00FF;
	cMsgBody[wOffset++]	= wCityID >> 8;					//市县域ID，2byte
	cMsgBody[wOffset++]	= wCityID & 0x00FF;
	strncpy((LPSTR)&cMsgBody[wOffset], pcManufacturerID, 5);	//制造商ID，5byte
	wOffset += 5;
	strncpy((LPSTR)&cMsgBody[wOffset], pcTerminalType, 20);		//终端型号，8byte
	wOffset += 20;
	strncpy((LPSTR)&cMsgBody[wOffset], pcTerminalID, 7);		//终端ID，7byte
	wOffset += 7;
	cMsgBody[wOffset++]	= cNumberPlateColor;			//车牌颜色，1byte
	strncpy((LPSTR)&cMsgBody[wOffset], pcPlateNumber, wNumberLength);	//车牌号码，wNumberLength byte
	wOffset += wNumberLength;

	CLogTool::Instance()->WriteLogFile(
		"Register(0x0100)--ProvinceID:%d, CityID:%d, ManufacturerID:%s, TerminalType:%s, TerminalID:%s, NumberPlateColor:%d, PlateNumber:%s", 
		wProvinceID, wCityID, pcManufacturerID, pcTerminalType, pcTerminalID, cNumberPlateColor, pcPlateNumber);

	//打包消息并发送
	BuildFullMsg(MSG_TMN_REGISTER, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackLOGOUT
* 描  述:	6.终端注销0x0003
* 入  参:	
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackLOGOUT()
{
	BYTE	cMsgBody[4];
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	CLogTool::Instance()->WriteLogFile("Log out(0x0003)!");
	//打包消息并发送
	BuildFullMsg(MSG_TMN_LOGOUT, cMsgBody, 0);

	return TRUE;
}

/************************************************************************
* 函数名:	PackAUTHENTICATION
* 描  述:	7.终端鉴权0x0102
* 入  参:	1.wProvinceID:			省域ID，2byte
*			2.wCityID:				市县域ID，2byte
*			3.pcManufacturerID:		制造商ID，5byte
*			4.pcTerminalType:		终端型号，8byte
*			5.pcTerminalID:			终端ID，7byte
*			6.cNumberPlateColor:	车牌颜色，1byte，1 蓝   2 黄   3 黑   4 白   9 其他
*			7.pcPlateNumber:		车牌号码，wNumberLength byte
*			8.wNumberLength:		车牌号码长度
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackAUTHENTICATION(PCSTR pAuthenCode/* = NULL*/)
{
	const PREGISTER_ACK	p_register_ack	= CDataParser::Instance()->GetRegisterAck();
	PBYTE pBuf	= NULL;
	memset(&pAuhenBuf, 0, sizeof(pAuhenBuf));
	if( NULL == pAuthenCode )
		pBuf = p_register_ack->cAuthenCode;
	else
		pBuf = (PBYTE)pAuthenCode;

	memcpy(&pAuhenBuf, pBuf, strlen((PSTR)pBuf));
	CLogTool::Instance()->WriteLogFile("Authentiacation(0x0102)--AuthenCode:%s", pAuhenBuf);

	//打包消息并发送
	BuildFullMsg(MSG_TMN_AUTHENTICATION, pAuhenBuf, strlen((PSTR)pAuhenBuf));

	CreateThread(NULL, 0, THDAuthentiaction, this, 0, NULL);

	return TRUE;
}


DWORD CDataPack::THDAuthentiaction(LPVOID lp)
{
	CDataPack *datapack = (CDataPack*)lp;
	int i = 3;

	while(i > 0)
	{
		Sleep(20*1000);
		if (CMsgProc::Instance()->GetLoginState() || CMsgProc::Instance()->GetSocketStatus() == -1)
		{
			return 0;
		}

		CLogTool::Instance()->WriteLogFile("Authentiacation(0x0102)--AuthenCode:%s", pAuhenBuf);

		//打包消息并发送
		datapack->BuildFullMsg(MSG_TMN_AUTHENTICATION, pAuhenBuf, strlen((PSTR)pAuhenBuf));
		i--;

	}
	CMsgProc::Instance()->ReConnectServer();

	return 0;
}

/************************************************************************
* 函数名:	PackPARAM_GET_ACK
* 描  述:	10.查询终端参数应答0x0104
* 入  参:	1.wMsgSerialNum:	应答流水号
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackPARAM_GET_ACK()
{
	//打包消息体
	BYTE	cMsgBody[NUM_1K];
	WORD	wOffset	= 0;	//位移
	DWORD	dwParamID	= 0;
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	//获取消息头结构体
	const PMSG_HEAD			p_msg_head	= CDataParser::Instance()->GetMsgHead();
	const PTERMINAL_PARAM	pParam		= CTerminalConfig::Instance()->GetTerminalParam();

	//应答流水号
	wOffset += Int2Str(&cMsgBody[wOffset], p_msg_head->wMsgSerialNum);

	//应答参数个数
	cMsgBody[wOffset++]	= 60;

	/***************打包参数项********************/
	//(0x0001)终端心跳发送间隔，单位为秒(s)
	wOffset += PackParam(PARAM_HEART_BEAT_SLICE, &cMsgBody[wOffset], pParam->dwHEART_BEAT_SLICE);

	//(0x0002)TCP消息应答超时时间，单位为秒(s)
	wOffset += PackParam(PARAM_TCP_ACK_OVERTIME, &cMsgBody[wOffset], pParam->dwTCP_ACK_OVERTIME);

	//(0x0003)TCP消息重传次数
	wOffset += PackParam(PARAM_TCP_RETRANSMIT_COUNT, &cMsgBody[wOffset], pParam->dwTCP_RETRANSMIT_COUNT);

	//(0x0004)UDP消息应答超时时间，单位为秒(s)
	wOffset += PackParam(PARAM_UDP_ACK_OVERTIME, &cMsgBody[wOffset], pParam->dwUDP_ACK_OVERTIME);

	//(0x0005)UDP消息重传次数
	wOffset += PackParam(PARAM_UDP_RETRANSMIT_COUNT, &cMsgBody[wOffset], pParam->dwUDP_RETRANSMIT_COUNT);

	//(0x0006)SMS消息应答超时时间，单位为秒(s)
	wOffset += PackParam(PARAM_SMS_ACK_OVERTIME, &cMsgBody[wOffset], pParam->dwSMS_ACK_OVERTIME);

	//(0x0007)SMS消息重传次数
	wOffset += PackParam(PARAM_SMS_RETRANSMIT_COUNT, &cMsgBody[wOffset], pParam->dwSMS_RETRANSMIT_COUNT);

	//0x0008~0x000F		保留
	//(0x0010)主服务器APN
	wOffset += PackParam(PARAM_MAIN_SVR_APN, &cMsgBody[wOffset], pParam->szMAIN_SVR_APN);

	//(0x0011)主服务器无线通信拨号用户名
	wOffset += PackParam(PARAM_MAIN_SVR_NAME, &cMsgBody[wOffset], pParam->szMAIN_SVR_NAME);

	//(0x0012)主服务器无线通信拨号密码
	wOffset += PackParam(PARAM_MAIN_SVR_PWD, &cMsgBody[wOffset], pParam->szMAIN_SVR_PWD);	

	//(0x0013)主服务器地址，IP或域名
	wOffset += PackParam(PARAM_MAIN_SVR_IP, &cMsgBody[wOffset], pParam->szMAIN_SVR_IP);

	//(0x0014)备份服务器APN
	wOffset += PackParam(PARAM_BAK_SVR_APN, &cMsgBody[wOffset], pParam->szBAK_SVR_APN);

	//(0x0015)备份服务器无线通信拨号用户名
	wOffset += PackParam(PARAM_BAK_SVR_NAME, &cMsgBody[wOffset], pParam->szBAK_SVR_NAME);

	//(0x0016)备份服务器无线通信拨号密码
	wOffset += PackParam(PARAM_BAK_SVR_PWD, &cMsgBody[wOffset], pParam->szBAK_SVR_PWD);

	//(0x0017)备份服务器地址，IP或域名
	wOffset += PackParam(PARAM_BAK_SVR_IP, &cMsgBody[wOffset], pParam->szBAK_SVR_IP);

	//(0x0018)服务器TCP端口
	wOffset += PackParam(PARAM_SVR_TCP_PORT, &cMsgBody[wOffset], pParam->dwSVR_TCP_PORT);

	//(0x0019)服务器UDP端口
	wOffset += PackParam(PARAM_SVR_UDP_PORT, &cMsgBody[wOffset], pParam->dwSVR_UDP_PORT);

	//0x001A~0x001F		保留
	//(0x0020)位置汇报策略
	wOffset += PackParam(PARAM_POS_REPORT_TYPE, &cMsgBody[wOffset], pParam->dwPOS_REPORT_TYPE);

	//(0x0021)位置汇报方案
	wOffset += PackParam(PARAM_POS_REPORT_CONDITION, &cMsgBody[wOffset], pParam->dwPOS_REPORT_CONDITION);

	//(0x0022)驾驶员未登录汇报时间间隔，单位为秒(s), >0
	wOffset += PackParam(PARAM_POS_REPORT_SLICE_LOGOUT, &cMsgBody[wOffset], pParam->dwPOS_REPORT_SLICE_LOGOUT);

	//0x0023~0x0026		保留
	//(0x0027)休眠时汇报时间间隔，单位为秒(s), >0
	wOffset += PackParam(PARAM_POS_REPORT_SLICE_SLEEP, &cMsgBody[wOffset], pParam->dwPOS_REPORT_SLICE_SLEEP);

	//(0x0028)紧急报警时汇报时间间隔，单位为秒(s), >0
	wOffset += PackParam(PARAM_POS_REPORT_SLICE_ALARM, &cMsgBody[wOffset], pParam->dwPOS_REPORT_SLICE_ALARM);

	//(0x0029)缺省时间汇报间隔，单位为秒(s), >0
	wOffset += PackParam(PARAM_POS_REPORT_SLICE_DEFAULT, &cMsgBody[wOffset], pParam->dwPOS_REPORT_SLICE_DEFAULT);

	//0x002A~0x002B		保留
	//(0x002C)缺省距离汇报间隔，单位为米(m), >0
	wOffset += PackParam(PARAM_POS_REPORT_DIST_DEFAULT, &cMsgBody[wOffset], pParam->dwPOS_REPORT_DIST_DEFAULT);

	//(0x002D)驾驶员未登录汇报距离间隔，单位为米(m), >0
	wOffset += PackParam(PARAM_POS_REPORT_DIST_LOGOUT, &cMsgBody[wOffset], pParam->dwPOS_REPORT_DIST_LOGOUT);

	//(0x002E)休眠时汇报距离间隔，单位为米(m), >0
	wOffset += PackParam(PARAM_POS_REPORT_DIST_SLEEP, &cMsgBody[wOffset], pParam->dwPOS_REPORT_DIST_SLEEP);

	//(0x002F)紧急报警时汇报距离间隔，单位为米(m), >0
	wOffset += PackParam(PARAM_POS_REPORT_DIST_ALARM, &cMsgBody[wOffset], pParam->dwPOS_REPORT_DIST_ALARM);

	//(0x0030)拐点补传角度，<180
	wOffset += PackParam(PARAM_CORNER_RETRANSMIT_ANGLE, &cMsgBody[wOffset], pParam->dwCORNER_RETRANSMIT_ANGLE);

	//(0x0031)电子围栏半径（非法位移阈值），单位为米  Radius of the electronic fence
	wOffset += PackParam(PARAM_ELECTRONIC_FENCE_RADIUS, &cMsgBody[wOffset], pParam->wELECTRONIC_FENCE_RADIUS);

	//0x0032~0x003F		保留
	//(0x0040)监控平台电话号码
	wOffset += PackParam(PARAM_PHONE_NUM_MONITOR, &cMsgBody[wOffset], pParam->szPHONE_NUM_MONITOR);

	//(0x0041)复位电话号码
	wOffset += PackParam(PARAM_PHONE_NUM_RESET, &cMsgBody[wOffset], pParam->szPHONE_NUM_RESET);

	//(0x0042)恢复出厂设置电话号码
	wOffset += PackParam(PARAM_PHONE_NUM_RESTORE, &cMsgBody[wOffset], pParam->szPHONE_NUM_RESTORE);

	//(0x0043)监控平台SMS电话号码
	wOffset += PackParam(PARAM_PHONE_NUM_SVR_SMS, &cMsgBody[wOffset], pParam->szPHONE_NUM_SVR_SMS);

	//(0x0044)接收终端SMS文本报警号码
	wOffset += PackParam(PARAM_PHONE_NUM_TMN_SMS, &cMsgBody[wOffset], pParam->szPHONE_NUM_TMN_SMS);

	//(0x0045)终端电话接听策略
	wOffset += PackParam(PARAM_ANSWER_PHONE_TYPE, &cMsgBody[wOffset], pParam->dwANSWER_PHONE_TYPE);

	//(0x0046)每次最长通话时间
	wOffset += PackParam(PARAM_SINGLE_CALL_TIME_LIMIT, &cMsgBody[wOffset], pParam->dwSINGLE_CALL_TIME_LIMIT);

	//(0x0047)当月最长通话时间
	wOffset += PackParam(PARAM_MONTH_CALL_TIME_LIMIT, &cMsgBody[wOffset], pParam->dwMONTH_CALL_TIME_LIMIT);

	//(0x0048)监听电话号码
	wOffset += PackParam(PARAM_MONITOR_PHONE_NUM, &cMsgBody[wOffset], pParam->szMONITOR_PHONE_NUM);

	//(0x0049)监管平台特权短信号码
	wOffset += PackParam(PARAM_SVR_PRIVILEGE_SMS_NUM, &cMsgBody[wOffset], pParam->szSVR_PRIVILEGE_SMS_NUM);

	//0x004A~0x004F		保留
	//(0x0050)报警屏蔽字
	wOffset += PackParam(PARAM_ALARM_MASK_BIT, &cMsgBody[wOffset], pParam->dwALARM_MASK_BIT);

	//(0x0051)报警发送文本SMS开关
	wOffset += PackParam(PARAM_ALARM_SEND_SMS_SWITCH, &cMsgBody[wOffset], pParam->dwALARM_SEND_SMS_SWITCH);

	//(0x0052)报警拍摄开关
	wOffset += PackParam(PARAM_ALARM_CAPTURE_SWITCH, &cMsgBody[wOffset], pParam->dwALARM_CAPTURE_SWITCH);

	//(0x0053)报警拍摄存储标志
	wOffset += PackParam(PARAM_ALARM_CAPTURE_STORE_FLAG, &cMsgBody[wOffset], pParam->dwALARM_CAPTURE_STORE_FLAG);

	//(0x0054)关键标志
	wOffset += PackParam(PARAM_KEY_FLAG, &cMsgBody[wOffset], pParam->dwKEY_FLAG);

	//(0x0055)最高速度
	wOffset += PackParam(PARAM_SPEED_LIMIT, &cMsgBody[wOffset], pParam->dwSPEED_LIMIT);

	//(0x0056)超速持续时间
	wOffset += PackParam(PARAM_OVERSPEED_DUREATION, &cMsgBody[wOffset], pParam->dwOVERSPEED_DUREATION);

	//(0x0057)连续驾驶时间门限
	wOffset += PackParam(PARAM_CONTINUE_DRIVE_TIME_LIMIT, &cMsgBody[wOffset], pParam->dwCONTINUE_DRIVE_TIME_LIMIT);

	//(0x0058)当天累计驾驶时间门限
	wOffset += PackParam(PARAM_ONE_DAY_DRIVE_TIME_LIMIT, &cMsgBody[wOffset], pParam->dwONE_DAY_DRIVE_TIME_LIMIT);

	//(0x0059)最小休息时间
	wOffset += PackParam(PARAM_MINIMUM_REST_TIME, &cMsgBody[wOffset], pParam->dwMINIMUM_REST_TIME);

	//(0x005A)最长停车时间
	wOffset += PackParam(PARAM_MAXIMUM_PARKING_TIME, &cMsgBody[wOffset], pParam->dwMAXIMUM_PARKING_TIME);

	//0x005B~0x006F		保留
	//(0x0070)图像/视频质量，1～10，1最好
	wOffset += PackParam(PARAM_IMAGE_VIDEO_QUALITY, &cMsgBody[wOffset], pParam->dwIMAGE_VIDEO_QUALITY);

	//(0x0071)亮度，0～255
	wOffset += PackParam(PARAM_BRIGHTNESS, &cMsgBody[wOffset], pParam->dwBRIGHTNESS);

	//(0x0072)对比度，0～127
	wOffset += PackParam(PARAM_CONTRAST, &cMsgBody[wOffset], pParam->dwCONTRAST);

	//(0x0073)饱和度，0～127
	wOffset += PackParam(PARAM_SATURATION, &cMsgBody[wOffset], pParam->dwSATURATION);

	//(0x0074)色度，0～255
	wOffset += PackParam(PARAM_CHROMA, &cMsgBody[wOffset], pParam->dwCHROMA);

	//0x0075~0x007F		保留
	//(0x0080)车辆里程表读数，1/10km
	wOffset += PackParam(PARAM_VEHICLE_ODOMETER, &cMsgBody[wOffset], pParam->dwVEHICLE_ODOMETER);

	//(0x0081)车辆所在的省域ID
	wOffset += PackParam(PARAM_VEHICLE_PROVINCE_ID, &cMsgBody[wOffset], pParam->wVEHICLE_PROVINCE_ID);

	//(0x0082)车辆所在的市域ID
	wOffset += PackParam(PARAM_VEHICLE_CITY_ID, &cMsgBody[wOffset], pParam->wVEHICLE_CITY_ID);

	//(0x0083)公安交通管理部门颁发的机动车号码
	wOffset += PackParam(PARAM_VEHICLE_ID, &cMsgBody[wOffset], pParam->szVEHICLE_ID);

	//(0x0084)车牌颜色
	wOffset += PackParam(PARAM_PLATE_COLOR, &cMsgBody[wOffset], pParam->dwPLATE_COLOR);

	CLogTool::Instance()->WriteLogFile("Report terminal param(0x0104)!");
	//打包消息并发送
	BuildFullMsg(MSG_TMN_PARAM_GET_ACK, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackPOS_GET_ACK
* 描  述:	12.位置信息汇报0x0200
* 入  参:	
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackPOS_REPORT()
{
	//打包消息体
	BYTE	cMsgBody[NUM_512B];
	WORD	wOffset	= 0;	//位移


	memset(cMsgBody, NULL, sizeof(cMsgBody));


	if ( g_pGpsPosInfo->rmc_data.UtcTime.wSecond%10 ==0 )
	{
		CTerminalConfig::Instance()->SaveMileage(g_alarm_state.gps_extra_info.dwMileage/3600);
	}

	CLogTool::Instance()->WriteLogFile("Report position(0x0200)!");

	if (m_bIsSubing)
	{
		return FALSE;
	}

	//获取位置信息
	wOffset += PackPosInfo(cMsgBody);
	
	//打包消息并发送
	BuildFullMsg(MSG_TMN_POS_REPORT, cMsgBody, wOffset);
	

	return TRUE;
}

/************************************************************************
* 函数名:	PackPOS_GET_ACK
* 描  述:	14.位置信息查询应答0x0201
* 入  参:	
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackPOS_GET_ACK()
{
	//打包消息体
	BYTE	cMsgBody[NUM_1K];
	WORD	wOffset	= 0;	//位移

	memset(cMsgBody, NULL, sizeof(cMsgBody));

	const PMSG_HEAD	p_msg_head	= CDataParser::Instance()->GetMsgHead();

	//应答流水号
	Int2Str(&cMsgBody[wOffset], p_msg_head->wMsgSerialNum);
	wOffset += 2;
	//获取位置信息
	wOffset += PackPosInfo(&cMsgBody[wOffset]);

	//打包消息并发送
	BuildFullMsg(MSG_TMN_POS_GET_ACK, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackEVENT_REPORT
* 描  述:	18.事件报告0x0301
* 入  参:	1. cEventID:	事件ID
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackEVENT_REPORT(BYTE cEventID)
{
	CLogTool::Instance()->WriteLogFile("Report Event(0x0301)--EventID:%d", cEventID);
	//打包消息并发送
	BuildFullMsg(MSG_TMN_EVENT_REPORT, &cEventID, 1);

	return TRUE;
}

/************************************************************************
* 函数名:	PackQUESTION_ACK
* 描  述:	20.提问应答0x0302
* 入  参:	1. dwQuestionUID:	问题的UID
*			2. cAnswerID:		选择答案的ID
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackQUESTION_ACK(/*DWORD dwQuestionUID,*/ BYTE cAnswerID)
{
	//打包消息体
	BYTE	cMsgBody[NUM_256B];
	WORD	wOffset	= 0;	//位移
	WORD	wSerialNum = 0;	//提问流水号
	WORD	wQuestionUID = 0;//问题ID
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	//查询数据库
	CppSQLite3DB db;
	db.open(PATH_SQLITE_DB_808);	//打开数据库
	char szSql[512];
	memset(szSql, 0, sizeof(szSql));

	//在数据库中查询数据
	sprintf(szSql, "Select * From answer where answer_ID = %d;", cAnswerID);
	CppSQLite3Query q = db.execQuery(szSql);
	wSerialNum = (WORD)q.getIntField("serial_num");
	
	//释放statement
	q.finalize();
	db.close();	//关闭数据库

	//应答流水号
	wOffset += Int2Str(&cMsgBody[wOffset], wSerialNum);
	//答案ID
	cMsgBody[wOffset++]	= cAnswerID;

	CLogTool::Instance()->WriteLogFile("Report QUESTION_ACK(0x0302)--AnswerID:%d", cAnswerID);
	//打包消息并发送
	BuildFullMsg(MSG_TMN_QUESTION_ACK, cMsgBody, wOffset);

	return TRUE;
}


/************************************************************************
* 函数名:	PackINFO_DEMAND_CANCEL
* 描  述:	22.信息点播/取消0x0303
* 入  参:	1. cInfoType:	信息类型
*			2. cFlag:		0:点播   1:取消
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackINFO_DEMAND_CANCEL(DWORD dwAck_result/*BYTE cInfoType, BYTE cFlag*/)
{
	unCOMMON_DEMAND common_demand;
	common_demand.dwDemand_result = dwAck_result;

	BYTE cInfoType = (BYTE)common_demand.demand_result.wMsgType;
	BYTE cFlag = (BYTE)common_demand.demand_result.wMsgTag;

	if(cFlag != 0 && cFlag != 1)
		return FALSE;

	//打包消息体
	BYTE	cMsgBody[NUM_256B];
	WORD	wOffset	= 0;	//位移
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	//信息类型
	cMsgBody[wOffset++]	= cInfoType;
	//点播/取消
	cMsgBody[wOffset++]	= cFlag;

	CLogTool::Instance()->WriteLogFile("Report INFO_DEMAND_CANCEL(0x0303)--Info type:%d-%d", cInfoType, cFlag);
	//打包消息并发送
	BuildFullMsg(MSG_TMN_INFO_DEMAND_CANCEL, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackCAR_CONTROL_ACK
* 描  述:	27.车辆控制应答0x0500
* 入  参:	
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackCAR_CONTROL_ACK()
{
	//打包消息体
	BYTE	cMsgBody[NUM_1K];
	WORD	wOffset	= 0;	//位移
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	static WORD dMsgSerialNum = 0;
	dMsgSerialNum++;

	//应答流水号
	Int2Str(&cMsgBody[wOffset], dMsgSerialNum);
	wOffset += 2;
	//获取位置信息
	wOffset += PackPosInfo(&cMsgBody[wOffset]);

	CLogTool::Instance()->WriteLogFile("Report CAR_CONTROL_ACK(0x0500)!");
	//打包消息并发送
	BuildFullMsg(MSG_TMN_CAR_CONTROL_ACK, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackDRIVE_RECORD_UPLOAD
* 描  述:	37.行驶记录数据上传0x0700
* 入  参:	1. cCMD:		命令字
*			2. pcDataBlock:	数据块
*			3. wLen:		数据块长度
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackDRIVE_RECORD_UPLOAD(BYTE cCMD, PBYTE pcDataBlock, WORD wLen)
{
	return TRUE;
}

/************************************************************************
* 函数名:	Pack_AWB_UPLoad
* 描  述:	39.电子运单上报(Auto Waybill)0x0701
* 入  参:	1. dwID:	电子运单ID
* 出  参: 
* 返  回: 
* 备  注:	改为从数据库中取数据
************************************************************************/

BOOL CDataPack::Pack_AWB_UPLoad(DWORD dwID)
{
	return TRUE;
}
/************************************************************************
* 函数名:	PackMULTIMEDIA_UPLOAD
* 描  述:	42.多媒体数据上传0x0801
* 入  参:	1. dwID:			多媒体数据ID
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackMULTIMEDIA_UPLOAD(DWORD dwID)
{
	//打包消息体
	BYTE	cMsgBody[32];
	WORD	wOffset	= 0;	//位移
	memset(cMsgBody, NULL, sizeof(cMsgBody));
	CLogTool::Instance()->WriteLogFile(_T("pack PackMULTIMEDIA_UPLOAD"));

	BYTE szPosition[512];

	memset( szPosition, 0, sizeof(szPosition) );

	
	Subcontract_Data subtemp;

	while(!m_Subqueue.empty())//清空队列元素
	{
		m_Subqueue.pop(); 
	}

	//多媒体数据ID
	wOffset += Int2Str(&cMsgBody[wOffset], dwID);
	//多媒体类型
	cMsgBody[wOffset++]	= 0x00;
	cMsgBody[wOffset++]	= 0x00;

	cMsgBody[wOffset++]	= 0x00;
	//通道ID
	cMsgBody[wOffset++]	= 0x01;

	//位置信息
	PackGPSPosInfo(szPosition);
	memcpy(&cMsgBody[wOffset], szPosition, 28 );
	wOffset += 28;

	CString strpath = _T("");
	char szpath[64];
	memset(szpath, 0 ,sizeof(szpath));

	strpath.Format(_T("\\Flashdrv Storage\\%d.jpg"), dwID);

	if (GetFileAttributes(strpath) == 0xFFFFFFFF)
	{
		return FALSE;
	}
	
	
	CFile file(strpath, CFile::modeReadWrite);

	ULONGLONG wFileLen = file.GetLength();

	if (wFileLen < NUM_960B)
	{
		return FALSE;
	}

	WORD  wPackNun = wFileLen/NUM_960B;

	

	if(0 != wFileLen%NUM_960B)
	{
		wPackNun++;
	}
	CLogTool::Instance()->WriteLogFile(_T("Total num:%d"),wPackNun);

	m_wjpgpacknum = wPackNun;

	WORD wbodylen = 0;
	BYTE byMsgBody[NUM_1K];
	char szcontent[NUM_960B];

	WORD wPackserial = 1;
	bool bfirst = true;


	

	while(TRUE)
	{
		memset(szcontent, 0, sizeof(szcontent));
		WORD wReadLen = file.Read(szcontent,sizeof(szcontent));

		if (wReadLen > 0)
		{
			//多媒体数据包
			memset(byMsgBody, 0, sizeof(byMsgBody));
			wbodylen = 0;

			if (bfirst)//第一个包
			{
				bfirst = false;

				memcpy(byMsgBody, cMsgBody, wOffset);
				memcpy(&byMsgBody[wOffset], szcontent, wReadLen);

				wbodylen	= wOffset;
				wbodylen	+= wReadLen;

				memset(subtemp.nData, 0 , sizeof(subtemp.nData));

				subtemp.wBodylen = wbodylen;
				subtemp.wTotalnum = wPackNun;
				subtemp.nSubnum = wPackserial;
				memcpy(subtemp.nData, byMsgBody, wbodylen);

				m_Subqueue.push(subtemp);
			}
			else
			{
				memset(subtemp.nData, 0 , sizeof(subtemp.nData));

				subtemp.wBodylen = wReadLen;
				subtemp.wTotalnum = wPackNun;
				subtemp.nSubnum = wPackserial;
				memcpy(subtemp.nData, szcontent, wReadLen);

				m_Subqueue.push(subtemp);
			}

			wPackserial++;
		}
		else
		{
			break;
		}
	}

	bfirst = true;

	file.Close();

	DeleteFile(strpath);

	UploadPhotoPack();

	
	return TRUE;
}

void CDataPack::UploadPhotoPack()
{
	if (m_Subqueue.empty())
	{
		CLogTool::Instance()->WriteLogFile(_T("photo upload end"));
		return;
	}

	m_bIsSubing = true;
	Subcontract_Data subdata = m_Subqueue.front();


	CLogTool::Instance()->WriteLogFile("%d:%d Report MULTIMEDIA_UPLOAD(0x0801)!", subdata.nSubnum, subdata.wTotalnum);

	//打包消息并发送
	BuildFullMsg(MSG_TMN_MULTIMEDIA_UPLOAD, subdata.nData, subdata.wBodylen, TRUE, subdata.wTotalnum, subdata.nSubnum);

	m_Subqueue.pop();
	m_bIsSubing = false;
}

/************************************************************************
* 函数名:	PackTextSMS
* 描  述:	附1.文本信息上传0x0F01
* 入  参:	
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackTextSMS(DWORD nTextSMS_ID)
{
	//打包消息体
	BYTE	cMsgBody[64];
	WORD	wOffset	= 0;	//位移
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	cMsgBody[wOffset++] = 0x02;

	const WCHAR tcchar[5] = _T("我已收到");
	char bybuff[12];
	memset(bybuff, 0, sizeof(bybuff));

	WideCharToMultiByte(CP_ACP,NULL, tcchar, sizeof(tcchar), bybuff, sizeof(bybuff), NULL,FALSE );

	WORD wlen = strlen(bybuff)+1;

	memcpy(&cMsgBody[wOffset],bybuff,wlen);

	wOffset += wlen;

	//打包消息并发送
	BuildFullMsg(MSG_TMN_TEXT_SMS, cMsgBody, wOffset);

	return TRUE;
}

/************************************************************************
* 函数名:	PackTMN_ATTRIBUTE
* 描  述:	打包查询终端属性
* 入  参:	
* 出  参: 
* 返  回: 
* 备  注: 
************************************************************************/
BOOL CDataPack::PackTMN_ATTRIBUTE(LPCSTR pcManufacturerID, LPCSTR pcTerminalType, 
						  LPCSTR pcTerminalID, LPCSTR pcHardwareVersion, LPCSTR pcFirmwareVersion)
{
	//打包消息体
	BYTE	cMsgBody[128]	= {0};	//数据体
	WORD	wOffset		= 0;		//位移

	//终端类型
	TMN_ATTRIBUTE m_Tmnattribute;
 	m_Tmnattribute.unTerminaltype.Terminaltype_flag.cPassengerCar = 1;			//0:不适用客运车辆,		1:适用客运车辆;
	m_Tmnattribute.unTerminaltype.Terminaltype_flag.cDangerousgoodsCar = 1;		//0:不适用危险品车辆,	1:适用危险品车辆;
	m_Tmnattribute.unTerminaltype.Terminaltype_flag.cGeneralcargo = 1;			//0:不适用普通货运车辆,	1:适用普通货运车辆;
	m_Tmnattribute.unTerminaltype.Terminaltype_flag.cTaxi = 1;					//0:不适用出租车辆,		1:适用出租车辆;
	m_Tmnattribute.unTerminaltype.Terminaltype_flag.charddiskVideo = 0;			//0:不支持硬盘录像,		1:支持硬盘录像;	
	m_Tmnattribute.unTerminaltype.Terminaltype_flag.cAIO = 0;					//0:一体机,				1:分体机.
	wOffset += Int2Str(&cMsgBody[wOffset], m_Tmnattribute.unTerminaltype.dwTerminaltypeFlag);	

	strncpy((LPSTR)&cMsgBody[wOffset], pcManufacturerID, 5);	//制造商ID，5byte
	wOffset += 5;
	strncpy((LPSTR)&cMsgBody[wOffset], pcTerminalType, 20);		//终端型号，8byte
	wOffset += 20;
	strncpy((LPSTR)&cMsgBody[wOffset], pcTerminalID, 7);		//终端ID，7byte
	wOffset += 7;

	cMsgBody[wOffset++] = strlen(pcHardwareVersion);		
	strncpy((LPSTR)&cMsgBody[wOffset], pcHardwareVersion, strlen(pcHardwareVersion));	//终端硬件版本号
	wOffset += strlen(pcHardwareVersion);

	cMsgBody[wOffset++] = strlen(pcFirmwareVersion);		
	strncpy((LPSTR)&cMsgBody[wOffset], pcFirmwareVersion, strlen(pcFirmwareVersion));	//终端固件版本号
	wOffset += strlen(pcFirmwareVersion);

	//GNSS 模块属性
	m_Tmnattribute.unGnssattribute[0] = 1;		//bit0, 0:不支持GPS 定位,		1:支持GPS 定位;
	m_Tmnattribute.unGnssattribute[1] = 0;		//bit1, 0:不支持北斗定位,		1:支持北斗定位;
	m_Tmnattribute.unGnssattribute[2] = 0;		//bit2, 0:不支持GLONASS 定位,	1:支持GLONASS 定位;
	m_Tmnattribute.unGnssattribute[3] = 0;		//bit3, 0:不支持Galileo 定位,	1:支持Galileo 定位.
	strncpy((LPSTR)&cMsgBody[wOffset], (LPSTR)&m_Tmnattribute.unGnssattribute[0], 4);
	wOffset += 4;

	//通信模块属性
	m_Tmnattribute.unModule[0] = 1;		//bit0, 0:不支持GPRS通信,      1:支持GPRS通信;
	m_Tmnattribute.unModule[1] = 0;		//bit1, 0:不支持CDMA通信,      1:支持CDMA通信;
	m_Tmnattribute.unModule[2] = 0;		//bit2, 0:不支持TD-SCDMA通信,  1:支持TD-SCDMA通信;
	m_Tmnattribute.unModule[3] = 0;		//bit3, 0:不支持WCDMA通信,     1:支持WCDMA通信;
	m_Tmnattribute.unModule[4] = 0;		//bit4, 0:不支持CDMA2000通信,  1:支持CDMA2000通信;
	m_Tmnattribute.unModule[5] = 0;		//bit5, 0:不支持TD-LTE通信,    1:支持TD-LTE通信;
	m_Tmnattribute.unModule[6] = 0;		//保留;
	m_Tmnattribute.unModule[7] = 1;		//bit7, 0:不支持其他通信方式,  1:支持其他通信方式.
	strncpy((LPSTR)&cMsgBody[wOffset], (LPSTR)&m_Tmnattribute.unModule[0], 8);
	wOffset += 8;

	//打包消息并发送
	BuildFullMsg(MSG_TMN_ATTRIBUTE_ACK, cMsgBody, wOffset);

	return TRUE;
}

VOID CDataPack::OutsideEvmDetect(BYTE bydetect)
{
//	static bool bNoPower = false;
	switch(bydetect)//00:断电 01:电压正常
	{
	case 0x00:
		g_alarm_state.unAlarm.alarm_flag.cNoExternalPower = 0x01;
		break;
	case 0x01:
		g_alarm_state.unAlarm.alarm_flag.cNoExternalPower = 0x00;
		break;
	default:
		break;
	}
}

VOID CDataPack::SystemPowerOff(WPARAM wparam)
{
	if (0xF1F1 == wparam)//按键关机
	{
		TTSSpeaking(_T("系统即将关闭"));
		Sleep(5000);
	}
	else if (0xF2F2 == wparam)//内部电池低电压关机
	{
		TTSSpeaking(_T("内部电池低电压,系统即将关闭"));
		Sleep(10*1000);
	}

	OnSysPowerOff();
}

VOID CDataPack::OnSysPowerOff()
{
	HANDLE h_handle = NULL;

	h_handle = CreateFile(TEXT("MID0:"), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(h_handle == INVALID_HANDLE_VALUE)
		return;

	if(!DeviceIoControl(h_handle, IOCTRL_SYSTEM_PowrOff, NULL, 0, NULL, 0, NULL, NULL))
		return;

	CloseHandle(h_handle);
	h_handle = NULL;
}

VOID CDataPack::OnAlarm()
{
	g_alarm_state.unAlarm.alarm_flag.cEmergency = 1;
}

BOOL CDataPack::SetillegalDisplace(WORD wdis)
{
	g_illegalDisMent_alarm.dPointLat = g_pGpsPosInfo->rmc_data.dbLatitude;
	g_illegalDisMent_alarm.dPointLong = g_pGpsPosInfo->rmc_data.dbLongitude;
	g_illegalDisMent_alarm.wDis = wdis;
	g_illegalDisMent_alarm.bSet = true;

	return TRUE;
}

BOOL CDataPack::Pack_Upgrade_ACK(BYTE byok)
{
	BYTE ByUpgrade[2];
	memset(ByUpgrade, 0, sizeof(ByUpgrade));

	ByUpgrade[0] = 0x00;
	ByUpgrade[1] = byok;
	CLogTool::Instance()->WriteLogFile("Remote Upgarde(0x0108)");

	//打包消息并发送
	BuildFullMsg(MSG_TMN_UPGRADE_ACK, ByUpgrade, 2);

	return TRUE;
}

void CDataPack::SaveGpsBlindData()
{
	//打包消息体
	BYTE	cMsgBody[65];
	WORD	wOffset	= 0;	//位移
	
	memset(cMsgBody, NULL, sizeof(cMsgBody));

	//添加GPS信息
	wOffset += PackPosInfo(cMsgBody);

	CLogTool::Instance()->WriteLogFile(_T("Save BlindData:"), cMsgBody, wOffset);

	//打开GPS配置文件，单文件不存在就创建
	CFile file;
	file.Open(PATH_BLIND_TXT, CFile::modeCreate | CFile::modeNoTruncate | CFile::modeReadWrite);

	file.SeekToEnd();
	file.Write(cMsgBody,wOffset);
	Sleep(1);
	file.Flush();
	Sleep(1);
	file.Close();
}

void CDataPack::SplitBlindAreaData()
{
	//盲区数据文件大小
	ULONGLONG dwFileSize = 0;
	//发送盲区数据缓冲区
	BYTE bybuff[64];

	if (GetFileAttributes(PATH_BLIND_TXT) == 0xFFFFFFFF)
	{
		return;
	}

	CFile file(PATH_BLIND_TXT, CFile::modeRead);

	//读取盲区文件大小
	dwFileSize = file.GetLength();

	if (dwFileSize == 0)
	{
		file.Close();
		return;
	}

	while(TRUE)
	{
		memset(bybuff, 0, sizeof(bybuff));
		//读取盲区数据内容
		WORD wReadLen = file.Read(bybuff,sizeof(bybuff));
		if (wReadLen > 0)
		{
			Blind_Data blinddata;
			blinddata.wBodylen = wReadLen;
			memcpy(blinddata.nData, bybuff, wReadLen);
			m_BlindStack.push(blinddata);
		}
		else
		{
			break;
		}
		Sleep(1);

	}

	file.Close();

	DeleteFile(PATH_BLIND_TXT);

	SendBlindAreaData();

}

bool CDataPack::SendBlindAreaData()
{
	if (m_BlindStack.empty())
	{
		return false;
	}

	Blind_Data blinddata = m_BlindStack.top();

	CLogTool::Instance()->WriteLogFile(_T("sendblinddata:"), blinddata.nData, blinddata.wBodylen);

	BuildFullMsg(MSG_TMN_POS_REPORT, blinddata.nData, blinddata.wBodylen);

	m_BlindStack.pop();

	return true;
}

void CDataPack::SetSendCountZero()
{
	m_wsendcount = 0;
}