#define __CUSTOMER_CODE__
#ifdef __CUSTOMER_CODE__
#include "custom_feature_def.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"
#include "ql_stdlib.h"
#include "ql_common.h"
#include "ql_type.h"
#include "ril.h"
#include "math.h"
#include "ril_util.h"
#include "ril_telephony.h"
#include "ril_mqtt.h"
#include "ql_stdlib.h"
#include "ql_error.h"
#include "ql_trace.h"
#include "ql_uart.h"
#include "ql_system.h"
#include "ril_network.h"
#include "ril_http.h"
#include "ql_timer.h"   
#include "ql_iic.h"
#include "oled.h"
#include "sht20.h"


#define APN "mcinet\0"
#define USERID ""
#define PASSWD ""


///  MQTT   ////////////////////////////////////////////////////////////////////////
//define process state
typedef enum
{
    STATE_NW_QUERY_STATE,
    STATE_MQTT_CFG,
    STATE_MQTT_OPEN,
    STATE_MQTT_CONN,
    STATE_MQTT_SUB,
    STATE_MQTT_PUB,
    STATE_MQTT_TUNS,
    STATE_MQTT_CLOSE,
    STATE_MQTT_DISC,
    STATE_MQTT_TOTAL_NUM} Enum_ONENETSTATE;
static u8 m_mqtt_state = STATE_NW_QUERY_STATE;
//timer
#define MQTT_TIMER_ID 0x200
#define MQTT_TIMER_PERIOD 500
//param
MQTT_Urc_Param_t *mqtt_urc_param_ptr = NULL;
ST_MQTT_topic_info_t mqtt_topic_info_t;
bool DISC_flag = TRUE;
bool CLOSE_flag = TRUE;

//connect info
Enum_ConnectID connect_id = ConnectID_0;
u8 clientID[] = "mc60tkjtkitk\0";
u8 username[] = "";
u8 passwd[] = "";
//topic and data
u32 pub_message_id = 0;
u32 sub_message_id = 0;
static u8 test_data[128] = "{\"location\":3}\0";            //packet data
static u8 pu_topic[128] = "v1/devices/me/telemetry3\0";  //Publisher  topic
static u8 su_topic[128] = "v1/devices/me/attributes\0"; //Subscriber topic
////////////////////////////////////////////////////////////////////////////////////
//SERVER
#define HOST_NAME "test.mosquitto.org"
#define HOST_PORT 1883

s32 stable = 0;
s32 SENDING_TO_SERVER = 0;
s32 rep = 0;
s32 RIL_STATUS = 0;
s32 iRet = 0;

static u32 Stack_timer = 0x102;
static u32 ST_Interval = 3000;
static s32 m_param1 = 0;
static u32 m_rcvDataLen = 0;

u8 RMC_BUFFER[1000];
u8 postMsg[300] = "";
u8 arrHttpRcvBuf[10 * 1024];

Enum_PinName LED_1 = PINNAME_RI;

#define SERIAL_RX_BUFFER_LEN 2048
char m_RxBuf_Uart[SERIAL_RX_BUFFER_LEN];

#define DEBUG_ENABLE 1
#if DEBUG_ENABLE > 0
#define DEBUG_PORT UART_PORT1
#define DBG_BUF_LEN 512
static char DBG_BUFFER[DBG_BUF_LEN];
#define APP_DEBUG(FORMAT, ...)                                                                                       \
    {                                                                                                                \
        Ql_memset(DBG_BUFFER, 0, DBG_BUF_LEN);                                                                       \
        Ql_sprintf(DBG_BUFFER, FORMAT, ##__VA_ARGS__);                                                               \
        if (UART_PORT2 == (DEBUG_PORT))                                                                              \
        {                                                                                                            \
            Ql_Debug_Trace(DBG_BUFFER);                                                                              \
        }                                                                                                            \
        else                                                                                                         \
        {                                                                                                            \
            Ql_UART_Write((Enum_SerialPort)(DEBUG_PORT), (u8 *)(DBG_BUFFER), Ql_strlen((const char *)(DBG_BUFFER))); \
        }                                                                                                            \
    }
#else
#define APP_DEBUG(FORMAT, ...)
#endif
#define SERIAL_RX_BUFFER_LEN 2048
static Enum_SerialPort m_myUartPort = UART_PORT1;
static u8 m_RxBuf_Uart1[SERIAL_RX_BUFFER_LEN];
static void Timer_handler(u32 timerId, void *param);
static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void *customizedPara);
static s32 ATResponse_Handler(char *line, u32 len, void *userData);
static void Callback_Timer(u32 timerId, void *param);
static void mqtt_recv(u8 *buffer, u32 length);
static void message(u8 *data);

