// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

#include <Wire.h>
#include "RTClibExtended.h"

#ifdef __AVR__

#include <avr/pgmspace.h>

#elif defined(ESP8266)
#include <pgmspace.h>
#elif defined(ARDUINO_ARCH_SAMD)
// nothing special needed
#elif defined(ARDUINO_SAM_DUE)
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define Wire Wire1
#endif

#if (ARDUINO >= 100)

#include <Arduino.h> // capital A so it is error prone on case-sensitive filesystems
// Macro to deal with the difference in I2C write functions from old and new Arduino versions.
#define _I2C_WRITE write
#define _I2C_READ  read
#else
#include <WProgram.h>
#define _I2C_WRITE send
#define _I2C_READ  receive
#endif

/**
 * @brief Read information from a device's register
 * @param addr The device address  on the I2C bus
 * @param reg The register
 * @return The byte value (unsigned) of the register
 */
static uint8_t read_i2c_register(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire._I2C_WRITE((byte) reg);
    Wire.endTransmission();

    Wire.requestFrom(addr, (byte) 1);
    return Wire._I2C_READ();
}

/**
 * @brief Write a byte value to a device's register
 * @param addr The device address on the I2C bus
 * @param reg The register
 * @param val The value to write
 * @see read_i2c_register
 */
static void write_i2c_register(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire._I2C_WRITE((byte) reg);
    Wire._I2C_WRITE((byte) val);
    Wire.endTransmission();
}

////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

const uint8_t daysInMonth[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
    if (y >= 2000)
        y -= 2000;
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i) {
        days += pgm_read_byte(daysInMonth + i - 1);
        //days += pgm_read_byte(daysInMonth + i - 1);
    }
    if (m > 2 && y % 4 == 0)
        ++days;
    return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
    return ((days * 24L + h) * 60 + m) * 60 + s;
}

////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime(uint32_t t) {
    t -= SECONDS_FROM_1970_TO_2000;    // bring to 2000 timestamp from 1970

    ss = t % 60;
    t /= 60;
    mm = t % 60;
    t /= 60;
    hh = t % 24;
    uint16_t days = t / 24;
    uint16_t leap;
    for (yOff = 0;; ++yOff) {
        leap = yOff % 4 == 0;
        if (days < 365 + leap)
            break;
        days -= 365 + leap;
    }
    for (m = 1;; ++m) {
        uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        // uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
        if (leap && m == 2)
            ++daysPerMonth;
        if (days < daysPerMonth)
            break;
        days -= daysPerMonth;
    }
    d = days + 1;
}

DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
    if (year >= 2000)
        year -= 2000;
    yOff = year;
    m = month;
    d = day;
    hh = hour;
    mm = min;
    ss = sec;
}

DateTime::DateTime(const DateTime &copy) :
        yOff(copy.yOff),
        m(copy.m),
        d(copy.d),
        hh(copy.hh),
        mm(copy.mm),
        ss(copy.ss) {}

