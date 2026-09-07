#include "rtc_ds1302.h"

#include <RtcDS1302.h>

#include "../config.h"
#include "../pins.h"

static_assert(DS1302_DATA_PIN != DS1302_CLOCK_PIN,
              "Les broches DATA et CLOCK du DS1302 doivent etre differentes.");
static_assert(DS1302_DATA_PIN != DS1302_CE_PIN,
              "Les broches DATA et CE du DS1302 doivent etre differentes.");
static_assert(DS1302_CLOCK_PIN != DS1302_CE_PIN,
              "Les broches CLOCK et CE du DS1302 doivent etre differentes.");
static_assert(RTC_GMT_OFFSET_MINUTES >= -1439 &&
              RTC_GMT_OFFSET_MINUTES <= 1439,
              "Le decalage GMT doit rester compris entre -23:59 et +23:59.");

// ThreeWire attend les broches dans l'ordre IO, SCLK, CE.
static ThreeWire rtcWire(
    DS1302_DATA_PIN,
    DS1302_CLOCK_PIN,
    DS1302_CE_PIN
);
static RtcDS1302<ThreeWire> rtc(rtcWire);
static bool rtcAvailable = false;

static RTCData invalidRTCData()
{
    RTCData data = {};
    data.running = false;
    data.valid = false;
    return data;
}

static void printRtcDateTime(const RtcDateTime& dateTime)
{
    char buffer[24];

    snprintf(
        buffer,
        sizeof(buffer),
        "%04u-%02u-%02u %02u:%02u:%02u",
        dateTime.Year(),
        dateTime.Month(),
        dateTime.Day(),
        dateTime.Hour(),
        dateTime.Minute(),
        dateTime.Second()
    );

    Serial.println(buffer);
}

bool initRTC()
{
    rtc.Begin();

    RtcDateTime currentDateTime = rtc.GetDateTime();
    bool dateTimeValid =
        rtc.IsDateTimeValid() &&
        currentDateTime.IsValid();
    bool running = rtc.GetIsRunning();

    if (!dateTimeValid || !running)
    {
        bool originalWriteProtection = rtc.GetIsWriteProtected();
        rtc.SetIsWriteProtected(false);

        if (!running)
        {
            Serial.println("[DS1302] Demarrage de l'oscillateur.");
            rtc.SetIsRunning(true);
        }

#if RTC_AUTO_INITIALIZE_IF_INVALID
        if (!dateTimeValid)
        {
            RtcDateTime compileDateTime(__DATE__, __TIME__);
            Serial.print("[DS1302] Initialisation a la date de compilation : ");
            printRtcDateTime(compileDateTime);
            rtc.SetDateTime(compileDateTime);
        }
#endif

        rtc.SetIsWriteProtected(originalWriteProtection);
        delay(250);
    }

    currentDateTime = rtc.GetDateTime();
    rtcAvailable =
        rtc.IsDateTimeValid() &&
        currentDateTime.IsValid() &&
        rtc.GetIsRunning();

    if (rtcAvailable)
    {
        Serial.print("[DS1302] Date/heure : ");
        printRtcDateTime(currentDateTime);
        Serial.print("[DS1302] Fuseau : ");
        Serial.println(RTC_TIMEZONE_LABEL);
    }
    else
    {
        Serial.println(
            "[DS1302] Indisponible - utilisation du timestamp de secours."
        );
    }

    return rtcAvailable;
}

RTCData readRTC()
{
    RTCData data = invalidRTCData();
    RtcDateTime currentDateTime = rtc.GetDateTime();

    data.running = rtc.GetIsRunning();
    data.valid =
        currentDateTime.IsValid() &&
        rtc.IsDateTimeValid() &&
        data.running;

    if (!data.valid)
    {
        rtcAvailable = false;
        return data;
    }

    data.year = currentDateTime.Year();
    data.month = currentDateTime.Month();
    data.day = currentDateTime.Day();
    data.hour = currentDateTime.Hour();
    data.minute = currentDateTime.Minute();
    data.second = currentDateTime.Second();

    rtcAvailable = true;
    return data;
}

bool isRTCAvailable()
{
    return rtcAvailable;
}

bool formatRTCTimestamp(
    const RTCData& data,
    char* output,
    size_t outputSize
)
{
    if (!data.valid || output == nullptr || outputSize == 0)
    {
        return false;
    }

    long offsetMinutes = RTC_GMT_OFFSET_MINUTES;
    char offsetSign = offsetMinutes < 0 ? '-' : '+';
    unsigned long absoluteOffset =
        static_cast<unsigned long>(
            offsetMinutes < 0 ? -offsetMinutes : offsetMinutes
        );
    unsigned long offsetHours = absoluteOffset / 60UL;
    unsigned long remainingMinutes = absoluteOffset % 60UL;

    int written = snprintf(
        output,
        outputSize,
        "%04u-%02u-%02uT%02u:%02u:%02u%c%02lu:%02lu",
        data.year,
        data.month,
        data.day,
        data.hour,
        data.minute,
        data.second,
        offsetSign,
        offsetHours,
        remainingMinutes
    );

    return written > 0 && static_cast<size_t>(written) < outputSize;
}
