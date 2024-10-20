#include "ql_type.h"
#include "sht20.h"

s32 ret;
void initSHT20()
{
}

u16 readValue(u8 cmd)
{
    u8 mdata[2] = {cmd,0x00 };
    ret = Ql_IIC_Config(0, TRUE, SLAVE_ADDRESS, 100);
    ret = Ql_IIC_Write(0, SLAVE_ADDRESS, mdata, 2);
    u8 toRead;
    u8 counter;
    u8 data[3];

    Ql_Sleep(100);
    ret = Ql_IIC_Config(0, TRUE, SLAVE_ADDRESS, 100);
    ret = Ql_IIC_Read(0, SLAVE_ADDRESS, data, 3);

    u8 msb, lsb, checksum;
    msb = data[0];
    lsb = data[1];
    checksum = data[2];
    u16 rawValue = ((u16)msb << 8) | (u16)lsb;
    if (checkCRC(rawValue, checksum) != 0)
    {
        return (ERROR_BAD_CRC);
    }
    return rawValue & 0xFFFC;
}

float readHumidity(void)
{
    u16 rawHumidity = readValue(TRIGGER_HUMD_MEASURE_NOHOLD);
    if (rawHumidity == ERROR_I2C_TIMEOUT || rawHumidity == ERROR_BAD_CRC)
    {
        return (rawHumidity);
    }
    float tempRH = rawHumidity * (125.0 / 65536.0);
    float rh = tempRH - 6.0;
    return (rh);
}

float readTemperature(void)
{
    u16 rawTemperature = readValue(TRIGGER_TEMP_MEASURE_NOHOLD);
    if (rawTemperature == ERROR_I2C_TIMEOUT || rawTemperature == ERROR_BAD_CRC)
    {
        return (rawTemperature);
    }
    float tempTemperature = rawTemperature * (175.72 / 65536.0);
    float realTemperature = tempTemperature - 46.85;
    return (realTemperature);
}

void setResolution(u8 resolution)
{
    u8 userRegister = readUserRegister();
    userRegister &= 0b01111110;
    resolution &= 0b10000001;
    userRegister |= resolution;
    writeUserRegister(userRegister);
}

u8 readUserRegister(void)
{
    u8 userRegister;
    ret = Ql_IIC_Config(0, TRUE, SLAVE_ADDRESS, 100);
    ret = Ql_IIC_Write_Read(0, SLAVE_ADDRESS, READ_USER_REG, 1, userRegister, 1);
    return (userRegister);
}

void writeUserRegister(u8 val)
{
    u8 data[2];
    data[0] = WRITE_USER_REG;
    data[1] = val;
    ret = Ql_IIC_Config(0, TRUE, SLAVE_ADDRESS, 100);
    ret = Ql_IIC_Write(0, SLAVE_ADDRESS, data, 2);
}

u8 checkCRC(u16 message_from_sensor, u8 check_value_from_sensor)
{
    u32 remainder = (u32)message_from_sensor << 8;
    remainder |= check_value_from_sensor;
    u32 divsor = (u32)SHIFTED_DIVISOR;
    for (int i = 0; i < 16; i++)
    {
        if (remainder & (u32)1 << (23 - i))
        {
            remainder ^= divsor;
        }
        divsor >>= 1;
    }
    return (u8)remainder;
}

void showReslut(const char *prefix, int val)
{
  /*   APP_DEBUG("%s",prefix);
    if (val)
    {
        APP_DEBUG("%s\r\n");
    }
    else
    {
        APP_DEBUG("%s\r\n");
    }  */
}

void checkSHT20(void)
{
    u8 reg = readUserRegister();
    showReslut("End of battery: ", reg & USER_REGISTER_END_OF_BATTERY);
    showReslut("Heater enabled: ", reg & USER_REGISTER_HEATER_ENABLED);
    showReslut("Disable OTP reload: ", reg & USER_REGISTER_DISABLE_OTP_RELOAD);
}