#ifndef DFRobot_SHT20_h
#define DFRobot_SHT20_h


#define ERROR_I2C_TIMEOUT                     998
#define ERROR_BAD_CRC                         999
#define SLAVE_ADDRESS                         0x80 
#define TRIGGER_TEMP_MEASURE_HOLD             0xE3
#define TRIGGER_HUMD_MEASURE_HOLD             0xE5
#define TRIGGER_TEMP_MEASURE_NOHOLD           0xF3
#define TRIGGER_HUMD_MEASURE_NOHOLD           0xF5
#define WRITE_USER_REG                        0xE6
#define READ_USER_REG                         0xE7
#define SOFT_RESET                            0xFE
#define USER_REGISTER_RESOLUTION_MASK         0x81
#define USER_REGISTER_RESOLUTION_RH12_TEMP14  0x00
#define USER_REGISTER_RESOLUTION_RH8_TEMP12   0x01
#define USER_REGISTER_RESOLUTION_RH10_TEMP13  0x80
#define USER_REGISTER_RESOLUTION_RH11_TEMP11  0x81
#define USER_REGISTER_END_OF_BATTERY          0x40
#define USER_REGISTER_HEATER_ENABLED          0x04
#define USER_REGISTER_DISABLE_OTP_RELOAD      0x02
#define MAX_WAIT                              100
#define DELAY_INTERVAL                        10
#define SHIFTED_DIVISOR                       0x988000
#define MAX_COUNTER                           (MAX_WAIT/DELAY_INTERVAL)


    void     checkSHT20(void);
    void     setResolution(u8 resBits);
    void     writeUserRegister(u8 val);
    void     initSHT20();
    void     showReslut(const char *prefix, int val);
    float    readHumidity(void);
    float    readTemperature(void);
    u8     readUserRegister(void);

    u8     checkCRC(u16 message_from_sensor, u8 check_value_from_sensor);
    u16 readValue(u8 cmd);

#endif