static uint8_t conv2d(const char *p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using F() would further reduce the RAM footprint, see below.
DateTime::DateTime(const char *date, const char *time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    yOff = conv2d(date + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec 
    switch (date[0]) {
        case 'J':
            if (date[1] == 'a')
                m = 1;
            else {
                if (date[2] == 'n')
                    m = 6;
                else
                    m = 7;
            }
            // m = date[1] == 'a' ? 1 : m = date[2] == 'n' ? 6 : 7;
            break;
        case 'F':
            m = 2;
            break;
        case 'A':
            m = date[2] == 'r' ? 4 : 8;
            break;
        case 'M':
            m = date[2] == 'r' ? 3 : 5;
            break;
        case 'S':
            m = 9;
            break;
        case 'O':
            m = 10;
            break;
        case 'N':
            m = 11;
            break;
        case 'D':
            m = 12;
            break;
    }
    d = conv2d(date + 4);
    hh = conv2d(time);
    mm = conv2d(time + 3);
    ss = conv2d(time + 6);
}

// A convenient constructor for using "the compiler's time":
// This version will save RAM by using PROGMEM to store it by using the F macro.
//   DateTime now (F(__DATE__), F(__TIME__));
DateTime::DateTime(const __FlashStringHelper *date, const __FlashStringHelper *time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    char buff[11];
    memcpy_P(buff, date, 11);
    yOff = conv2d(buff + 9);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (buff[0]) {
        case 'J':
            if (buff[1] == 'a')
                m = 1;
            else {
                if (buff[2] == 'n')
                    m = 6;
                else
                    m = 7;
            }
            // m = buff[1] == 'a' ? 1 : m = buff[2] == 'n' ? 6 : 7;
            break;
        case 'F':
            m = 2;
            break;
        case 'A':
            m = buff[2] == 'r' ? 4 : 8;
            break;
        case 'M':
            m = buff[2] == 'r' ? 3 : 5;
            break;
        case 'S':
            m = 9;
            break;
        case 'O':
            m = 10;
            break;
        case 'N':
            m = 11;
            break;
        case 'D':
            m = 12;
            break;
    }
    d = conv2d(buff + 4);
    memcpy_P(buff, time, 8);
    hh = conv2d(buff);
    mm = conv2d(buff + 3);
    ss = conv2d(buff + 6);
}

uint8_t DateTime::dayOfTheWeek() const {
    uint16_t day = date2days(yOff, m, d);
    return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::unixtime(void) const {
    uint32_t t;
    uint16_t days = date2days(yOff, m, d);
    t = time2long(days, hh, mm, ss);
    t += SECONDS_FROM_1970_TO_2000;  // seconds from 1970 to 2000

    return t;
}

long DateTime::secondstime(void) const {
    long t;
    uint16_t days = date2days(yOff, m, d);
    t = time2long(days, hh, mm, ss);
    return t;
}

DateTime DateTime::operator+(const TimeSpan &span) {
    return DateTime(unixtime() + span.totalseconds());
}

DateTime DateTime::operator-(const TimeSpan &span) {
    return DateTime(unixtime() - span.totalseconds());
}

TimeSpan DateTime::operator-(const DateTime &right) {
    return TimeSpan(unixtime() - right.unixtime());
}

////////////////////////////////////////////////////////////////////////////////
// TimeSpan implementation

TimeSpan::TimeSpan(int32_t seconds) :
        _seconds(seconds) {}

TimeSpan::TimeSpan(int16_t days, int8_t hours, int8_t minutes, int8_t seconds) :
        _seconds((int32_t) days * 86400L + (int32_t) hours * 3600 + (int32_t) minutes * 60 + seconds) {}

TimeSpan::TimeSpan(const TimeSpan &copy) :
        _seconds(copy._seconds) {}

TimeSpan TimeSpan::operator+(const TimeSpan &right) {
    return TimeSpan(_seconds + right._seconds);
}

TimeSpan TimeSpan::operator-(const TimeSpan &right) {
    return TimeSpan(_seconds - right._seconds);
}

////////////////////////////////////////////////////////////////////////////////
// RTC_DS1307 implementation

static uint8_t bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }

static uint8_t bin2bcd(uint8_t val) { return val + 6 * (val / 10); }

boolean RTC_DS1307::begin(void) {
    Wire.begin();
    return true;
}

uint8_t RTC_DS1307::isrunning(void) {
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE((byte) 0);
    Wire.endTransmission();

    Wire.requestFrom(DS1307_ADDRESS, 1);
    uint8_t ss = Wire._I2C_READ();
    return !(ss >> 7);
}

void RTC_DS1307::adjust(const DateTime &dt) {
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE((byte) 0); // start at location 0
    Wire._I2C_WRITE(bin2bcd(dt.second()));
    Wire._I2C_WRITE(bin2bcd(dt.minute()));
    Wire._I2C_WRITE(bin2bcd(dt.hour()));
    Wire._I2C_WRITE(bin2bcd(0));
    Wire._I2C_WRITE(bin2bcd(dt.day()));
    Wire._I2C_WRITE(bin2bcd(dt.month()));
    Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
    Wire.endTransmission();
}

DateTime RTC_DS1307::now() {
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE((byte) 0);
    Wire.endTransmission();

    Wire.requestFrom(DS1307_ADDRESS, 7);
    uint8_t ss = bcd2bin(Wire._I2C_READ() & 0x7F);
    uint8_t mm = bcd2bin(Wire._I2C_READ());
    uint8_t hh = bcd2bin(Wire._I2C_READ());
    Wire._I2C_READ();
    uint8_t d = bcd2bin(Wire._I2C_READ());
    uint8_t m = bcd2bin(Wire._I2C_READ());
    uint16_t y = bcd2bin(Wire._I2C_READ()) + 2000;

    return DateTime(y, m, d, hh, mm, ss);
}

Ds1307SqwPinMode RTC_DS1307::readSqwPinMode() {
    int mode;

    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE(DS1307_CONTROL);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS1307_ADDRESS, (uint8_t) 1);
    mode = Wire._I2C_READ();

    mode &= 0x93;
    return static_cast<Ds1307SqwPinMode>(mode);
}

void RTC_DS1307::writeSqwPinMode(Ds1307SqwPinMode mode) {
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE(DS1307_CONTROL);
    Wire._I2C_WRITE(mode);
    Wire.endTransmission();
}

void RTC_DS1307::readnvram(uint8_t *buf, uint8_t size, uint8_t address) {
    int addrByte = DS1307_NVRAM + address;
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE(addrByte);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS1307_ADDRESS, size);
    for (uint8_t pos = 0; pos < size; ++pos) {
        buf[pos] = Wire._I2C_READ();
    }
}