void proc_main_task(s32 taskId)
{
    s32 ret;
    ST_MSG msg;

    Ql_GPIO_Init(LED_1, PINDIRECTION_OUT, PINLEVEL_LOW, PINPULLSEL_PULLUP);
    Ql_GPIO_SetLevel(LED_1, PINLEVEL_LOW);
    APP_DEBUG("START PROGRAM MQTT Client\r\n");
        //<register state timer
    Ql_Timer_Register(MQTT_TIMER_ID, Callback_Timer, NULL);

        //register MQTT recv callback
    ret = Ql_Mqtt_Recv_Register(mqtt_recv);
    APP_DEBUG("//<register recv callback,ret = %d\r\n", ret);

        //init i2c for oled and sht20
    Ql_IIC_Init(0, PINNAME_RI, PINNAME_DCD, TRUE);
    Ql_IIC_Config(0, TRUE, SLAVE_ADDRESS, 300);

        //init oled display
    oledInit();
    oledClear();
    setFont(Adafruit5x7);



    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch (msg.message)
        {
            case MSG_ID_RIL_READY:
                APP_DEBUG("LOAD LEVEL 1 (RIL READY)\r\n");
                Ql_RIL_Initialize();
                RIL_STATUS = 1;
                GPSPower(1);
                break;
            case MSG_ID_URC_INDICATION:
                APP_DEBUG("Received URC: type: %d\r\n", msg.param1);
                switch (msg.param1)
                {
                    case URC_GSM_NW_STATE_IND:
                        APP_DEBUG("GSM Network Status:%d\r\n", msg.param2);
                        break;
                    case URC_SIM_CARD_STATE_IND:
                    {
                        APP_DEBUG("//<SIM Card Status:%d\r\n", msg.param2);
                        if (SIM_STAT_READY == msg.param2)
                        {
                            Ql_Timer_Start(MQTT_TIMER_ID, MQTT_TIMER_PERIOD, TRUE);
                            APP_DEBUG("//<state timer start,ret = %d\r\n", ret);
                        }
                    }
                    break;
                    case URC_MQTT_OPEN:
                    {
                        mqtt_urc_param_ptr = msg.param2;
                        if (0 == mqtt_urc_param_ptr->result)
                        {
                            APP_DEBUG("//<Open a MQTT client successfully\r\n");
                            m_mqtt_state = STATE_MQTT_CONN;
                        }
                        else
                        {
                            APP_DEBUG("//<Open a MQTT client failure,error = %d\r\n", mqtt_urc_param_ptr->result);
                        }
                    }
                    break;
                    case URC_MQTT_CONN:
                    {
                        mqtt_urc_param_ptr = msg.param2;
                        if (0 == mqtt_urc_param_ptr->result)
                        {
                            APP_DEBUG("//<Connect to MQTT server successfully\r\n");
                            m_mqtt_state = STATE_MQTT_SUB;
                        }
                        else
                        {
                            APP_DEBUG("//<Connect to MQTT server failure,error = %d\r\n", mqtt_urc_param_ptr->result);
                        }
                    }
                    break;
                    case URC_MQTT_SUB:
                    {
                        mqtt_urc_param_ptr = msg.param2;
                        if ((0 == mqtt_urc_param_ptr->result) && (128 != mqtt_urc_param_ptr->sub_value[0]))
                        {
                            APP_DEBUG("//<Subscribe topics successfully\r\n");
                            m_mqtt_state = STATE_MQTT_PUB;
                        }
                        else
                        {
                            APP_DEBUG("//<Subscribe topics failure,error = %d\r\n", mqtt_urc_param_ptr->result);
                        }
                    }
                    break;
                    case URC_MQTT_PUB:
                    {
                        mqtt_urc_param_ptr = msg.param2;
                        if (0 == mqtt_urc_param_ptr->result)
                        {
                            APP_DEBUG("//<Publish messages to MQTT server successfully\r\n");
                            m_mqtt_state = STATE_MQTT_TOTAL_NUM;
                        }
                        else
                        {
                            APP_DEBUG("//<Publish messages to MQTT server failure,error = %d\r\n", mqtt_urc_param_ptr->result);
                        }
                    }
                    break;
                    case URC_MQTT_CLOSE:
                    {
                        mqtt_urc_param_ptr = msg.param2;
                        if (0 == mqtt_urc_param_ptr->result)
                        {
                            APP_DEBUG("//<Closed MQTT socket successfully\r\n");
                        }
                        else
                        {
                            APP_DEBUG("//<Closed MQTT socket failure,error = %d\r\n", mqtt_urc_param_ptr->result);
                        }
                    }
                    break;
                    case URC_MQTT_DISC:
                    {
                        mqtt_urc_param_ptr = msg.param2;
                        if (0 == mqtt_urc_param_ptr->result)
                        {
                            APP_DEBUG("//<Disconnect MQTT successfully\r\n");
                        }
                        else
                        {
                            APP_DEBUG("//<Disconnect MQTT failure,error = %d\r\n", mqtt_urc_param_ptr->result);
                        }
                    }
                    break;
                    case URC_GPRS_NW_STATE_IND:
                        APP_DEBUG("GPRS Network Status:%d\r\n", msg.param2);
                        if (NW_STAT_REGISTERED == msg.param2 || NW_STAT_REGISTERED_ROAMING == msg.param2)
                        {
                            APP_DEBUG("LOAD LEVEL 2 (NETWORK REGISTERED)\r\n");
                            stable = 1;
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
}

//Read serial port
void proc_subtask1(s32 TaskId)
{
    s32 ret;
    ST_MSG msg;
    ST_UARTDCB dcb;
    Enum_SerialPort mySerialPort = UART_PORT1;
    dcb.baudrate = 115200;
    dcb.dataBits = DB_8BIT;
    dcb.stopBits = SB_ONE;
    dcb.parity = PB_NONE;
    dcb.flowCtrl = FC_NONE;
    Ql_UART_Register(mySerialPort, CallBack_UART_Hdlr, NULL);
    Ql_UART_OpenEx(mySerialPort, &dcb);
    Ql_UART_ClrRxBuffer(mySerialPort);
    APP_DEBUG("START PROGRAM \r\n");
    while (TRUE)
    {
        Ql_OS_GetMessage(&msg);
        switch (msg.message)
        {
        case MSG_ID_USER_START:
            break;
        default:
            break;
        }
    }
}

//Update location data
proc_subtask2(s32 TaskId)
{
    while (1)
    {
        Ql_Sleep(1000);
        if (RIL_STATUS = 1)
        {
            iRet = RIL_GPS_Read("RMC", RMC_BUFFER);
            if (RIL_AT_SUCCESS != iRet)
            {
                APP_DEBUG("Read %s information failed.\r\n", "RMC");
            }
            else
            {
                if (RMC_BUFFER[30] == 'A')
                {
                    Ql_GPIO_SetLevel(LED_1, PINLEVEL_HIGH);
                    Ql_Sleep(50);
                    Ql_GPIO_SetLevel(LED_1, PINLEVEL_LOW);
                    Ql_Sleep(50);
                    Ql_GPIO_SetLevel(LED_1, PINLEVEL_HIGH);
                    Ql_Sleep(50);
                    Ql_GPIO_SetLevel(LED_1, PINLEVEL_LOW);
                }
                else if (RMC_BUFFER[30] == 'V')
                {
                    Ql_GPIO_SetLevel(LED_1, PINLEVEL_HIGH);
                    Ql_Sleep(50);
                    Ql_GPIO_SetLevel(LED_1, PINLEVEL_LOW);
                }
            }
        }
    }
}
double convert_coordinate(double coordinate) {
    int integerPart = (int)(coordinate / 100);
    int decimalPart = (int)coordinate % 100;

    int degree = integerPart * 60;
    
    char str_decimalPart[20]; // Define a buffer for the decimal part as a string
    Ql_snprintf(str_decimalPart, sizeof(str_decimalPart), "%d", decimalPart);
    int power = 6 - (int)Ql_strlen(str_decimalPart);

    int multipliedDecimalPart = decimalPart * (int)pow(10, power);

    int minutes = multipliedDecimalPart / 10000;
    minutes += degree;

    return (double)minutes / 60.0;
}
// float uchar_array_to_float(unsigned char* uchar_array) {
//     return strtof((const char*)uchar_array, NULL);
// }
// Send location data to server
proc_subtask3(s32 TaskId)
{
    while (1)
    {
        u32 step = 0;
        u32 ret = 0;
        u8 result1[300] = "";
        u8 result2[300] = "";
        char *token;
        u8 *parts[13];
        u8 url[1000]="";
        // if (stable == 1)
        if (m_mqtt_state == STATE_MQTT_TOTAL_NUM && stable == 1)
        {
            step++;
            Ql_strcpy(postMsg, "");
            Ql_strcat(postMsg, RMC_BUFFER);
            Ql_strcat(postMsg, "\0");
            int i = 0;
            token = strtok(postMsg, ",");
            // Split the string into three parts
            while (token != NULL && i < 13) {
                parts[i] = token;
                i++;
                token = strtok(NULL, ",");
            }
            // char a[20];
            // // snprintf(a,sizeof(a),"%u",parts[3]);
            double l1= Ql_atof((const char *)parts[3]);
            double  l2 =Ql_atof((const char *)parts[5]);
            l1 = convert_coordinate(l1);
            l2 = convert_coordinate(l2); 
            char result12[20];
            char result22[20];

            // printf('%f',l1);
            Ql_snprintf(result12, 20, "%f", l1);
            Ql_snprintf(result22, 20, "%f", l2);

            Ql_strcpy(result1, parts[3]);
            Ql_strcpy(result2, parts[5]);
            Ql_strcpy(url, "https://www.google.com/maps/@");
            Ql_strcat(url, result12);
            Ql_strcat(url, ",");
            Ql_strcat(url,result22);
            Ql_strcat(url, ",18z");
            Ql_strcat(url, ",");
            Ql_strcat(url, "?entry=ttu");
            set1X();
            setCursor(0, 3);
            pub_message_id++; // The range is 0-65535. It will be 0 only when<qos>=0.
            ret = RIL_MQTT_QMTPUB(connect_id, pub_message_id, QOS1_AT_LEASET_ONCE, 0, pu_topic, Ql_strlen(url), url);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("//<Start publish a message to server\r\n");
            }
            else
            {
                APP_DEBUG("//<Publish a message to server failure,ret = %d\r\n", ret);
            }
        }
        Ql_Sleep(3000);
    }
}

static s32 ReadSerialPort(Enum_SerialPort port, /*[out]*/ u8 *pBuffer, /*[in]*/ u32 bufLen)
{
    s32 rdLen = 0;
    s32 rdTotalLen = 0;
    if (NULL == pBuffer || 0 == bufLen)
    {
        return -1;
    }
    Ql_memset(pBuffer, 0x0, bufLen);
    while (1)
    {
        rdLen = Ql_UART_Read(port, pBuffer + rdTotalLen, bufLen - rdTotalLen);
        if (rdLen <= 0)
        {
            break;
        }
        rdTotalLen += rdLen;
    }
    return rdTotalLen;
}

void GPSPower(int status)
{
    if (status == 1)
    {
        iRet = RIL_GPS_Open(1);
        if (RIL_AT_SUCCESS != iRet)
        {
            APP_DEBUG("GPS is on \r\n");
        }
        else
        {
            APP_DEBUG("Power on GPS Successful.\r\n");
        }
    }
    else if (status == 0)
    {
        iRet = RIL_GPS_Open(0);
        if (RIL_AT_SUCCESS != iRet)
        {
            APP_DEBUG("GPS is off \r\n");
        }
        else
        {
            APP_DEBUG("Power off GPS Successful.\r\n");
        }
    }
}

static void CallBack_UART_Hdlr(Enum_SerialPort port, Enum_UARTEventType msg, bool level, void *customizedPara)
{
    switch (msg)
    {
    case EVENT_UART_READY_TO_READ:
    {
        char *p = NULL;
        s32 totalBytes = ReadSerialPort(port, m_RxBuf_Uart, sizeof(m_RxBuf_Uart));
        if (totalBytes <= 0)
        {
            break;
        }
        if (Ql_strstr(m_RxBuf_Uart, "GPSOn"))
        {
            APP_DEBUG("ok\r\n");
            GPSPower(1);
            break;
        }
        if (Ql_strstr(m_RxBuf_Uart, "GPSOff"))
        {
            APP_DEBUG("ok\r\n");
            GPSPower(0);
            break;
        }
        if (Ql_strstr(m_RxBuf_Uart, "location"))
        {
            APP_DEBUG("ok\r\n");
            APP_DEBUG("%s \r\n", RMC_BUFFER);
            break;
        }
        break;
    }
    }
}
static void message(u8 *data)
{
    set2X();
    setCursor(10, 0);
    Ql_sprintf(data, "%s          \0", data);
    oledPrint(data);
    APP_DEBUG("message: %s\r\n", data);
}

static void mqtt_recv(u8 *buffer, u32 length)
{
    APP_DEBUG("//<data:%s,len:%d\r\n", buffer, length);
    u16 sec = 0;
    for (u32 i = 0; i < 100; i++)
    {
        if (buffer[i] == ',')
        {
            sec++;
            if (sec == 4)
            {
                u8 data[100] = "";
                for (u32 y = 0; y < 100; y++)
                {

                    data[y] = buffer[i + 10];
                    i++;
                    if (data[y] == '"')
                    {
                        data[y] = '\0';
                        message(data);
                        break;
                        break;
                    }
                }
            }
        }
    }
}
static void Callback_Timer(u32 timerId, void *param)
{
    s32 ret;

    if (MQTT_TIMER_ID == timerId)
    {
        switch (m_mqtt_state)
        {
        case STATE_NW_QUERY_STATE:
        {
            s32 cgreg = 0;
            ret = RIL_NW_GetGPRSState(&cgreg);
            set2X();
            setCursor(10, 0);
            oledPrint("Net check    ");
            APP_DEBUG("//<Network State:cgreg = %d\r\n", cgreg);
            if ((cgreg == NW_STAT_REGISTERED) || (cgreg == NW_STAT_REGISTERED_ROAMING))
            {
                //<Set PDP context 0
                RIL_NW_SetGPRSContext(0);
                APP_DEBUG("//<Set PDP context 0 \r\n");
                //<Set APN
                ret = RIL_NW_SetAPN(1, APN, USERID, PASSWD);
                APP_DEBUG("//<Set APN \r\n");
                set2X();
                setCursor(10, 0);
                oledPrint("Set APN    ");
                //PDP activated
                ret = RIL_NW_OpenPDPContext();
                if (ret == RIL_AT_SUCCESS)
                {
                    set2X();
                    setCursor(10, 0);
                    oledPrint("Act PDP     ");
                    APP_DEBUG("//<Activate PDP context,ret = %d\r\n", ret);
                    m_mqtt_state = STATE_MQTT_CFG;
                }
            }
            break;
        }
        case STATE_MQTT_CFG:
        {
            RIL_MQTT_QMTCFG_Showrecvlen(connect_id, ShowFlag_1); //<This sentence must be configured. The configuration will definitely succeed, so there is no need to care about.
            ret = RIL_MQTT_QMTCFG_Version_Select(connect_id, Version_3_1_1);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("//<Select version 3.1.1 successfully\r\n");
                m_mqtt_state = STATE_MQTT_OPEN;
            }
            else
            {
                APP_DEBUG("//<Select version 3.1.1 failure,ret = %d\r\n", ret);
            }
            break;
        }
        case STATE_MQTT_OPEN:
        {
            ret = RIL_MQTT_QMTOPEN(connect_id, HOST_NAME, HOST_PORT);
            if (RIL_AT_SUCCESS == ret)
            {
                set2X();
                setCursor(10, 0);
                oledPrint("open MQTT      ");
                APP_DEBUG("//<Start opening a MQTT client\r\n");
                if (FALSE == CLOSE_flag)
                    CLOSE_flag = TRUE;
                m_mqtt_state = STATE_MQTT_TOTAL_NUM;
            }
            else
            {
                APP_DEBUG("//<Open a MQTT client failure,ret = %d-->\r\n", ret);
            }
            break;
        }
        case STATE_MQTT_CONN:
        {
            ret = RIL_MQTT_QMTCONN(connect_id, clientID, username, passwd);
            if (RIL_AT_SUCCESS == ret)
            {
                set2X();
                setCursor(10, 0);
                oledPrint("con MQTT      ");
                APP_DEBUG("//<Start connect to MQTT server\r\n");
                if (FALSE == DISC_flag)
                    DISC_flag = TRUE;
                m_mqtt_state = STATE_MQTT_TOTAL_NUM;
            }
            else
            {
                APP_DEBUG("//<connect to MQTT server failure,ret = %d\r\n", ret);
            }
            break;
        }
        case STATE_MQTT_SUB:
        {
            mqtt_topic_info_t.count = 1;
            mqtt_topic_info_t.topic[0] = (u8 *)Ql_MEM_Alloc(sizeof(u8) * 256);

            Ql_memset(mqtt_topic_info_t.topic[0], 0, 256);
            Ql_memcpy(mqtt_topic_info_t.topic[0], su_topic, Ql_strlen(su_topic));
            mqtt_topic_info_t.qos[0] = QOS1_AT_LEASET_ONCE;
            sub_message_id++; //< 1-65535.

            ret = RIL_MQTT_QMTSUB(connect_id, sub_message_id, &mqtt_topic_info_t);

            Ql_MEM_Free(mqtt_topic_info_t.topic[0]);
            mqtt_topic_info_t.topic[0] = NULL;
            if (RIL_AT_SUCCESS == ret)
            {
                set2X();
                setCursor(10, 0);
                oledPrint("sub MQTT     ");
                APP_DEBUG("//<Start subscribe topic\r\n");
                m_mqtt_state = STATE_MQTT_TOTAL_NUM;
            }
            else
            {
                APP_DEBUG("//<Subscribe topic failure,ret = %d\r\n", ret);
            }
            break;
        }
        case STATE_MQTT_PUB:
        {
            pub_message_id++; //< The range is 0-65535. It will be 0 only when<qos>=0.
            ret = RIL_MQTT_QMTPUB(connect_id, pub_message_id, QOS1_AT_LEASET_ONCE, 0, pu_topic, Ql_strlen(test_data), test_data);
            if (RIL_AT_SUCCESS == ret)
            {
                APP_DEBUG("//<Start publish a message to MQTT server\r\n");
                m_mqtt_state = STATE_MQTT_TOTAL_NUM;
            }
            else
            {
                APP_DEBUG("//<Publish a message to MQTT server failure,ret = %d\r\n", ret);
            }
            break;
        }
        case STATE_MQTT_TOTAL_NUM:
        {
            break;
        }
        default:
            break;
        }
    }
}
#endif // __CUSTOMER_CODE__
