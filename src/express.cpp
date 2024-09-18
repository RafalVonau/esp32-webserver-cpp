/*
 * WebServer implementation.
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
#include "express.h"
#include "mbedtls/base64.h"
#include "bootloader_random.h"
// #include "esp_httpd_priv.h"


static const char* TAG = "Express";

#ifdef DEBUG
#define msg_info(fmt, args...) ESP_LOGI(TAG, fmt, ## args);
#define msg_init(fmt, args...)  ESP_LOGI(TAG, fmt, ## args);
#define msg_debug(fmt, args...)  ESP_LOGI(TAG, fmt, ## args);
#define msg_error(fmt, args...)  ESP_LOGE(TAG, fmt, ## args);
#define msg_ota(fmt, args...)  ESP_LOGI(TAG, fmt, ## args);
#else
#define msg_info(fmt, args...)
#define msg_init(fmt, args...)
#define msg_debug(fmt, args...)
#define msg_error(fmt, args...)  ESP_LOGE(TAG, fmt, ## args);
#define msg_ota(fmt, args...)  ESP_LOGI(TAG, fmt, ## args);
#endif

#define HTTP_CHUNK_SIZE      (4096)
#define STATUS_JSON_MAX_SIZE (16384)

/*!
 * \brief Reboot.
 */
static void reboot(void)
{
    msg_info("Rebooting in 1 seconds...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

/*!
 * \brief Get current time in seconds.
 */
time_t express_get_time_s() 
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec;
}

/*!
 * \brief Get current time in miliseconds.
 */
uint64_t express_get_time_ms() 
{
    uint64_t r;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

/*!
 * \brief Lock CPU and LIGHT_SLEEP.
 */
esp_err_t Express::do_pm_lock()
{
#if defined(CONFIG_PM_ENABLE)
    if (m_pm_cpu_lock == NULL) {
        if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "express_sleep", &m_pm_sleep_lock) != ESP_OK) {
            msg_error("Failed to create PM sleep lock");
            return ESP_FAIL;
        }
        if (esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "express_cpu", &m_pm_cpu_lock) != ESP_OK) {
            msg_error("Failed to create PM CPU lock");
            return ESP_FAIL;
        }
    }
    esp_pm_lock_acquire(m_pm_cpu_lock);
    esp_pm_lock_acquire(m_pm_sleep_lock);
#endif    
    return ESP_OK;
}

/*!
 * \brief UnLock CPU and LIGHT_SLEEP.
 */
void Express::do_pm_unlock()
{
#if defined(CONFIG_PM_ENABLE)
    esp_pm_lock_release(m_pm_cpu_lock);
    esp_pm_lock_release(m_pm_sleep_lock);
#endif
}

//===========================================================================
//=====================--- esp_httpd_server wrappers ---=====================
//===========================================================================


static esp_err_t express_post_handler(httpd_req_t* req)
{
    Express* e = (Express*)httpd_get_global_user_ctx(req->handle);
    return e->doRQ(req, &e->m_post, &e->m_lpost);
}

static esp_err_t express_get_handler(httpd_req_t* req)
{
    Express* e = (Express*)httpd_get_global_user_ctx(req->handle);
    return e->doRQ(req, &e->m_get, &e->m_lget);
}

static esp_err_t express_delete_handler(httpd_req_t* req)
{
    Express* e = (Express*)httpd_get_global_user_ctx(req->handle);
    return e->doRQ(req, &e->m_delete, &e->m_ldelete);
}

static esp_err_t express_patch_handler(httpd_req_t* req)
{
    Express* e = (Express*)httpd_get_global_user_ctx(req->handle);
    return e->doRQ(req, &e->m_patch, &e->m_lpatch);
}

static esp_err_t express_put_handler(httpd_req_t* req)
{
    Express* e = (Express*)httpd_get_global_user_ctx(req->handle);
    return e->doRQ(req, &e->m_put, &e->m_lput);
}

static esp_err_t express_ws_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET) {
        // ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    } else {
        Express* e = (Express*)httpd_get_global_user_ctx(req->handle);
        WSRequest r(req, e);
        esp_err_t ret;

        ret = httpd_ws_recv_frame(req, &r.m_pkt, WS_MAX_FRAME_SIZE);
        if (ret != ESP_OK) {
            msg_error("httpd_ws_recv_frame failed with %d", ret);
            return ret;
        } else {
            // msg_info("Got frame (final = %d, fragmented=%d)", r.m_pkt.final?1:0, r.m_pkt.fragmented?1:0);
            return e->doWS(&r);
        }
        return ret;
    }
}

/*!
 * \brief Constructor.
 */