void RTC_DS1307::writenvram(uint8_t address, uint8_t *buf, uint8_t size) {
    int addrByte = DS1307_NVRAM + address;
    Wire.beginTransmission(DS1307_ADDRESS);
    Wire._I2C_WRITE(addrByte);
    for (uint8_t pos = 0; pos < size; ++pos) {
        Wire._I2C_WRITE(buf[pos]);
    }
    Wire.endTransmission();
}

uint8_t RTC_DS1307::readnvram(uint8_t address) {
    uint8_t data;
    readnvram(&data, 1, address);
    return data;
}

void RTC_DS1307::writenvram(uint8_t address, uint8_t data) {
    writenvram(address, &data, 1);
}

////////////////////////////////////////////////////////////////////////////////
// RTC_Millis implementation

long RTC_Millis::offset = 0;

void RTC_Millis::adjust(const DateTime &dt) {
    offset = dt.unixtime() - millis() / 1000;
}

DateTime RTC_Millis::now() {
    return (uint32_t) (offset + millis() / 1000);
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// RTC_PCF8563 implementation

boolean RTC_PCF8523::begin(void) {
    Wire.begin();
    return true;
}

boolean RTC_PCF8523::initialized(void) {
    Wire.beginTransmission(PCF8523_ADDRESS);
    Wire._I2C_WRITE((byte) PCF8523_CONTROL_3);
    Wire.endTransmission();

    Wire.requestFrom(PCF8523_ADDRESS, 1);
    uint8_t ss = Wire._I2C_READ();
    return ((ss & 0xE0) != 0xE0);
}

void RTC_PCF8523::adjust(const DateTime &dt) {
    Wire.beginTransmission(PCF8523_ADDRESS);
    Wire._I2C_WRITE((byte) 3); // start at location 3
    Wire._I2C_WRITE(bin2bcd(dt.second()));
    Wire._I2C_WRITE(bin2bcd(dt.minute()));
    Wire._I2C_WRITE(bin2bcd(dt.hour()));
    Wire._I2C_WRITE(bin2bcd(dt.day()));
    Wire._I2C_WRITE(bin2bcd(0)); // skip weekdays
    Wire._I2C_WRITE(bin2bcd(dt.month()));
    Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
    Wire.endTransmission();

    // set to battery switchover mode
    Wire.beginTransmission(PCF8523_ADDRESS);
    Wire._I2C_WRITE((byte) PCF8523_CONTROL_3);
    Wire._I2C_WRITE((byte) 0x00);
    Wire.endTransmission();
}

DateTime RTC_PCF8523::now() {
    Wire.beginTransmission(PCF8523_ADDRESS);
    Wire._I2C_WRITE((byte) 3);
    Wire.endTransmission();

    Wire.requestFrom(PCF8523_ADDRESS, 7);
    uint8_t ss = bcd2bin(Wire._I2C_READ() & 0x7F);
    uint8_t mm = bcd2bin(Wire._I2C_READ());
    uint8_t hh = bcd2bin(Wire._I2C_READ());
    uint8_t d = bcd2bin(Wire._I2C_READ());
    Wire._I2C_READ();  // skip 'weekdays'
    uint8_t m = bcd2bin(Wire._I2C_READ());
    uint16_t y = bcd2bin(Wire._I2C_READ()) + 2000;

    return DateTime(y, m, d, hh, mm, ss);
}

Pcf8523SqwPinMode RTC_PCF8523::readSqwPinMode() {
    int mode;

    Wire.beginTransmission(PCF8523_ADDRESS);
    Wire._I2C_WRITE(PCF8523_CLKOUTCONTROL);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) PCF8523_ADDRESS, (uint8_t) 1);
    mode = Wire._I2C_READ();

    mode >>= 3;
    mode &= 0x7;
    return static_cast<Pcf8523SqwPinMode>(mode);
}

