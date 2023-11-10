/*
 * RAM based circular log implementation.
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_pm.h"

#include "nvs_flash.h"

#include "netdb.h"
#include "esp_sntp.h"
#include <string>
#include "ramlog.h"

RAMLog *RAMLog::sm_instance = NULL;

static int ramlog_log_vprintf(const char *fmt, va_list args) 
{
	if (RAMLog::sm_instance) {		
		return RAMLog::sm_instance->write(fmt, args);
	}
    return 0;
}

/*!
 * \brief Install RAM log.
 * 
 * \param useSerial - print also on serial port (true/false),
 * \param bufSize   - RAM buffer size in bytes.
 */
void RAMLog::install(bool useSerial, int bufSize)
{
	if (m_buf) {
		::free(m_buf);
	}
	m_useSerial = useSerial;
	m_size = bufSize;
	m_buf = (uint8_t *)malloc(bufSize);
	m_wp = m_rp = 0;
	m_buf[0] = '\0';
	m_old_fn = esp_log_set_vprintf(ramlog_log_vprintf);
}

int RAMLog::write(const char *fmt, va_list args)
{
	if (m_useSerial) {
		m_old_fn(fmt, args);
	}
	{
		char buf[256];
    	int len;
		len = vsnprintf(buf, 255, fmt, args);
		return this->write(buf, len);
	}
}

#define ramlog_offset(n)	((n) & (m_size - 1))

/*
 * clock_interval - is a < c < b in mod-space? Put another way, does the line
 * from a to b cross c?
 */
static inline int clock_interval(size_t a, size_t b, size_t c)
{
	if (b < a) {
		if (a < c || b >= c)
			return 1;
	} else {
		if (a < c && b >= c)
			return 1;
	}
	return 0;
}

int RAMLog::getNextPointer(int offset) 
{
	if (offset == m_wp) return offset;
	offset += m_buf[offset] + 1;
	return ramlog_offset(offset);
}


/*!
 * \brief Write data to RAM log (maximum line size is 255).
 */
int RAMLog::write(const char *data, int len)
{
	if (m_buf) {
		int need = 1;
		/* Limit length */
		if (len > 255) len = 255;
		need += len;
		/* Locked access */
		if (xSemaphoreTake(m_lock, (TickType_t)1000) == pdFALSE) return 0;
		{
			int l, wp = ramlog_offset(m_wp + 1);
			int newp = ramlog_offset(m_wp + need);
			/* Fixup read pointer if no space left */
			while (clock_interval(m_wp, newp, m_rp)) {
				m_rp = getNextPointer(m_rp);
			}
			/* Write header */
			m_buf[m_wp] = (char)len; 
			l = MIN(len, m_size - wp);
			memcpy(m_buf + wp, data, l);
			if (len != l)
				memcpy(m_buf, data + l, len - l);
			m_wp = ramlog_offset(m_wp + need);
		}
		xSemaphoreGive(m_lock);
	}
	
	return len;
}

/*!
 * \brief Read all data from RAM log.
 */
std::string RAMLog::read()
{
	std::string r;
	/* Locked access */
	if (xSemaphoreTake(m_lock, (TickType_t)1000) == pdFALSE) return 0;
	int _rp = m_rp;
	while (_rp != m_wp) {
		int l, s = (unsigned char)m_buf[_rp];
		int rp = ramlog_offset(_rp + 1);
		std::string res(s, 0);
		l = MIN(s, m_size - rp);
		memcpy(&res[0], m_buf + rp, l);
		if (s != l)
			memcpy(&res[l], m_buf, s - l);
		_rp = ramlog_offset(_rp + s + 1);
		r += res;
	}
	xSemaphoreGive(m_lock);
	return r;
}