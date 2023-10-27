/*
 * WebServer implementation.
 *
 * Author: Rafal Vonau <rafal.vonau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __EXPRESS__
#define __EXPRESS__

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

class ExRequest_cmp_str {
public:
    ExRequest_cmp_str() {}

    bool operator()(const char* str1, const char* str2) const {
        if (str1 == str2)
            return false; // same pointer so "not less"
        else
            return (strcmp(str1, str2) < 0); //string compare: str1<str2 ?
    }
};

/*!
 * \brief HTTP Request/Response.
 */
class ExRequest {
public:
    ExRequest(httpd_req_t* rq) {
        m_url = strdup(rq->uri);
        m_uri = m_url;
        m_req = rq;
        parseURI();
    }

    ~ExRequest() {
        free(m_url);
    }

    const char* uri() const { return m_uri; }
    const char* getArg(const char* key) {
        auto i = m_query.find(key);
        if (i == m_query.end()) return NULL;
        return i->second;
    }
    int getArgInt(const char* key, int df) {
        int v;
        auto i = m_query.find(key);
        if (i == m_query.end()) return df;
        if (sscanf(i->second, "0x%x", &v) == 1) return v;
        if (sscanf(i->second, "%d", &v) == 1) return v;
        return df;
    }
    int argCount() const { return m_query.size(); }

    esp_err_t json(const char* resp, int len = 0);
    esp_err_t json(std::string& s) { return json(s.c_str(), s.length()); }
    esp_err_t txt(const char* resp, int len = 0);
    esp_err_t txt(std::string& s) { return txt(s.c_str(), s.length()); }
    esp_err_t gzip(const char* type, const char* resp, int len = 0);
    esp_err_t send(const char* type, const char* resp, int len);
    esp_err_t send_res(esp_err_t ret);
private:
    void parseURI();

public:
    httpd_req_t* m_req;
    char* m_url;
    const char* m_uri;
    std::map<const char*, const char*, ExRequest_cmp_str> m_query;
};

#define WS_MAX_FRAME_SIZE (4100)

/*!
 * \brief Websocket Request/Response.
 */
class WSRequest {
public:
    WSRequest(httpd_req_t* rq) {
        m_req = rq;
        m_server = rq->handle;
        memset(&m_pkt, 0, sizeof(httpd_ws_frame_t));
        m_pkt.payload = m_buf;
        m_pkt.type = HTTPD_WS_TYPE_TEXT;
    }

    ~WSRequest() {}

    uint8_t* payload() { return m_pkt.payload; }
    size_t  len() const { return m_pkt.len; }

    httpd_ws_type_t type() const { return m_pkt.type; }
    void setType(httpd_ws_type_t t) { m_pkt.type = t; }

    esp_err_t res_val(const char* fn, esp_err_t ret, uint32_t val);
    esp_err_t send(const char* s, int len = 0);
    void send_to_all_clients(const char* buf);
public:
    uint8_t           m_buf[WS_MAX_FRAME_SIZE];
    httpd_ws_frame_t  m_pkt;
    httpd_req_t* m_req;
    httpd_handle_t    m_server;
};



/*!
 * \brief HTTP Server.
 */
class Express;

typedef std::function<void(Express* c, ExRequest* req)> ExpressPageCB;
typedef std::function<void(Express* c, WSRequest* req)> ExpressWSCB;
typedef std::function<void(Express* c, WSRequest* req, char* arg, int arg_len)> ExpressWSON;
typedef std::map<const char*, ExpressPageCB, ExRequest_cmp_str> ExpressPgMap;
class Express {
public:
    /*!
     * \brief Construct a new Express object.
     */
    Express();
    ~Express();

    /*!
     * \brief Start http server.
     * \param port - listening port,
     * \param pr - task priority,
     * \param coreID - task CPU core.
     */
    void start(int port = 80, uint8_t pr = 0, BaseType_t coreID = tskNO_AFFINITY);

    void get(const char* path, ExpressPageCB cb) { m_get.insert({ path, cb }); }
    void post(const char* path, ExpressPageCB cb) { m_post.insert({ path, cb }); }
    void del(const char* path, ExpressPageCB cb) { m_delete.insert({ path, cb }); }
    void patch(const char* path, ExpressPageCB cb) { m_patch.insert({ path, cb }); }
    void put(const char* path, ExpressPageCB cb) { m_put.insert({ path, cb }); }
    void all(const char* path, ExpressPageCB cb) { m_all.insert({ path, cb }); }
    /* Websocket events */
    void onWS(ExpressWSCB w) { m_wsCB = w; }
    void on(const char* what, ExpressWSON cb) { m_on.insert({ what, cb }); }
    void off(const char* what) { m_on.erase(what); }

    /*!
     * \brief Add static files.
     * \param arg - pointer to generated file table in the form [ { name, size, data, gz, mime_type }, ...]
     */
    void addStatic(void*);

    /* Wrappers */
    esp_err_t doRQ(httpd_req_t* req, ExpressPgMap* m);
    esp_err_t doWS(WSRequest* req);
    /* OTA */
    esp_err_t ota_stop(uint32_t abort);
    esp_err_t ota_post_handler(httpd_req_t* req);
private:
    /* PM */
    esp_err_t do_pm_lock();
    void      do_pm_unlock();

public:
#if defined(CONFIG_PM_ENABLE)
    esp_pm_lock_handle_t   m_pm_cpu_lock;
    esp_pm_lock_handle_t   m_pm_sleep_lock;
#endif
    ExpressPgMap           m_get, m_post, m_delete, m_patch, m_put, m_all;
    httpd_handle_t         m_server;
    httpd_config_t         m_config;
    ExpressWSCB            m_wsCB;
    /* Wrap handlers */
    httpd_uri_t            m_h_get, m_h_post, m_h_ws, m_h_delete, m_h_patch, m_h_put;
    /* OTA */
    volatile uint32_t      __ota_id, __ota_size, __ota_cnt, __ota_active;
    const esp_partition_t* __ota_update_partition;
    esp_ota_handle_t       __ota_update_handle;
    int64_t                __ota_start_timestamp;
    std::map<const char*, ExpressWSON, ExRequest_cmp_str> m_on;
};

#endif

