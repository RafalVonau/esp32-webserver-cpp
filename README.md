# Simple WebServer for esp-idf
This project implements basic Web server protocol for esp-idf version 4 and above.

![](https://raw.githubusercontent.com/BubuHub/esp32-webserver-cpp/997cd07ba4c3fa29c57117d3e8e50408098541ac/blob/assets/firmware.png?raw=true)

The functionality of the server is inspired by [express.js](https://expressjs.com/) from [nodejs](https://nodejs.org/).

Example usage:
```cpp
Express e;

/* Parameters by query string  (like /api/test?nr=12) */
e.get("api/test", [](ExRequest* req) {
  int id = req->getArgInt("nr", 0);
  req->json("{ \"test\": true }");
});

/* Parameters by path (like /api/add/10/12 ) */
e.get("api/add/:id/:val", [](ExRequest* req) {
  int id = req->getParamInt("id");
  int val = req->getParamInt("val");
  // .. do something with id and val
  req->json({"ok", true, "id", id, "val", val});
});

/* Ignore section (use *) /api/[anything]/move */
e.get("api/*/move", [](ExRequest* req) {
    req->json("{ \"move\": true }");
});

/* Ignore rest of the path (use #) /api/copy/[any path] */
e.get("api/copy/#", [](ExRequest* req) {
    req->json("{ \"copy\": true }");
});

/* Add session middleware */
bool sessionMiddleware(ExRequest* req) {
	const char *sessionID;

	sessionID = req->getCookie("SessionID");
	if (!sessionID) {
		if (strcmp(req->uri(),"index.html")) return true;
		/* Generate new session ID */	
		std::string uuid = req->m_e->generateUUID();
		req->m_user["sessionid"] = uuid;
		req->m_user["cookie"] = "SessionID=" + uuid + "; Max-Age=2592000";
		req->setCookie(req->m_user["cookie"].c_str());
		msg_debug("Generate new session ID: %s",uuid.c_str());
	} else {
		msg_debug("Got session ID: %s",sessionID);
		req->m_user["sessionid"] = std::string(sessionID);
	}
	return true;
}

/* Empty string "" = execute for any path */
e.use("", sessionMiddleware);

/* Add middleware in .get .post ... methods  */

bool withAuth(ExRequest* req) { .... }

e.get("api/secure", withAuth, [](ExRequest* req) {
    req->json("{ \"copy\": true }");
});

/* Add more middlewares in .get .post ... methods  */
e.get("api/secure", {withAuth, json}, [](ExRequest* req) {
    req->json("{ \"copy\": true }");
});

/* Get data from post request */
e.post("api/login", [](ExRequest* req) {
	bool ok = false;
	std::string json = req->readAll();
	...
});


/* Add static pages compiled from Next.js */
e.addStatic(www_filesystem);

/* start server port = 80, priotity = 7, bind to core ID 1 */
e.start(80, 7, 1);

/* Builtin support for Authorization when CONFIG_EXPRESS_USE_AUTH is defined */
e.use("", e.getSessionMW());

auto withAuth = e.getWithAuthMW();

e.get("api/login", e.getStdLoginFunction());

e.get("api/logout", [](ExRequest* req) {
	e.doLogOut(req);
	req->json("{ \"ok\": true }");
});

```
See more details in examples folder.

# Configurable parameters
Configure WiFi SSID/PASSWORD in menuconfig or by manually editing sdkconfig.defaults.

* CONFIG_EXAMPLE_WIFI_SSID
* CONFIG_EXAMPLE_WIFI_PASSWORD

# Building under Linux
* install PlatformIO
* enter examples/simple directory
* type in terminal:
  platformio run -t upload
* type http://express.local/ or http://express.lan/ in the web browser.

You can also use IDE to build this project on Linux/Windows/Mac. My fvorite one:
* [Code](https://code.visualstudio.com/) 

# Example page
The page is based on [Next.js](https://nextjs.org/) and [Mantine UI](https://mantine.dev/).

## Building page under linux (requires nodejs instalation and perl interpreter)
* cd www-next
* npm i
* ./compile.sh
* ./generate_www

Enjoy :-)