void RTC_PCF8523::writeSqwPinMode(Pcf8523SqwPinMode mode) {
    Wire.beginTransmission(PCF8523_ADDRESS);
    Wire._I2C_WRITE(PCF8523_CLKOUTCONTROL);
    Wire._I2C_WRITE(mode << 3);
    Wire.endTransmission();
}

////////////////////////////////////////////////////////////////////////////////
// RTC_DS3231 implementation

boolean RTC_DS3231::begin(void) {
    Wire.begin();
    return true;
}

bool RTC_DS3231::lostPower(void) {
    return (read_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG) >> 7);
}

void RTC_DS3231::adjust(const DateTime &dt) {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire._I2C_WRITE((byte) 0); // start at location 0
    Wire._I2C_WRITE(bin2bcd(dt.second()));
    Wire._I2C_WRITE(bin2bcd(dt.minute()));
    Wire._I2C_WRITE(bin2bcd(dt.hour()));
    Wire._I2C_WRITE(bin2bcd(0));
    Wire._I2C_WRITE(bin2bcd(dt.day()));
    Wire._I2C_WRITE(bin2bcd(dt.month()));
    Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
    Wire.endTransmission();

    uint8_t statreg = read_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG);
    statreg &= ~0x80; // flip OSF bit
    write_i2c_register(DS3231_ADDRESS, DS3231_STATUSREG, statreg);
}

DateTime RTC_DS3231::now() {
    Wire.beginTransmission(DS3231_ADDRESS);
    Wire._I2C_WRITE((byte) 0);
    Wire.endTransmission();

    Wire.requestFrom(DS3231_ADDRESS, 7);
    uint8_t ss = bcd2bin(Wire._I2C_READ() & 0x7F);
    uint8_t mm = bcd2bin(Wire._I2C_READ());
    uint8_t hh = bcd2bin(Wire._I2C_READ());
    Wire._I2C_READ();
    uint8_t d = bcd2bin(Wire._I2C_READ());
    uint8_t m = bcd2bin(Wire._I2C_READ());
    uint16_t y = bcd2bin(Wire._I2C_READ()) + 2000;

    return DateTime(y, m, d, hh, mm, ss);
}

/**
 * @brief Access the DS3231 Control register
 *
 * Read the control register value and mask it so that only the values for
 * xxxx xxxx xxxx RS2  RS1 INTCN xxxx xxxx  are returned (elided values shown
 * by 'xxxx'). The full set of control values is:
 * EOSC BBSQW CONV RS2   RS1 INTCN A2IE A1IE
 *
 * @return The Ds3231SqwPinMode
 */