Express::Express()
{
#if defined(CONFIG_PM_ENABLE)
    m_pm_cpu_lock = NULL;
    m_pm_sleep_lock = NULL;
#endif
    __ota_active = 0;
    __ota_update_partition = NULL;
    __ota_update_handle = 0;
    __ota_start_timestamp = 0;

    /* Fill handlers */
    memset(&m_h_get, 0, sizeof(httpd_uri_t));
    memset(&m_h_post, 0, sizeof(httpd_uri_t));
    memset(&m_h_ws, 0, sizeof(httpd_uri_t));
    memset(&m_h_delete, 0, sizeof(httpd_uri_t));
    memset(&m_h_patch, 0, sizeof(httpd_uri_t));
    memset(&m_h_put, 0, sizeof(httpd_uri_t));

    m_h_get.uri = "*";
    m_h_get.handler = express_get_handler;
    m_h_get.user_ctx = (void*)this;
    m_h_get.is_websocket = false;
    m_h_get.method = HTTP_GET;

    m_h_post.uri = "*";
    m_h_post.handler = express_post_handler;
    m_h_post.user_ctx = (void*)this;
    m_h_post.is_websocket = false;
    m_h_post.method = HTTP_POST;

    m_h_ws.uri = "/ws";
    m_h_ws.handler = express_ws_handler;
    m_h_ws.user_ctx = (void*)this;
    m_h_ws.is_websocket = true;
    m_h_ws.method = HTTP_GET;

    m_h_delete.uri = "*";
    m_h_delete.handler = express_delete_handler;
    m_h_delete.user_ctx = (void*)this;
    m_h_delete.is_websocket = false;
    m_h_delete.method = HTTP_DELETE;

    m_h_patch.uri = "*";
    m_h_patch.handler = express_patch_handler;
    m_h_patch.user_ctx = (void*)this;
    m_h_patch.is_websocket = false;
    m_h_patch.method = HTTP_PATCH;

    m_h_put.uri = "*";
    m_h_put.handler = express_put_handler;
    m_h_put.user_ctx = (void*)this;
    m_h_put.is_websocket = false;
    m_h_put.method = HTTP_PUT;

    m_wsCB = NULL;
    m_onMissing = NULL;

    /* Generic API */
    get("api/mem", [](ExRequest* req) {
        /* Send JSON as response */
        std::string json = "{ \"free\": " + std::to_string(esp_get_free_heap_size()) + ",\"reset\": " + std::to_string(esp_reset_reason()) + " }";
        req->json(json.c_str(), json.length());
    });
    get("api/restart", [](ExRequest* req) {
        req->json("{ \"ok\": true }");
        reboot();
    });
    get("api/ping", [](ExRequest* req) {
        req->json("{ \"pong\": true }");
    });
    get("sn", [](ExRequest* req) {
        req->json("{ \"sn\": true }");
    });
#ifdef CONFIG_PM_PROFILING
    get("api/pm", [](ExRequest* req) {
        char* buf = (char*)::calloc(HTTP_CHUNK_SIZE, sizeof(char));
        if (buf) {
            FILE* stream;
            stream = fmemopen(buf, HTTP_CHUNK_SIZE, "w");
            esp_pm_dump_locks(stream);
            fclose(stream);
            req->txt((const char*)buf, strlen(buf));
            free(buf);
        }
    });
#endif
#ifndef NO_EXPRESS_TASKLIST
    get("api/tasks", [](ExRequest* req) {
        std::string ret;
        const size_t bytes_per_task = 40; /* see vTaskList description */
        char *task_list_buffer = (char *)malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
        if (task_list_buffer == NULL) {
            msg_error("failed to allocate buffer for vTaskList output");
            return;
        }
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
        ret = "Task Name\tStatus\tPrio\tHWM\tTask#\tAffinity\n";
#else
        ret = "Task Name\tStatus\tPrio\tHWM\tTask#\n";
#endif
        vTaskList(task_list_buffer);
        ret += std::string(task_list_buffer);
        free(task_list_buffer);
        req->txt((const char*)ret.c_str(), ret.length());
    });
#endif    
    /* OTA */
    post("ota", [](ExRequest* req) { req->m_e->ota_post_handler(req->m_req); });
}

/*!
* \brief
*/
Express::~Express()
{
#if defined(CONFIG_PM_ENABLE)
    if (m_pm_cpu_lock) esp_pm_lock_delete(m_pm_cpu_lock);
    if (m_pm_sleep_lock) esp_pm_lock_delete(m_pm_sleep_lock);
#endif
}

/*!
 * \brief Check for meta keys ( *, :, # ) in path.
 */
bool Express::hasMeta(const char *a) const
{
    const char *ch = a;
    while (*ch != '\0') {
        /* Check for meta characters */
        if ((*ch == '*') || (*ch == ':') || (*ch == '#')) return true;
        ch++;
    }
    return false;
}

/*!
 * \brief Compare path (skip section if * or : characters are detected).
 */
bool Express::comparePath(const char *a, const char *b) const
{
    if (*a == '\0') return true;
	while ((*a != '\0') && (*b != '\0')) {
        if (*a == '#') return true;
		if ((*a == '*') || (*a == ':')) {
			/* Skip section */
			while ((*a != '/') && (*a != '\0')) a++;
			while ((*b != '/') && (*b != '\0')) b++;
		}
		if ((*a == '\0') || (*b == '\0')) break;
		if (*a != *b) return false;		
		a++;
		b++;
	}
	if ((*a != '\0') || (*b != '\0')) return false;
    return true;
}


