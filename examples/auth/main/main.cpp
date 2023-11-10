#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <map>
#include <set>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include "sdkconfig.h"
#include <esp_event.h>
#include <esp_event_loop.h>
#include <string>
#include "express.h"
#include "mdns.h"
#include "protocol_examples_common.h"
#include "www_fs.h"
#include "cJSON.h"
#include "ramlog.h"

static char tag[] = "EXM";
#if 1
#define msg_info(fmt, args...) ESP_LOGI(tag, fmt, ## args);
#define msg_init(fmt, args...)  ESP_LOGI(tag, fmt, ## args);
#define msg_debug(fmt, args...)  ESP_LOGI(tag, fmt, ## args);
#define msg_error(fmt, args...)  ESP_LOGE(tag, fmt, ## args);
#else
#define msg_info(fmt, args...)
#define msg_init(fmt, args...)
#define msg_debug(fmt, args...)
#define msg_error(fmt, args...)  ESP_LOGE(TAG, fmt, ## args);
#endif


Express e;


#define INACTIVE_TIME_S (3600)
class SessionData {
public:
	SessionData() { logged_in = false; }
	SessionData(std::string n) { logged_in = true;  userName = n; expire = express_get_time_s() + INACTIVE_TIME_S; }
	void ping() {
		expire = express_get_time_s() + INACTIVE_TIME_S;
	}
	std::string userName;
	time_t expire;
	bool logged_in;
};

std::map<std::string, SessionData*> m_sessions;

void cleanupOutdated()
{
	time_t v = express_get_time_s();
	for (auto i = m_sessions.begin(); i != m_sessions.end();) {
		auto j = i++;
		SessionData* s = j->second;
		if (s->expire < v) {
			msg_debug("Delete outdated session %s", j->first.c_str());
			m_sessions.erase(j);
			delete s;
		}
	}
}


/*!
 * \brief Session middleware.
 */
bool sessionMiddleware(ExRequest* req) {
	const char* sessionID;

	sessionID = req->getCookie("SessionID");
	if (!sessionID) {
		if ((strcmp(req->uri(), "login")) && (strcmp(req->uri(), "index.html"))) return true;
		/* Cleanup outdated sessions */
		cleanupOutdated();
		/* Generate new session ID */
		std::string uuid = req->m_e->generateUUID();
		req->m_user["sessionid"] = uuid;
		req->m_user["cookie"] = "SessionID=" + uuid + "; Max-Age=2592000";
		req->setCookie(req->m_user["cookie"].c_str());
		msg_debug("Generate new session ID: %s", uuid.c_str());
	} else {
		msg_debug("Got session ID: %s", sessionID);
		req->m_user["sessionid"] = std::string(sessionID);
	}
	return true;
}

/*!
 * \brief withAuth middleware.
 */
bool withAuth(ExRequest* req) {
	std::string sid = req->m_user["sessionid"];
	SessionData* s = m_sessions[sid];
	if (s) {
		/* Check expired */
		if ((s->logged_in) && (s->expire > express_get_time_s())) {
			s->ping();
			return true;
		}
		/* Delete session */
		m_sessions.erase(sid);
		delete s;
	}
	req->error("401 Unauthorized");
	return false;
}


void SETUP_task(void* parameter)
{
	e.use("", sessionMiddleware);

	e.get("api/info", withAuth, [](ExRequest* req) {
		req->json("[{\"k\": \"Serial number\", \"v\": 1},{\"k\": \"Firmware\", \"v\": \"ESP32 test v1.0.0\"} ]");
	});

	e.get("api/network", withAuth, [](ExRequest* req) {
		req->json("{\"ip\": \"192.168.124.227\", \"netmask\": \"255.255.255.0\", \"gateway\": \"\", \"dhcp\": \"STATIC\", \"ntp\": \"NONTP\", \"ntps\": \"\" }");
	});
	
	e.get("api/log", [](ExRequest* req) {
		req->txt(RAMLog::instance()->read().c_str());
	});

	e.post("api/login", [](ExRequest* req) {
		bool ok = false;
		std::string json = req->readAll();
		cJSON* j = cJSON_Parse(json.c_str());
		if ((cJSON_GetObjectItem(j, "user")) && (cJSON_GetObjectItem(j, "password"))) {
			char* user = cJSON_GetObjectItem(j, "user")->valuestring;
			char* password = cJSON_GetObjectItem(j, "password")->valuestring;
			msg_debug("Got user = %s, pass = %s", user, password);
			/* Analize user password ... */
			if ((!strcmp(user, "admin")) && (!strcmp(password, "admin"))) {
				ok = true;
				m_sessions[req->m_user["sessionid"]] = new SessionData(user);
			}
		}
		cJSON_Delete(j);
		if (ok) {
			req->json("{ \"ok\": true}");
		} else {
			req->error("403 Forbidden");
		}
	});

	e.get("api/logout", [](ExRequest* req) {
		std::string sid = req->m_user["sessionid"];
		SessionData* s = m_sessions[sid];
		if (s) {
			m_sessions.erase(sid);
			delete s;
		}
	});

	e.addStatic(www_filesystem);

	e.on("test", [](WSRequest* req, char* arg, int arg_len) {
		std::string s(arg, arg_len);
		msg_debug("Got test value <%s>", s.c_str());
		req->send(s.c_str());
	});
	vTaskDelay(100);
	e.start(80, 0, 1);
	// e.start(80, 7, 1); // More speed
	vTaskDelete(NULL); /* Delete SETUP task */
}

/*!
 * \brief MAIN.
 */
extern "C" void app_main(void)
{
	esp_chip_info_t chip_info;
	std::string m_hostname = "express";
	// esp_pm_config_esp32_t m_pcfg;

	/* Activate RAM log */
	RAMLog::instance()->install();


	/* Print chip information */
	esp_chip_info(&chip_info);
	ESP_LOGI(tag, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ", chip_info.cores, (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
	ESP_LOGI(tag, "silicon revision %d, ", chip_info.revision);
	// ESP_LOGI(tag,"CPU frequency: %d, Maximum priority level: %d, IDLE priority level: %d",getCpuFrequencyMhz(), configMAX_PRIORITIES - 1, tskIDLE_PRIORITY);
	ESP_LOGI(tag, "main_task: active on core %d", xPortGetCoreID());


	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// m_pcfg.light_sleep_enable = false;
	// m_pcfg.max_freq_mhz = 80;
	// m_pcfg.min_freq_mhz = 80;
	// ESP_ERROR_CHECK(esp_pm_configure(&m_pcfg));


	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	//initialize mDNS service
	ESP_ERROR_CHECK(mdns_init());
	mdns_hostname_set(m_hostname.c_str());
	mdns_instance_name_set(m_hostname.c_str());
	mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
	msg_info("Host name: %s.local\n", m_hostname.c_str());

	xTaskCreatePinnedToCore(SETUP_task, "SETUP", 10000, NULL, /* prio */ 0, /* task handle */ NULL, 1);
	vTaskDelete(NULL); /* Delete SETUP task */
}
