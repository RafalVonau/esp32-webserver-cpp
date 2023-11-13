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
#include <vector>
#include <json.hpp>
// using json = nlohmann::json;

struct www_file_t {
    const char *name;
    int size;
    const char *data;
    int gz;
    const char* mime_type;
};

/*!
 * \brief Compare string (const char*) implementation for std::map/std::multimap.
 */
class ExRequest_cmp_str {
public:
    ExRequest_cmp_str() {}
    bool operator()(const char* str1, const char* str2) const {
        if (str1 == str2) return false; else return (strcmp(str1, str2) < 0);
    }
};

class Express;
class ExRequest;
class WSRequest;
#ifdef CONFIG_EXPRESS_USE_AUTH
class ExpressSession;
#endif

/*!
 * \brief Get current time in seconds.
 */
time_t express_get_time_s();

/*!
 * \brief Get current time in miliseconds.
 */
uint64_t express_get_time_ms(); 

/*!
 * \brief Page callback.
 * \param c - pointer to Express class,
 * \param req - pointer to request/response class.
 */
typedef std::function<void(ExRequest* req)> ExpressPageCB;
/*!
 * \brief Middleware callback.
 * \param c - pointer to Express class,
 * \param req - pointer to request/response class.
 * \return true - process next middleware or page, false - break processing.
 */
typedef std::function<bool(ExRequest* req)> ExpressMidCB;
typedef const std::vector<ExpressMidCB> ExpressMidCBList;

/*!
 * \brief WebSocket callback.
 * \param c - pointer to Express class,
 * \param req - pointer to WebSocket request/response class.
 */
typedef std::function<void(WSRequest* req)> ExpressWSCB;
/*!
 * \brief WebSocket callback.
 * \param c - pointer to Express class,
 * \param req - pointer to WebSocket request/response class.
 * \param arg - pointer to argument,
 * \param arg_arg - argument length in bytes.
 */
typedef std::function<void(WSRequest* req, char* arg, int arg_len)> ExpressWSON;

typedef std::map<const char*, ExpressPageCB, ExRequest_cmp_str> ExpressPgMap;
typedef std::list<std::pair<const char*, ExpressPageCB> > ExpressPgList;
typedef std::list<std::pair<const char*, ExpressMidCB> > ExpressMidMap;



/*!
 * \brief HTTP Request/Response.
 */
class ExRequest {
public:
    ExRequest(httpd_req_t* rq, Express *e) {
        m_url = strdup(rq->uri);
        m_uri = m_url;
        m_req = rq;
        m_key = "";
        m_e = e;
        m_cookie_mem = NULL;
        m_param_mem = NULL;
        m_key_mem = NULL;
#ifdef CONFIG_EXPRESS_USE_AUTH
        m_session = NULL;
#endif
        m_json_parsed = false;
        parseURI();
    }

    ~ExRequest() {
        m_query.clear();
        m_cookie.clear();
        if (m_url) ::free((void *)m_url);
        if (m_cookie_mem) ::free((void *)m_cookie_mem);
        if (m_param_mem) ::free((void *)m_param_mem);
        if (m_key_mem) ::free((void *)m_key_mem);
    }
    const char* uri() const { return m_uri; }
    int getMethod() { return m_req->method; }

    void setKey(const char *key) { m_key = key; }

    /* Parameters from query like /xxx?nr=0&val=3  (key = [nr, val])*/
    const char* getArg(const char* key) {
        auto i = m_query.find(key);
        if (i == m_query.end()) return NULL;
        return i->second;
    }
    int getArgInt(const char* key, int df = -1) {
        int v = df;
        auto i = m_query.find(key);
        if (i == m_query.end()) return df;
        if (sscanf(i->second, "0x%x", &v) == 1) return v;
        if (sscanf(i->second, "%d", &v) == 1) return v;
        return df;
    }
    
    /* Parameters from cookie */
    const char* getCookie(const char* key) {
        if (!m_cookie_mem) parseCookie();
        auto i = m_cookie.find(key);
        if (i == m_cookie.end()) return NULL;
        return i->second;
    }
    void setCookie(const char* cookie);
    
    /* Parameters from uri like /api/add/:id/:val (name = [id, val]) */
    const char *getParamString(const char *name) {
        if (!m_param_mem) parseParams();
        auto i = m_param.find(name);
        if (i == m_param.end()) return NULL;
        return i->second;
    }
    int getParamInt(const char *name, int defVal = -1) {
        int v = defVal;
        const char *val = getParamString(name);
        if (val) {
            if (sscanf(val, "0x%x", &v) == 1) return v;
            if (sscanf(val, "%d", &v) == 1) return v;
        }
        return defVal;
    }

    /* Read data (from post for example) */
    int getContentLen() const { return m_req->content_len; }
    std::string getHeader(const char *key) const;
    std::string getContentType() const;
    std::string readAll() {
        int len = m_req->content_len;
        std::string res(len, 0);
        if ( httpd_req_recv(m_req, (char *)&res[0], len) == len) {
            return res;
        }
        return std::string();
    }
    int read(char *buf, int len) {
        return httpd_req_recv(m_req, buf, len);
    }