/*!
 * \brief Start http server.
 * \param port - listening port,
 * \param pr - task priority,
 * \param coreID - task CPU core.
 */
void Express::start(int port, uint8_t pr, BaseType_t coreID)
{
    std::map<const char*, ExpressPageCB, ExRequest_cmp_str>::iterator i;
    m_config = HTTPD_DEFAULT_CONFIG();

    /* LOCK core ID for HTTP server */
    m_config.server_port = port;
    m_config.core_id = coreID;
    m_config.task_priority = pr;
    m_config.max_uri_handlers = 8;
    m_config.global_user_ctx = (void *)this;
    /* this is an important option that isn't set up by default.
     * We could register all URLs one by one, but this would not work while the fake DNS is active */
    m_config.uri_match_fn = httpd_uri_match_wildcard;
    m_config.lru_purge_enable = true;
    m_config.stack_size = 16 * 1024;

    // for (i = m_get.begin(); i != m_get.end(); ++i) {
    //     msg_debug("GET <%s>", i->first);
    // }

    msg_info("Starting server on port: '%d'", m_config.server_port);
    if (httpd_start(&m_server, &m_config) == ESP_OK) {
        msg_info("Registering URI handlers");
        httpd_register_uri_handler(m_server, &m_h_ws);
        httpd_register_uri_handler(m_server, &m_h_get);
        httpd_register_uri_handler(m_server, &m_h_post);
        httpd_register_uri_handler(m_server, &m_h_delete);
        httpd_register_uri_handler(m_server, &m_h_patch);
        httpd_register_uri_handler(m_server, &m_h_put);
        return;
    }
    msg_error("Error starting server!");
}


const static char http_404_hdr[] = "404 Not Found";
const static char http_401_hdr[] = "401 Unauthorized";
/*!
 * \brief Handle GET (HTML/CSS/JS/JSON code).
 */
esp_err_t Express::doRQ(httpd_req_t* req, ExpressPgMap* m, ExpressPgList *l)
{
    ExRequest rq(req, this);
    esp_err_t ret = ESP_OK;
    do_pm_lock();

    /* Middleware - phase1 (mach to all pages) */
    {
        auto itr1 = m_midAll.cbegin();
        while (itr1 != m_midAll.cend()) {
            rq.setKey(itr1->first);
            if (!itr1->second(&rq)) {
                do_pm_unlock();
                return ret;
            }
            itr1++;
        }

        /* Middleware - phase2 (selective match) */
        itr1 = m_mid.cbegin();
        while (itr1 != m_mid.cend()) {
            if (comparePath(itr1->first, rq.uri())) {
                rq.setKey(itr1->first);
                if (!itr1->second(&rq)) {
                    do_pm_unlock();
                    return ret;
                }
            }
            itr1++;
        }    
    }

    /* Find page in map */
    {
        auto i = m->find(rq.uri());
        if (i != m->end()) {
            rq.setKey(i->first);
            i->second(&rq);
            do_pm_unlock();
            return ret;
        }
    }

    /* Find page in list */
    {
        auto i = l->cbegin();
        while (i != l->cend()) {
            if (comparePath(i->first, rq.uri())) {
                rq.setKey(i->first);
                i->second(&rq);
                do_pm_unlock();
                return ret;
            }
            i++;
        }
    }
    if (m_onMissing) {
        if (m_onMissing(&rq)) {
            do_pm_unlock();
            return ret;            
        }
    }
    /* Not found */
    httpd_resp_set_status(req, http_404_hdr);
    ret = httpd_resp_send(req, NULL, 0);
    
    do_pm_unlock();
    return ret;
}

/*!
 * \brief Add static files.
 * 
 * \param arg - pointer to generated file table in the form [ { name, size, data, gz, mime_type }, ...]
 */
void Express::addStatic(struct www_file_t *f)
{
    struct www_file_t* n;
    int l, i = 0;

    l = strlen(f[i].name);
    while (l) {
        n = &f[i];
        if (n->gz) {
            get(n->name, [n](ExRequest* req) { req->gzip(n->mime_type, n->data, n->size); });
        } else {
            get(n->name, [n](ExRequest* req) { req->send(n->mime_type, n->data, n->size); });
        }
        i++;
        l = strlen(f[i].name);
    }
}


/*!
 * \brief Handle websocket message (API).
 */