Ds3231SqwPinMode RTC_DS3231::readSqwPinMode() {
    int mode;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire._I2C_WRITE(DS3231_CONTROL);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
    mode = Wire._I2C_READ();

    //mode &= 0x93;//bug due to using ds1307 read mask
    mode &= 0x1C;
    if (mode == 0x04)
        mode = DS3231_OFF;
    return static_cast<Ds3231SqwPinMode>(mode);
#if 0
    mode &= 0x93; // b 1001 0011
    return static_cast<Ds3231SqwPinMode>(mode);
#endif
}

/**
 * @brief Control the INT/SQW pin mode
 *
 * If the mode is DS3231_OFF, then pin 3 (INT/SQW) is set to interrupt
 * output mode (INTCON is set). If it is one of the other values
 * DS3231_SquareWave1Hz, ..., DS3231_SquareWave8kHz) then the pin outputs
 * a square wave (INTCON is cleared).
 *
 * @note if the interrupt output is to work when the DS3231 is in
 * battery backup mode, the BBSQW bit must also be set.
 *
 * @param mode One of Ds3231SqwPinMode
 */
void RTC_DS3231::writeSqwPinMode(Ds3231SqwPinMode mode) {
    uint8_t ctrl;
    ctrl = read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL);

    ctrl &= ~0x04; // clear INTCON
    ctrl &= ~0x18; // clear freq bits (b 0001 1000)

    if (mode == DS3231_OFF) {
        ctrl |= 0x04; // set INTCN
    } else {
        ctrl |= mode;
    }
    write_i2c_register(DS3231_ADDRESS, DS3231_CONTROL, ctrl);

    //Serial.println( read_i2c_register(DS3231_ADDRESS, DS3231_CONTROL), HEX);
}

/*----------------------------------------------------------------------*/

float RTC_DS3231::getTemp() {
    int8_t temp_msb, temp_lsb;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_TEMP);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 2);
    temp_msb = Wire._I2C_READ();
    temp_lsb = (Wire._I2C_READ() >> 6) & 0x03;
    Wire.endTransmission();

    if (temp_msb & 0b10000000) {     //check if negative number
        temp_msb ^= 0b11111111;
        temp_msb += 0x1;
        return (-1.0 * ((float) temp_msb) + ((float) temp_lsb * 0.25));
    } else {
        return ((float) temp_msb + ((float) temp_lsb * 0.25));
    }
}

/**
 * @brief Test the status of the EN32kHz bit of the control/status register
 *
 *  When set to logic 1, pin 1 is enabled and outputs a 32.768kHz
 *  square wave signal. When set to logic 0, pin 1 goes to a
 *  high-impedance state.
 *
 * @return True if pin 1 is set to output a 32kHz square wave, false if not
 */
bool RTC_DS3231::getEN32kHz() {
    //void write(byte addr, byte value);
    //byte read(byte addr);

#if 0
    byte _byteValue = read(DS3231_STATUSREG);

    if (_byteValue & DS3231_EN32kHz) {
        return (true);
    } else {
        return (false);
    }
#endif

    return read(DS3231_STATUSREG) & DS3231_EN32kHz;
}

/**
 * @brief Enable 32kHz Output (EN32kHz) on pin 1.
 *
 * @param Enable True if the square wave should be output on pin 1,
 * false if not.
 * @return The actual value of the status register; AND with DS3231_EN32kHz
 * to get the state of the bit.
 *
 * @note If this control bit is cleared, pin 1 will not output the 32kHz square
 * wave and will go to high impedance instead. Setting this to high impedance
 * reduces battery-backed power use.
 */
byte RTC_DS3231::setEN32kHz(bool Enable) {
    byte _byteValue = read(DS3231_STATUSREG);

    if (Enable) {
        // Set the bit to enable 32kHz output on pin 1
        _byteValue |= DS3231_EN32kHz;
    } else {
        // Clear the bit to enable 32kHz output on pin 1
        _byteValue &= ~DS3231_EN32kHz;
    }

    write(DS3231_CONTROL, _byteValue);
    return _byteValue;
}

