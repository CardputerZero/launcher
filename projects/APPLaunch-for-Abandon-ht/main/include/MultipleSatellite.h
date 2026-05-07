/*
 *SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 *SPDX-License-Identifier: MIT
 */

#ifndef __MULITIPLESATELITE_H
#define __MULITIPLESATELITE_H

#include <Arduino.h>
#include <TinyGPS++.h>

typedef enum {
    SATELLITE_MODE_GPS = 0,  // GPS mode
    SATELLITE_MODE_BDS,      // BDS mode
    SATELLITE_MODE_GLONASS,  // GLONASS mode
    SATELLITE_MODE_GALILEO,  // GALILEO mode
    SATELLITE_MODE_QZSS,     // QZSS mode
} satellite_mode_t;

typedef enum {
    BOOT_HOST_START = 0,                // Hot start
    BOOT_WARM_START,                    // Warm start
    BOOT_COLD_START,                    // Cold start
    BOOT_FACTORY_START,                 // Factory start
    BOOT_FACTORY_DISABLE_SERIAL_RADIO,  // Factory starts, disables the serial port output and radio parts, and can
                                        // respond to serial port configurations.
    BOOT_FACTORY_ENABLE_SERIAL_RADIO,   // Factory start enables the serial port output and radio frequency parts
} boot_mode_t;

class MultipleSatellite : public TinyGPSPlus {
public:
    MultipleSatellite(HardwareSerial& serialPort, long baudRate, uint32_t config, int rxPin, int txPin)
        : _serial(serialPort), _baudRate(baudRate), _config(config), _rxPin(rxPin), _txPin(txPin)
    {
    }
    void begin()
    {
        _serial.begin(_baudRate, _config, _rxPin, _txPin);
    }

    bool available()
    {
        return _serial.available() > 0;
    }

    char read()
    {
        return _serial.read();
    }

    void write(const char* data)
    {
        _serial.print(data);
    }

    void updateGPS()
    {
        while (available()) {
            char c = read();
            encode(c);
        }
    }

    String getGNSSVersion()
    {
        write("$PCAS06,0*1B\r\n");
        return findCharacter("SW=");
    }

    String getAntennaState()
    {
        return findCharacter("ANTENNA");
    }
    
    String getSatelliteMode()
    {
        write("$PCAS06,2*19\r\n");
        String mode = findCharacter("MO=");
        if (mode == "G") {
            return "GPS";
        } else if (mode == "B") {
            return "BDS";
        } else if (mode == "R") {
            return "GLONASS";
        } else if (mode == "E") {
            return "GALILEO";
        } else if (mode == "Q") {
            return "QZSS";
        } else {
            return "Unknown";
        }
    }

    void setSatelliteMode(satellite_mode_t mode)
    {
        switch (mode) {
            case SATELLITE_MODE_GPS:
                write("$PCAS04,1*18\r\n");
                break;
            case SATELLITE_MODE_BDS:
                write("$PCAS04,2*1B\r\n");
                break;
            case SATELLITE_MODE_GLONASS:
                write("$PCAS04,4*1D\r\n");
                break;
            case SATELLITE_MODE_GALILEO:
                write("$PCAS04,8*11\r\n");
                break;
            case SATELLITE_MODE_QZSS:
                write("$PCAS04,20*2B\r\n");
                break;
            default:
                break;
        }
    }

    void setSystemBootMode(boot_mode_t mode)
    {
        switch (mode) {
            case BOOT_HOST_START:
                write("$PCAS10,0*1C\r\n");
                break;
            case BOOT_WARM_START:
                write("$PCAS10,1*1D\r\n");
                break;
            case BOOT_COLD_START:
                write("$PCAS10,2*1E\r\n");
                break;
            case BOOT_FACTORY_START:
                reset();
                write("$PCAS10,3*1F\r\n");
                break;
            case BOOT_FACTORY_DISABLE_SERIAL_RADIO:
                write("$PCAS10,8*14\r\n");
                break;
            case BOOT_FACTORY_ENABLE_SERIAL_RADIO:
                write("$PCAS10,9*15\r\n");
                break;
            default:
                break;
        }
    }

    void StandbyMode(){
        write("$PCAS12,65535*1E\r\n");
    }
     
private:
    HardwareSerial& _serial;
    long _baudRate;
    uint32_t _config;
    int _rxPin;
    int _txPin;
    String findCharacter(const char* searchStr)
    {
        delay(100);
        char GNSSbuf[100];
        int GNSSbufindexsum = 0;

        while (true) {
            if (available()) {
                char d = read();

                if (d == '\n') {
                    GNSSbuf[GNSSbufindexsum] = 0;
                    // Serial.printf("%s\n", GNSSbuf);
                    GNSSbufindexsum = 0;
                    if (strstr(GNSSbuf, searchStr) != nullptr) {
                        break;
                    }
                } else {
                    GNSSbuf[GNSSbufindexsum++] = d;
                    if (GNSSbufindexsum >= sizeof(GNSSbuf) - 1) {
                        GNSSbufindexsum = sizeof(GNSSbuf) - 1;
                    }
                }
            }
        }
        char* p = strstr(GNSSbuf, searchStr);
        if (p != nullptr) {
            strcpy(GNSSbuf, p + strlen(searchStr));
        }

        char* q = strstr(GNSSbuf, "*");
        if (q != nullptr) {
            *q = 0;
        }
        return String(GNSSbuf);
    }
};

#endif