esp_err_t Express::doWS(WSRequest* rq)
{
    char* pl = (char*)rq->payload();
    esp_err_t ret = ESP_OK;

    do_pm_lock();

    if (rq->type() == HTTPD_WS_TYPE_TEXT) {
        if (strncmp((char*)pl, "ota ", 4) == 0) {
            /* Perform OTA */
            char* dt = pl + 4;
            ret = ota_stop(2); /* Abort any pending OTA update */
            if (ret == ESP_OK) {
                if (sscanf(dt, "%u %u", (unsigned int *)&__ota_id, (unsigned int *)&__ota_size) == 2) {
                    msg_ota("ota request id = %u, size = %u",(unsigned int)__ota_id, (unsigned int)__ota_size);
                    __ota_start_timestamp = esp_timer_get_time();
                    __ota_update_partition = esp_ota_get_next_update_partition(NULL);
                    __ota_cnt = 0;
                    ret = esp_ota_begin(__ota_update_partition, __ota_size, &__ota_update_handle);
                    rq->res_val("ota", ret, 0);
                    if (ret == ESP_OK) { 
                        __ota_active = 1; 
                    } else {
                        msg_error("esp_ota_begin error = %d", ret);
                    }
                }
            } else {
                msg_error("ota_stop(2) error = %d", ret);
                rq->res_val("ota", ESP_FAIL, 0);
            }
        } else {
            if (m_wsCB) m_wsCB(rq);
            if (m_on.size()) {
                char *k = (char *)pl, *x;
                int len = rq->len();
                bool lev = false;
                /* Get first string from array */
                while ((len > 0) && ((*k == '[') || (*k == '"') || (*k == ' '))) { if (*k == '"') lev = true; k++;len--; }
                x = k + 1;len--;
                while ((len > 0) && (*x != '"') && (*x != ',') && ((lev) || (*x != ' '))) { x++; len--; }
                *x = '\0';
                msg_debug("key (%s)", k);
                auto itr = m_on.find(k);
                if ((len > 1) && (itr != m_on.end())) {
                    x++;len--;
                    while ((len > 0) && ((*x == ',') || (*x == ' '))) { x++; len--; }
                    while ((len > 0) && ((x[len - 1] == ']') || (x[len - 1] == ' '))) { x[len - 1] = '\0'; len--; }
                    if (len > 0) {
                        if (itr != m_on.end()) {
                            itr->second(rq, x, len);
                        }
                    }
                }
            }
        }
    } else {
        /* This is binary FRAME :-) */
        if (__ota_active) {
            uint32_t* v = (uint32_t*)rq->payload();
            if (*v == __ota_id) {
                // msg_ota("Write block size = %d", rq->len());
                ret = esp_ota_write(__ota_update_handle, &rq->payload()[4], (rq->len() - 4));
                if (ret != ESP_OK) {
                    msg_error("OTA write error !");
                    rq->res_val("ota", ret, 0);
                    ret = ota_stop(1);
                    __ota_active = 0;
                } else {
                    __ota_cnt += (rq->len() - 4);
                    msg_ota("(%d) Download Progress: %0.2f %%", rq->len(), ((float)(__ota_cnt) / __ota_size) * 100);
                    if (__ota_cnt >= __ota_size) {
                        ret = ota_stop(0);
                        rq->res_val("ota", ret, __ota_size);
                        __ota_active = 0;
                        if (ret == ESP_OK) { reboot(); }
                    } else {
                        /* Config block write OK */
                        rq->res_val("ota", ESP_OK, __ota_cnt);
                    }
                }
            } else {
                msg_error("Bad OTA id (%u != %u)", (unsigned int)*v, (unsigned int)__ota_id);
            }
        } else {
            /* Handle WebSocket binary data frame */
            if (m_wsCB) m_wsCB(rq);
        }
    }

    do_pm_unlock();

    return ret;
}

int Express::ws_connected_clients_count()
{
    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP] = { 0 };
    esp_err_t ret = httpd_get_client_list(m_server, &fds, client_fds);
    int res = 0;
    if (ret != ESP_OK) return 0;
    for (int i = 0; i < fds; i++) {
        httpd_ws_client_info_t client_info = httpd_ws_get_fd_info(m_server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) { res++; }
    }
    return res;
}
/* ========================================================================================== */


//===========================================================================
//===============================--- OTA ---=================================
//===========================================================================


/*!
 * \brief Stop/acquire of finalize OTA update.
 * \param abort (2 - acquire firmware lock, 1 - abort update (ERROR), 0 - finalize update).
 */
esp_err_t Express::ota_stop(uint32_t abort)
{
    esp_err_t ret = ESP_FAIL;

    if (abort == 2) ret = ESP_OK;

    if (__ota_active) {
        if (abort == 2) {
            /* Try to acquire lock Check timeout */
            float elapsed = (float)(esp_timer_get_time() - __ota_start_timestamp) / 1000000L;
            if (elapsed > 60.0f) {
                msg_error("OTA timeout - reset !");
                __ota_active = 0;
                return esp_ota_abort(__ota_update_handle);
            } else {
                msg_error("OTA in progress !");
                return ESP_FAIL;
            }
        } else if (abort) {
            msg_error("OTA error");
            __ota_active = 0;
            return esp_ota_abort(__ota_update_handle);
        } else {
            msg_ota("Time taken to download firmware: %0.3f s", (float)(esp_timer_get_time() - __ota_start_timestamp) / 1000000L);
            msg_ota("Firmware size: %uKB", (unsigned int )(__ota_size / 1024));
            ret = esp_ota_end(__ota_update_handle);
            if (ret == ESP_OK) {
                ret = esp_ota_set_boot_partition(__ota_update_partition);
            }
            msg_ota("OTA result: %d", ret);
        }
    }
    __ota_active = 0;
    return ret;
}
/* ========================================================================================== */