    /* Write answer */
    esp_err_t json(const char* resp, int len = 0);
    esp_err_t json(std::string& s) { return json(s.c_str(), s.length()); }
    esp_err_t txt(const char* resp, int len = 0);
    esp_err_t txt(std::string& s) { return txt(s.c_str(), s.length()); }
    esp_err_t gzip(const char* type, const char* resp, int len = 0);
    esp_err_t send(const char* type, const char* resp, int len);
    esp_err_t send_res(esp_err_t ret);
    esp_err_t error(httpd_err_code_t error, const char *msg = NULL) { return httpd_resp_send_err(m_req, error, msg); }
    /* Low level versions */
    esp_err_t setStatus(const char *status) { return httpd_resp_set_status(m_req, status); }
    esp_err_t setType(const char *type) { return httpd_resp_set_type(m_req, type); }
    esp_err_t setHeader(const char *key, const char *val) { return httpd_resp_set_hdr(m_req, key, val); }
    esp_err_t sendAll(const char* buf, int buf_len) {return httpd_resp_send(m_req, buf, buf_len); }
    /*!
     * \brief Send in chunks. When you are finished sending all your chunks, you must call
     *   this function with buf_len as 0.
     */
    esp_err_t sendChunk(const char* buf, int buf_len) { return httpd_resp_send_chunk(m_req, buf, buf_len); }
    


    /* Redirect to another location */
    esp_err_t redirect(const char *path, const char *type = "302 Found");

private:
    void parseURI();
    void parseParams();
    void parseCookie();

public:
    Express      *m_e;
    httpd_req_t  *m_req;
    char *m_url, *m_cookie_mem, *m_param_mem, *m_key_mem;
    const char *m_uri, *m_key;
    std::map<const char*, const char*, ExRequest_cmp_str> m_query;    /*!< Parameters from query.   */
    std::map<const char*, const char*, ExRequest_cmp_str> m_cookie;   /*!< Parameters from cookie.  */
    std::map<const char*, const char*, ExRequest_cmp_str> m_param;    /*!< Parameters from path.    */
    std::map<std::string, std::string> m_user;                        /*!< Additional parameters.   */
    nlohmann::json m_json;                                            /*!< Parsed JSON document.    */
    bool           m_json_parsed;                                     /*!< JSON parsed flag.        */
#ifdef CONFIG_EXPRESS_USE_AUTH
    ExpressSession *m_session;                                        /*!< Pointer to session data. */
#endif
};

#define WS_MAX_FRAME_SIZE (4100)

/*!
 * \brief Websocket Request/Response.
 */
class WSRequest {
public:
    WSRequest(httpd_req_t* rq, Express *e) {
        m_req = rq;
        m_server = rq->handle;
        m_e = e;
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
    httpd_req_t      *m_req;
    httpd_handle_t    m_server;
    Express          *m_e;
};

#ifdef CONFIG_EXPRESS_USE_AUTH

class ExpressSession {
public:
	ExpressSession(std::string n, int st = 3600) {
        sessionTimeout = st; 
        userName = n; 
        expire = express_get_time_s() + sessionTimeout; 
    }
	void ping() { 
		expire = express_get_time_s() + sessionTimeout; 
	}
public:
    int sessionTimeout;                          /*!< Session inactivity timeout in seconds.  */
	std::string userName;                        /*!< Logged user name.                       */
	time_t expire;                               /*!< Timestamp when the session will expire. */
    std::map<std::string, std::string> m_user;   /*!< Additional parameters.                  */
};

#endif

/*!
 * \brief HTTP Server.
 */
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

    /*!
     * \brief Check for meta keys ( *, : , #) in path.
     */
    bool hasMeta(const char *a) const;
    /*!
     * \brief Compare path (skip section if * or : characters are detected).
     */
    bool comparePath(const char *a, const char *b) const;

    /* http methods */
    void get(const char* path, ExpressPageCB cb)   { if (hasMeta(path)) m_lget.push_back({ path, cb }); else m_get.insert({ path, cb }); }
    void post(const char* path, ExpressPageCB cb)  { if (hasMeta(path)) m_lpost.push_back({ path, cb }); else m_post.insert({ path, cb }); }
    void del(const char* path, ExpressPageCB cb)   { if (hasMeta(path)) m_ldelete.push_back({ path, cb }); else m_delete.insert({ path, cb }); }
    void patch(const char* path, ExpressPageCB cb) { if (hasMeta(path)) m_lpatch.push_back({ path, cb }); else m_patch.insert({ path, cb }); }
    void put(const char* path, ExpressPageCB cb)   { if (hasMeta(path)) m_lput.push_back({ path, cb }); else m_put.insert({ path, cb }); }
    void all(const char* path, ExpressPageCB cb)   { 
        if (hasMeta(path)) {
            /* put to std::list */
            m_lget.push_back({ path, cb });
            m_lpost.push_back({ path, cb });
            m_ldelete.push_back({ path, cb });
            m_lpatch.push_back({ path, cb });
            m_lput.push_back({ path, cb });
        } else {
            /* put to std::map */
            m_get.insert({ path, cb }); 
            m_post.insert({ path, cb });
            m_delete.insert({ path, cb }); 
            m_patch.insert({ path, cb });
            m_put.insert({ path, cb });
        }
    }