/**
 * @brief Test the status of the BBSQW bit of the control register
 *
 *  When set to logic 1, pin 3 will output a square wave or interrupt when
 *  the DS3231 is powered by the battery backup; when set to logic 0, it
 *  will not.
 *
 * @return True if BBSQW is set, false if not
 */
bool RTC_DS3231::getBBSQW(void) {
    //void write(byte addr, byte value);
    //byte read(byte addr);
#if 0
    byte _byteValue = read(DS3231_CONTROL);

    if (_byteValue & DS3231_BBSQW) {
        return (true);
    } else {
        return (false);
    }
#endif

    return read(DS3231_CONTROL) & DS3231_BBSQW;
}

/**
 * @brief Set BBSQW
 *
 * @param Enable True sets the BBSQW bit of the CONTROL register, False
 * clears it.
 *
 * @note Setting BBSQW is needed to generate an interrupt (pin 3) when on battery
 * backup power. Setting it when pin 3 is used for a square wave will consume
 * more power when battery backed.
 */
byte RTC_DS3231::setBBSQW(bool Enable) {
    byte _byteValue = read(DS3231_CONTROL);

    if (Enable) {
        // Set the bit to enable 32kHz output on pin 1
        _byteValue |= DS3231_BBSQW;
    } else {
        // Clear the bit to enable 32kHz output on pin 1
        _byteValue &= ~DS3231_BBSQW;
    }

    write(DS3231_STATUSREG, _byteValue);
    return _byteValue;
}

/*----------------------------------------------------------------------*
 * Enable or disable an alarm "interrupt" which asserts the INT pin     *
 * on the RTC.                                                          *
 *----------------------------------------------------------------------*/
void RTC_DS3231::alarmInterrupt(byte alarmNumber, bool interruptEnabled) {
    uint8_t controlReg, mask;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    controlReg = Wire.endTransmission();
    if (!controlReg) {
        Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
        controlReg = Wire._I2C_READ();
        Wire.endTransmission();
    }

    mask = _BV(A1IE) << (alarmNumber - 1);
    if (interruptEnabled)
        controlReg |= mask;
    else
        controlReg &= ~mask;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.write(controlReg);
    Wire.endTransmission();
}

/*----------------------------------------------------------------------*
 * Set an alarm time. Sets the alarm registers only.  To cause the      *
 * INT pin to be asserted on alarm match, use alarmInterrupt().         *
 * This method can set either Alarm 1 or Alarm 2, depending on the      *
 * value of alarmType (use a value from the ALARM_TYPES_t enumeration). *
 * When setting Alarm 2, the seconds value must be supplied but is      *
 * ignored, recommend using zero. (Alarm 2 has no seconds register.)    *
 *----------------------------------------------------------------------*/
void RTC_DS3231::setAlarm(Ds3231_ALARM_TYPES_t alarmType, byte seconds, byte minutes, byte hours, byte daydate) {

    uint8_t addr;
    byte alarmNumber;

    seconds = bin2bcd(seconds);
    minutes = bin2bcd(minutes);
    hours = bin2bcd(hours);
    daydate = bin2bcd(daydate);
    if (alarmType & 0x01) seconds |= _BV(A1M1);
    if (alarmType & 0x02) minutes |= _BV(A1M2);
    if (alarmType & 0x04) hours |= _BV(A1M3);
    if (alarmType & 0x10) hours |= _BV(DYDT);
    if (alarmType & 0x08) daydate |= _BV(A1M4);

    if (!(alarmType & 0x80)) {    //alarm 1
        alarmNumber = 1;
        addr = ALM1_SECONDS;
        Wire.beginTransmission(DS3231_ADDRESS);
        Wire.write(addr++);
        Wire.write(seconds);
        Wire.endTransmission();
    } else {
        alarmNumber = 2;
        addr = ALM2_MINUTES;      //alarm 2
    }

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(addr++);
    Wire.write(minutes);
    Wire.endTransmission();

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(addr++);
    Wire.write(hours);
    Wire.endTransmission();

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(addr++);
    Wire.write(daydate);
    Wire.endTransmission();

    armAlarm(alarmNumber, true);
    clearAlarm(alarmNumber);
}