/*!
 * \brief Handle POST buffer (FIRMWARE update).
 */
esp_err_t Express::ota_post_handler(httpd_req_t* req)
{
    char* recv_buf = (char*)::calloc(HTTP_CHUNK_SIZE, sizeof(char));
    int ret, content_length = req->content_len;
    esp_err_t err;

    if (!recv_buf) { return ESP_FAIL; }

    msg_ota("Content length: %d B", content_length);

    int remaining = content_length;

    err = ota_stop(2);
    if (err != ESP_OK) {
        msg_error("OTA stop Error: %d", err);
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    __ota_start_timestamp = esp_timer_get_time();
    __ota_update_partition = esp_ota_get_next_update_partition(NULL);
    __ota_active = 1;
    __ota_size = remaining;

    size_t count = 0;
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, recv_buf, MIN(remaining, HTTP_CHUNK_SIZE))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) { continue; }
            httpd_resp_send_500(req);
            return ESP_OK;
        }
        if (count == 0 && esp_ota_begin(__ota_update_partition, OTA_WITH_SEQUENTIAL_WRITES, &__ota_update_handle) != ESP_OK) {
            httpd_resp_send_500(req);
            return ESP_OK;
        }
        if (esp_ota_write(__ota_update_handle, (const void*)recv_buf, ret) != ESP_OK) {
            ota_stop(1);
            __ota_active = 0;
            httpd_resp_send_500(req);
            return ESP_OK;
        };
        count += ret;
        remaining -= ret;
        memset(recv_buf, 0x00, HTTP_CHUNK_SIZE);
        msg_ota("Download Progress: %0.2f %%", ((float)(content_length - remaining) / content_length) * 100);
    }
    free(recv_buf);

    err = ota_stop(0);
    if (err) {
        msg_error("Error: %d", err);
        httpd_resp_send_500(req);
        return ESP_OK;
    }
    httpd_resp_send(req, NULL, 0);
    reboot();
    return ESP_OK;
}

/*!
 * \brief Generate unique session ID.
 */
std::string Express::generateUUID()
{
    uint32_t v[9];
    struct timeval tv;
    std::string sid(64,0);
    size_t olen = 0;

	gettimeofday(&tv, NULL);
    /* 64 bit timestamp */
    v[0] = tv.tv_sec;
    v[1] = tv.tv_usec;
    /* 196 bit random */
    v[2] = esp_random();
    v[3] = esp_random();
    v[4] = esp_random();
    v[5] = esp_random();
    v[6] = esp_random();
    v[7] = esp_random();
    /* Dummy for pad*/
    v[8] = (uint32_t)this;
    mbedtls_base64_encode((unsigned char *)&sid[0], 64, &olen, (const unsigned char *)&v[0], 33 );
    sid.resize(olen);
    // msg_error("olen = %d, string len = %d, ssid = %s", olen, sid.length(), sid.c_str());
	return sid;
}

#ifdef CONFIG_EXPRESS_USE_AUTH

void Express::cleanupOutdatedSessions()
{
    time_t v = express_get_time_s();
	for (auto i = m_sessions.begin(); i != m_sessions.end();) {
		auto j = i++;
		ExpressSession *s = j->second;
		if (s) {
            if (s->expire < v ) {
			    msg_debug("Delete outdated session %s", j->first.c_str());
			    m_sessions.erase(j);
			    delete s;
            }
		} else {
			msg_debug("Delete empty session %s", j->first.c_str());
			m_sessions.erase(j);
        }
	}
}
//===========================================================================
//=====================--- generic middleware  ---===========================
//===========================================================================
ExpressMidCB Express::getSessionMW(int maxAge)
{
    return [this, maxAge](ExRequest* req) {
    	const char *sessionID;

	    sessionID = req->getCookie("SessionID");
	    if (!sessionID) {
            if ((strcmp(req->uri(),"login")) && (strcmp(req->uri(),"sn")) && (strcmp(req->uri(),"index.html"))) return true;
		    /* Cleanup outdated sessions */
		    this->cleanupOutdatedSessions();
		    /* Generate new session ID */
            std::string uuid = generateUUID();
		    req->m_user["sessionid"] = uuid;
		    req->m_user["cookie"] = "SessionID=" + uuid + "; Max-Age=" + std::to_string(maxAge);
		    req->setCookie(req->m_user["cookie"].c_str());
		    msg_debug("Generate new session ID: %s", uuid.c_str());
	    } else {
            /* Store session ID from cookie */
            std::string sid = std::string(sessionID);
		    msg_debug("Got session ID: %s", sid.c_str());
		    req->m_user["sessionid"] = sid;
            req->m_session = m_sessions[sid];
	    }
	    return true;
    };
}

