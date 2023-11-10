/*
 * RAM based circular log implementation.
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __RAMLOG__
#define __RAMLOG__

#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "esp_pm.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <map>
#include <string>
#include <list>

class RAMLog;

/*!
 * \brief RAM based circular log.
 */
class RAMLog {
public:
    ~RAMLog() {
        vSemaphoreDelete(m_lock);
        if (m_buf) ::free(m_buf);
    }

    /* Singleton */
    static RAMLog* instance() {
        if (!sm_instance) {
            sm_instance = new RAMLog();
        }
        return sm_instance;
    }

    /*!
     * \brief Install RAM log.
     *
     * \param useSerial - print also on serial port (true/false),
     * \param bufSize   - RAM buffer size in bytes.
     */
    void install(bool useSerial = true, int bufSize = 16 * 1024);

    int write(const char* fmt, va_list args);
    
    /*!
     * \brief Write data to RAM log (maximum line size is 255).
     */
    int write(const char* data, int len);

    /*!
     * \brief Read all data from RAM log.
     */
    std::string read();
private:
    int getNextPointer(int offset);
protected:
    RAMLog() {
        m_buf = NULL;
        m_size = m_wp = m_rp = 0;
        vSemaphoreCreateBinary(m_lock);
    }
public:
    static RAMLog* sm_instance;
    uint8_t* m_buf;
    int      m_size;
    int      m_wp;
    int      m_rp;
    bool     m_useSerial;
    vprintf_like_t m_old_fn;
    SemaphoreHandle_t m_lock;
};


#endif