/*----------------------------------------------------------------------*
 * Set an alarm time. Sets the alarm registers only.  To cause the      *
 * INT pin to be asserted on alarm match, use alarmInterrupt().         *
 * This method can set either Alarm 1 or Alarm 2, depending on the      *
 * value of alarmType (use a value from the ALARM_TYPES_t enumeration). *
 * However, when using this method to set Alarm 1, the seconds value    *
 * is set to zero. (Alarm 2 has no seconds register.)                   *
 *----------------------------------------------------------------------*/
void RTC_DS3231::setAlarm(Ds3231_ALARM_TYPES_t alarmType, byte minutes, byte hours, byte daydate) {
    setAlarm(alarmType, 0, minutes, hours, daydate);
}

/*----------------------------------------------------------------------*
 * This method arms or disarms Alarm 1 or Alarm 2, depending on the     *
 * value of alarmNumber (1 or 2) and arm (true or false).               *
 *----------------------------------------------------------------------*/
void RTC_DS3231::armAlarm(byte alarmNumber, bool armed) {
    uint8_t value, mask;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
    value = Wire._I2C_READ();
    Wire.endTransmission();

    mask = _BV(alarmNumber - 1);
    if (armed) {
        value |= mask;
    } else {
        value &= ~mask;
    }

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.write(value);
    Wire.endTransmission();
}

/*----------------------------------------------------------------------*
 * This method clears the status register of Alarm 1 or Alarm 2,        *
 * depending on the value of alarmNumber (1 or 2).                      *
 *----------------------------------------------------------------------*/
void RTC_DS3231::clearAlarm(byte alarmNumber) {
    uint8_t value, mask;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_STATUSREG);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
    value = Wire._I2C_READ();
    Wire.endTransmission();

    mask = _BV(alarmNumber - 1);
    value &= ~mask;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_STATUSREG);
    Wire.write(value);
    Wire.endTransmission();
}

/*----------------------------------------------------------------------*
 * This method can check either Alarm 1 or Alarm 2, depending on the    *
 * value of alarmNumber (1 or 2).                                       *
 *----------------------------------------------------------------------*/
bool RTC_DS3231::isArmed(byte alarmNumber) {
    uint8_t value;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
    value = Wire._I2C_READ();
    Wire.endTransmission();

    if (alarmNumber == 1) {
        value &= 0b00000001;
    } else {
        value &= 0b00000010;
        value >>= 1;
    }
    return value;
}

/*----------------------------------------------------------------------*
 * This method writes a single byte in RTC memory                       *
 * Valid address range is 0x00 - 0x12, no checking.                     *
 *----------------------------------------------------------------------*/
void RTC_DS3231::write(byte addr, byte value) {

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(addr);
    Wire.write(value);
    Wire.endTransmission();
}

/*----------------------------------------------------------------------*
 * This method reads a single byte from RTC memory                      *
 * Valid address range is 0x00 - 0x12, no checking.                     *
 *----------------------------------------------------------------------*/
byte RTC_DS3231::read(byte addr) {
    uint8_t value;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(addr);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
    value = Wire._I2C_READ();
    Wire.endTransmission();

    return value;
}

/*----------------------------------------------------------------------*
 * The temperature registers are updated after every 64-second          *
 * conversion. If you want force temperature conversion call this       *
 * function.                                                            *
 *----------------------------------------------------------------------*/
void RTC_DS3231::forceConversion(void) {
    uint8_t value;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
    value = Wire._I2C_READ();
    Wire.endTransmission();

    value |= 0b00100000;

    Wire.beginTransmission(DS3231_ADDRESS);
    Wire.write(DS3231_CONTROL);
    Wire.write(value);
    Wire.endTransmission();

    do {
        Wire.beginTransmission(DS3231_ADDRESS);
        Wire.write(DS3231_CONTROL);
        Wire.endTransmission();

        Wire.requestFrom((uint8_t) DS3231_ADDRESS, (uint8_t) 1);
        value = Wire._I2C_READ();
        Wire.endTransmission();
    } while ((value & 0b00100000) != 0);
} 
 