/*!
 * \brief withAuth middleware.
 */
ExpressMidCB Express::getWithAuthMW()
{
    return [this](ExRequest* req) {
	    ExpressSession *s = req->m_session;	
	    if (s) {
		    /* Check expired */
		    if (s->expire > express_get_time_s() ) {
			    s->ping();
			    return true;		
		    }
		    /* Delete session */
		    m_sessions.erase(req->m_user["sessionid"]);
            req->m_session = NULL;
		    delete s;
	    }
	    req->error(http_401_hdr);
	    return false;
    };
}



bool Express::doLogin(ExRequest* req, std::string user)
{
    /* Create new session */
    std::string sid = req->m_user["sessionid"];
    if (sid.length()) {
        ExpressSession *s = new ExpressSession(user);
        m_sessions[sid] = s;
        req->m_session = s; 
        return true;
    }
    return false;
}

void Express::doLogOut(ExRequest* req)
{
    std::string sid = req->m_user["sessionid"];
	ExpressSession *s = m_sessions[sid];	
	if (s) {
		m_sessions.erase(sid);
        req->m_session = NULL;
		delete s;
	}
}

ExpressPageCB Express::getStdLoginFunction()
{
	return [this](ExRequest* req) {
        bool ok = false;
        std::string user,password;
        req->m_e->doLogOut(req);
        if (req->m_json.is_null()) {
            /* JSON not parsed yet  */
            req->m_json = njson::parse(req->readAll().c_str());
        }
        // msg_error("%s\n", req->m_json.dump().c_str() );
        user = req->m_json["user"].getString();
        password = req->m_json["password"].getString();
        // msg_error("%s %s\n", user.c_str(), password.c_str() );
        if ((user.length()) && (password.length())) {
		    /* Analize user password ... */
            auto k = this->m_passwd.find(user);
            if (k != this->m_passwd.end()) {
		        if (k->second == std::string(password)) {
			        ok = this->doLogin(req, user);
		        }
            }
	    }
	    if (ok) {
		    req->json("{ \"ok\": true}");
	    } else {
		    req->error(http_401_hdr);
	    }
    };
}

#endif

/*!
 * \brief JSON middleware.
 */
ExpressMidCB Express::getJsonMW()
{
    return [this](ExRequest* req) {
        if (req->getMethod() == HTTP_GET) return true;
        if (req->getContentLen() <= 0 ) return true;
        if (req->getContentType().find("json") == std::string::npos) return true;
        req->m_json = njson::parse(req->readAll().c_str());
	    return true;
    };
}


//===========================================================================
//======================--- Request/Response  ---============================
//===========================================================================

typedef enum {
    cgi_load_key,
    cgi_load_value
} cgi_load_t;

void ExRequest::parseURI()
{
    const char* key = NULL, * value = NULL;
    cgi_load_t load;
    int len, i;
    char* qr;

    len = strlen(m_url);

    load = cgi_load_key;
    key = qr = m_url;
    // enumerate string characters
    for (i = 0;i < len;++i) {
        switch (qr[i]) {
            case '?': {
                qr[i] = '\0';
                value = NULL;
                key = &qr[i + 1];
            } break;
            case '=': {
                load = cgi_load_value;
                qr[i] = '\0';
                value = &qr[i + 1];
            } break;
            case ';':
            case '&': {
                if (load == cgi_load_key) {
                    key = &qr[i + 1];
                    value = NULL;
                } else {
                    qr[i] = '\0';
                    load = cgi_load_key;
                    if ((key) && (value)) {
                        /* process key/value */
                        m_query.insert({ key, value });
                    }
                    key = &qr[i + 1];
                    value = NULL;
                }
            } break;
            default: break;
        }
    }
    if ((key) && (value)) {
        /* process key/value */
        m_query.insert({ key, value });
    }
    m_uri = m_url;
    while (*m_uri == '/') m_uri++;
    if (strlen(m_uri) == 0) m_uri = "index.html";
}


/* const httpd related values stored in ROM */
const static char http_200_hdr[] = "200 OK";
const static char http_content_type_html[] = "text/html";
const static char http_content_type_json[] = "application/manifest+json";
const static char http_content_type_js[] = "text/javascript";
const static char http_content_type_image[] = "image/png";
const static char http_cache_control_hdr[] = "Cache-Control";
const static char http_cache_control_cache[] = "public, max-age=31536000";
const static char http_cache_control_no_cache[] = "no-store, no-cache, must-revalidate, max-age=0";
const static char http_pragma_hdr[] = "Pragma";
const static char http_pragma_no_cache[] = "no-cache";
const static char http_content_type_txt[] = "text/plain";
const static char http_set_cookie[] = "Set-Cookie";
const static char http_cookie[] = "Cookie";
const static char http_content_type[] = "Content-Type";