    /* Middleware */
    void use(const char* path, ExpressMidCB cb) { if (*path == '\0') m_midAll.push_back({ path, cb }); else m_mid.push_back({ path, cb }); }
    /* Single middleware */
    void get(const char* path, ExpressMidCB m, ExpressPageCB cb)   { get(path, [cb, m](ExRequest* r)   { if (m(r)) cb(r); }); }
    void post(const char* path, ExpressMidCB m, ExpressPageCB cb)  { post(path, [cb, m](ExRequest* r)  { if (m(r)) cb(r); }); }
    void del(const char* path, ExpressMidCB m, ExpressPageCB cb)   { del(path, [cb, m](ExRequest* r)   { if (m(r)) cb(r); }); }
    void patch(const char* path, ExpressMidCB m, ExpressPageCB cb) { patch(path, [cb, m](ExRequest* r) { if (m(r)) cb(r); }); }
    void put(const char* path, ExpressMidCB m, ExpressPageCB cb)   { put(path, [cb, m](ExRequest* r)   { if (m(r)) cb(r); }); }
    void all(const char* path, ExpressMidCB m, ExpressPageCB cb)   { all(path, [cb, m](ExRequest* r)   { if (m(r)) cb(r); }); }
        
    /* List of Middlewares like .get("path", {middlewareFunction0, middlewareFunction1}, [] ... );  */
    void get(const char* path,ExpressMidCBList &l,ExpressPageCB cb)   { get(path,[cb,l](ExRequest* r)   {for (const auto& f : l) if (!f(r)) return; cb(r); }); }
    void post(const char* path,ExpressMidCBList &l,ExpressPageCB cb)  { post(path,[cb,l](ExRequest* r)  {for (const auto& f : l) if (!f(r)) return; cb(r); }); }
    void del(const char* path,ExpressMidCBList &l,ExpressPageCB cb)   { del(path,[cb,l](ExRequest* r)   {for (const auto& f : l) if (!f(r)) return; cb(r); }); }
    void patch(const char* path,ExpressMidCBList &l,ExpressPageCB cb) { patch(path,[cb,l](ExRequest* r) {for (const auto& f : l) if (!f(r)) return; cb(r); }); }
    void put(const char* path,ExpressMidCBList &l,ExpressPageCB cb)   { put(path,[cb,l](ExRequest* r)   {for (const auto& f : l) if (!f(r)) return; cb(r); }); }
    void all(const char* path,ExpressMidCBList &l,ExpressPageCB cb)   { all(path,[cb,l](ExRequest* r)   {for (const auto& f : l) if (!f(r)) return; cb(r); }); }

    /* Websocket events */
    void onWS(ExpressWSCB w) { m_wsCB = w; }
    void on(const char* what, ExpressWSON cb) { m_on.insert({ what, cb }); }
    void off(const char* what) { m_on.erase(what); }
    void once(const char* what, ExpressWSON cb) { 
        m_on.insert({ what, [this,cb,what](WSRequest* req, char* arg, int arg_len) {
            cb(req, arg, arg_len);
            this->m_on.erase(what);
        }}); 
    }

    std::string generateUUID();

    ExpressMidCB getJsonMW();
#ifdef CONFIG_EXPRESS_USE_AUTH
    /* Session helper */
    void cleanupOutdatedSessions();
    
    /* Buildin middlewares */
    ExpressMidCB getSessionMW(int maxAge = 2592000);
    ExpressMidCB getWithAuthMW();
    void doLogin(ExRequest* req, std::string user);
    void doLogOut(ExRequest* req);

    /* Std login */
    void addUser(const char *userName, const char *pass) {m_passwd[userName] = pass;}
    ExpressPageCB getStdLoginFunction();
#endif

    /*!
     * \brief Add static files.
     * \param arg - pointer to generated file table in the form [ { name, size, data, gz, mime_type }, ...]
     */
    void addStatic(struct www_file_t *);

    /* Wrappers */
    esp_err_t doRQ(httpd_req_t* req, ExpressPgMap* m, ExpressPgList *l);
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
    ExpressPgMap           m_get, m_post, m_delete, m_patch, m_put;
    ExpressPgList          m_lget, m_lpost, m_ldelete, m_lpatch, m_lput;
    ExpressMidMap          m_mid, m_midAll;
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
#ifdef CONFIG_EXPRESS_USE_AUTH
    std::map<std::string, ExpressSession *> m_sessions;
    std::map<std::string, std::string> m_passwd;
#endif
};


#endif