std::string ExRequest::getHeader(const char *key) const
{
    size_t len;
    len = httpd_req_get_hdr_value_len(m_req, key);    
    if (len > 0) {    
        std::string res(len, 0);
        httpd_req_get_hdr_value_str(m_req, key, &res[0], len + 1);
        return res;
    }
    return std::string();  
}

std::string ExRequest::getContentType() const
{
    return getHeader(http_content_type);
}


void ExRequest::setCookie(const char* cookie)
{
    httpd_resp_set_hdr(m_req, http_set_cookie, cookie);
}

void ExRequest::parseCookie() 
{
    const char *key = NULL, *value = NULL;
    cgi_load_t load;
    char* qr;
    size_t i, len;
    
    if (m_cookie_mem) {
        m_cookie.clear();
        free(m_cookie_mem);
        m_cookie_mem=NULL;
    }
    len = httpd_req_get_hdr_value_len(m_req, http_cookie);    
    if (len == 0) return;
    /* Allocate memory for cookie string */
    m_cookie_mem = (char *)malloc(len + 2);
    if (!m_cookie_mem) return;
    /* Get cookie string */
    httpd_req_get_hdr_value_str(m_req, http_cookie, m_cookie_mem, len + 1);
    // msg_error("Got cookie string: %s",m_cookie_mem);
    /* decode cookie string */
    load = cgi_load_key;
    key = qr = m_cookie_mem;
    value = NULL;
    for (i = 0;i < len;++i) {
        switch (qr[i]) {
            case '=': {
                load = cgi_load_value;
                qr[i] = '\0';
                value = &qr[i + 1];
            } break;
            case ';': {
                if (load == cgi_load_key) {
                    key = &qr[i + 1];
                    value = NULL;
                } else {
                    qr[i] = '\0';
                    load = cgi_load_key;
                    if ((key) && (value)) {
                        while (*key == ' ') key++;
                        while (*value == ' ') value++;
                        /* process key/value */
                        // msg_error("Key: %s, value: %s", key, value);
                        m_cookie.insert({ key, value });
                    }
                    key = &qr[i + 1];
                    value = NULL;
                }
            } break;
            default: break;
        }
    }
    if ((key) && (value)) {
        while (*key == ' ') key++;
        while (*value == ' ') value++;
        /* process key/value */
        // msg_error("Key: %s, value: %s", key, value);
        m_cookie.insert({ key, value });
    }
}


void ExRequest::parseParams()
{
    m_param.clear();
    if (m_param_mem) free(m_param_mem);
    if (m_key_mem) free(m_key_mem);
    m_param_mem = strdup(this->uri());
    m_key_mem = strdup(m_key);
    {
        bool final = false;
        char *a = m_key_mem, *b = m_param_mem, *key, *val;
        
	    while ((*a != '\0') && (*b != '\0')) {
    		if (*a == ':') {
                a++;
                key = a;
                val = b;
			    /* Skip section */
			    while ((*a != '/') && (*a != '\0')) a++;
			    while ((*b != '/') && (*b != '\0')) b++;
                if ((*a== '\0') || (*b== '\0')) final = true;
                *a='\0';
                *b='\0';
                msg_error("Key = %s, val = %s", key, val);
                m_param.insert({ key, val });
		    }
            if (final) break;
	    	a++;
		    b++;
	    }
    }
}


esp_err_t ExRequest::json(njson v) 
{ 
    std::string s = v.dump();
    return json(s.c_str(), s.length()); 
}


esp_err_t ExRequest::json(const char* resp, int len)
{
    if (len == 0) len = strlen(resp);
    httpd_resp_set_status(m_req, http_200_hdr);
    httpd_resp_set_type(m_req, http_content_type_json);
    httpd_resp_set_hdr(m_req, http_cache_control_hdr, http_cache_control_no_cache);
    httpd_resp_set_hdr(m_req, http_pragma_hdr, http_pragma_no_cache);
    return httpd_resp_send(m_req, resp, len);
}

esp_err_t ExRequest::txt(const char* resp, int len)
{
    if (len == 0) len = strlen(resp);
    httpd_resp_set_status(m_req, http_200_hdr);
    httpd_resp_set_type(m_req, http_content_type_txt);
    httpd_resp_set_hdr(m_req, http_cache_control_hdr, http_cache_control_no_cache);
    httpd_resp_set_hdr(m_req, http_pragma_hdr, http_pragma_no_cache);
    return httpd_resp_send(m_req, resp, len);
}

esp_err_t ExRequest::send_res(esp_err_t ret)
{
    if (ret == ESP_OK) {
        return json((const char*)"{ \"ok\": true }", 14);
    } else {
        return json((const char*)"{ \"ok\": false }", 15);
    }
}

esp_err_t ExRequest::error(const char *status)
{
    httpd_resp_set_status(m_req, status);
    httpd_resp_set_type(m_req, http_content_type_txt);
    return httpd_resp_send(m_req, NULL, 0);
}


esp_err_t ExRequest::gzip(const char* type, const char* resp, int len)
{
    if (len == 0) len = strlen(resp);
    httpd_resp_set_status(m_req, http_200_hdr);
    httpd_resp_set_type(m_req, type);
    httpd_resp_set_hdr(m_req, http_cache_control_hdr, http_cache_control_cache);
    httpd_resp_set_hdr(m_req, "Content-Encoding", "gzip");
    return httpd_resp_send(m_req, (const char*)resp, len);
}

esp_err_t ExRequest::send(const char* type, const char* resp, int len)
{
    if (len == 0) len = strlen(resp);
    httpd_resp_set_status(m_req, http_200_hdr);
    httpd_resp_set_type(m_req, type);
    httpd_resp_set_hdr(m_req, http_cache_control_hdr, http_cache_control_cache);
    return httpd_resp_send(m_req, (const char*)resp, len);
}


esp_err_t ExRequest::redirect(const char *path, const char *type)
{
    httpd_resp_set_type(m_req, http_content_type_html);
    httpd_resp_set_status(m_req, type);
    httpd_resp_set_hdr(m_req, "Location", path);
    httpd_resp_send(m_req, NULL, 0);
    return ESP_OK;
}

/*!
 * \brief Send command return code and value over websocket.
 */
esp_err_t WSRequest::res_val(const char* fn, esp_err_t ret, uint32_t val)
{
    httpd_ws_frame_t pkt;

    pkt.len = snprintf((char*)m_buf, (WS_MAX_FRAME_SIZE - 1), "{ \"cmd\": \"%s\", \"ret\": %d, \"val\": %u }", fn, ret, (unsigned int)val);
    pkt.payload = (uint8_t*)m_buf;
    pkt.final = true;
    pkt.fragmented = false;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    return httpd_ws_send_frame(m_req, &pkt);
}

/*!
 * \brief Send string over websocket.
 */
esp_err_t WSRequest::send(const char *s, int len)
{
    httpd_ws_frame_t pkt;

    if (len == 0) len = strlen(s);
    pkt.payload = (uint8_t*)s;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    pkt.final = true;
    pkt.fragmented = false;
    pkt.len = len;
    return httpd_ws_send_frame(m_req, &pkt);
}

struct https_async_params {
    httpd_handle_t m_server;
    int len;
    char buf[];
};

/*!
 * \brief Async send to all function, which we put into the httpd work queue
 */
static void httpd_ws_send_data_to_all_clients_int(void* arg)
{
    struct https_async_params* pr = (https_async_params*)arg;
    static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[CONFIG_LWIP_MAX_LISTENING_TCP] = { 0 };
    httpd_ws_frame_t ws_pkt;
    esp_err_t ret = httpd_get_client_list(pr->m_server, &fds, client_fds);
    if (ret != ESP_OK) { ::free((void*)pr); return; }
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)pr->buf;
    ws_pkt.len = pr->len;
    ws_pkt.final = true;
    ws_pkt.fragmented = false;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    for (int i = 0; i < fds; i++) {
        httpd_ws_client_info_t client_info = httpd_ws_get_fd_info(pr->m_server, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            ret = httpd_ws_send_frame_async(pr->m_server, client_fds[i], &ws_pkt);
            if (ret != ESP_OK) {
                msg_error("httpd_ws_send_frame failed with %d = %d", client_fds[i], ret);
            }
        }
    }
    ::free((void*)pr);
}
/* ========================================================================================== */


void WSRequest::send_to_all_clients(const char* buf)
{
    int len = strlen(buf);
    struct https_async_params* pr = (struct https_async_params*)malloc(sizeof(struct https_async_params) + len + 1);
    if (pr) {
        pr->m_server = m_server;
        pr->len = len;
        strcpy(pr->buf, buf);
        httpd_queue_work(m_server, httpd_ws_send_data_to_all_clients_int, pr);
    }
}

// /*!
//  * \brief Async send function, which we put into the httpd work queue
//  */
// static void ws_async_send_state(void *arg)
// {
//     httpd_ws_frame_t ws_pkt;
//     int fd = (int)arg;
//     esp_err_t ret;
//     char state[256];

//     snprintf(state, 255, "{ \"sn\": %u, \"version\": \"1.0.1\" }", SN_int);
//     memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
//     ws_pkt.payload = (uint8_t *)state;
//     ws_pkt.len = strlen(state);
//     ws_pkt.type = HTTPD_WS_TYPE_TEXT;
//     ret = httpd_ws_send_frame_async(http_ota_server_handle, fd, &ws_pkt);
//     if (ret != ESP_OK) {
//         msg_error("httpd_ws_send_frame failed with %d", ret);
//     }
// }
// /* ========================================================================================== */

void Express::ws_send_to_all_clients(const char* buf)
{
    int len = strlen(buf);
    struct https_async_params* pr = (struct https_async_params*)malloc(sizeof(struct https_async_params) + len + 1);
    if (pr) {
        pr->m_server = m_server;
        pr->len = len;
        strcpy(pr->buf, buf);
        httpd_queue_work(m_server, httpd_ws_send_data_to_all_clients_int, pr);
    }
}
