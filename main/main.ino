#include <WiFi.h>
#include <Time.h>
#include "SimpleTimer.h"
#include "QList.h"
#include "ESPAsyncWebServer.h"
#include "ping.h"
#include "SPIFFS.h"
#include "Vector.h"
#include "HTTPClient.h"
#include "ArduinoJSON/ArduinoJson.h"


#define CONFIG_FILE "/config.cfg"
#define SCHEDULE_FILE "/schedule.cfg"
#define DEVICE_STATE_FILE "/device.cfg"

#define URL_TEST_INTERNET "http://103.236.201.99/Device/TestInternet"
#define URL_LOGIN "http://103.236.201.99/Token/Create"
#define URL_REGISTER_DEVICE "http://103.236.201.99/Device/RegisterDevice"
#define URL_STATUS_DEVICE "http://103.236.201.99/Device/GetDeviceById/"
#define URL_STATUS_UPDATE_DEFAULT_DEVICE "http://103.236.201.99/Device/SendDefaultCommand/"
#define URL_SCHEDULE_DEVICE "http://103.236.201.99/Device/GetSchedulesByIdSimple/"
#define URL_DETAILED_SCHEDULE_DEVICE "http://103.236.201.99/Device/GetDetailedScheduleById/" 

#define STATE_READY "READY"
#define STATE_STATE_WATERING_ONDEMAND "STATE_WATERING_ONDEMAND"
#define STATE_REBOOT "STATE_WATERING_ONDEMAND"

#define TIME_SERVER_NTP "pool.ntp.org"
#define TIME_DEFAULT_GMTOFFSET  28800
#define TIME_DEFAULT_DAYLIGHTOFFSET  3600

#define PORT_WATERING_SOLENOID 27
#define PORT_LED_READY 28
#define PORT_LED_ERROR 29

#define HARDWARE_VERSION "Sirmoto-EZ"


const char *g_ssid = "MyESP32AP";
const char *g_password = "testpassword";

struct WifiCredential
{
	String ssid;
	String password;
};

struct DeviceInfo
{
	String token;
	int currentDeviceId;
	String currentMode;
	String currentdeviceName;
	String currentWeatherLocation;
	String currentInfo;
};

struct DeviceState
{	
	String mode;
};

struct DeviceSchedule
{
	String Id;
	String TimeStart;
	String TimeDuration;	
};

AsyncWebServer server(80);
SimpleTimer timer;
DeviceInfo g_deviceInfo;
QList<DeviceSchedule> g_deviceSchedules;



void SSIDReset()
{
	File file = SPIFFS.open(CONFIG_FILE, "w");
 
    file.println("{ \"ssid\":\"ssid\",\"password\":\"password\" }");
 
    file.close();
}

String PrintDate()
{
	String out = "";
	out += String(day()) + "/";
	if(month()<10)
		out += "0" + String(month()) + "/";
	else
		out += String(month()) + "/";
	out +=  String(year());
	return out;
}

String PrintTime()
{
	String out = "";
	if(hour()<12)
		out += "0" + String(hour()) + ":";
	else
		out += String(hour()) + ":";
	if(minute()<10)
		out += "0" + String(minute()) + ":";
	else
		out += String(minute()) + ":";
	if(second()<10)
		out += "0" + String(second());
	else
		out += String(second());
	return out;
}

void ClearConfig()
{
	File file = SPIFFS.open(DEVICE_STATE_FILE, "w");
 
    file.println("");
 
    file.close();
}


String Head()
{
	
	String out;
	out = " ";
	out += "	<html>";
	out +="		<head>";
	out +="			<style>";
	out +="				body{";
	out +="				}";
	out +="				body{";
	out +="				}";
	out +="			</style>";
	out +="		</head>";
	out +="		<body>";
	out +="			<div>";
	out +="				<span><a href=\"index\" >Home</a></span> | ";
	out +="				<span><a href=\"settings\">Setting</a></span> | ";
	out +="				<span><a href=\"account\">Account</a></span>  " ;
	out +="				<span style=\"float:right\">"+ PrintTime() + " " +  PrintDate() +"</span>  " ;
	out +="			</div>	";
	return out;
}


String Foot()
{
	String out;
	out = " " ;
	out +="		</body>";
	out +="	</html>";
	return out;
}

String RandomString()
{
	String out = "";
	for(auto i=0;i<10;i++)
	{
		char c = random(65, 90);
		out += c;
	}
	return out;
}

void PeriodicProcedure()
{
	Serial.println("running!");
}

void WateringProcedureTurnOffValve()
{
	Serial.println("WateringProcedureTurnOffValve() closing valve");
	digitalWrite(PORT_WATERING_SOLENOID, false);
	Serial.println("WateringProcedureTurnOffValve() updating device state");
	if(AttemptUpdateDeviceState())
	{
		Serial.println("WateringProcedureTurnOffValve() update success");
	}
	else Serial.println("WateringProcedureTurnOffValve() update failure, state might still on STATE_WATERING_ONDEMAND");
}

void WateringProcedure()
{
	digitalWrite(PORT_WATERING_SOLENOID, true);
	Serial.println("WateringProcedure() watering now");
	timer.setTimer(1000*60*1, WateringProcedureTurnOffValve, 1);
	Serial.println("WateringProcedure() enabled turn off valve in 5 mins");
}

bool InternetConnectionTest()
{
	HTTPClient http;
	bool result = false;
    http.begin(URL_TEST_INTERNET); //HTTP

    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] InternetConnectionTest() GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			if(payload == "ok")
				result = true;
			Serial.println(payload);
		}
	}
	else
	{
		Serial.printf("[HTTP] InternetConnectionTest() GET... code: %d\n", httpCode);
	}
	return result;
}

bool AttemptLogin(String user, String password)
{
	HTTPClient http;
	bool result = false;
	String out;
	DynamicJsonBuffer jb;
    JsonObject &obj = jb.createObject();
	obj["Email"] = user;
	obj["Password"] = password;
	obj.printTo(out);
	Serial.println(out);

	Serial.print("[HTTP] AttemptLogin() begin...\n");
    http.begin(URL_LOGIN); //HTTP
	http.addHeader("Content-Type", "application/json");
    Serial.print("[HTTP] AttemptLogin()  POST...\n");
    int httpCode = http.POST(out);
	
    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] AttemptLogin()  POST... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			result = true;
			Serial.println(payload);
			g_deviceInfo.token= payload;
			g_deviceInfo.currentdeviceName = "Sirmoto Device "+RandomString();
			g_deviceInfo.currentMode = "SCHEDULED";
			AttemptRegisterDevice();
		}
	}
	else
	{
		Serial.printf("[HTTP] AttemptLogin()  POST... code: %d\n", httpCode);
	}
	return result;
}

bool AttemptRegisterDevice()
{
	HTTPClient http;
	bool result = false;
	String out, out2;
	DynamicJsonBuffer jb;
    JsonObject &obj = jb.createObject();
	DynamicJsonBuffer jb2;
    JsonObject &obj2 = jb2.createObject();
	obj["DeviceName"] = g_deviceInfo.currentdeviceName;
	obj2["Mode"] = g_deviceInfo.currentMode;
	obj2.printTo(out2);
	obj["DeviceState"] = out2;
	obj.printTo(out);
	Serial.println(out);
	//g_deviceInfo = device;

	Serial.print("[HTTP] AttemptRegisterDevice()  begin...\n");
    http.begin(URL_REGISTER_DEVICE); //HTTP
	http.addHeader("Content-Type", "application/json");
	http.addHeader("Authorization", "Bearer " + g_deviceInfo.token);
    Serial.print("[HTTP] AttemptRegisterDevice()  POST...\n");
    int httpCode = http.POST(out);
	
    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] AttemptRegisterDevice()  POST... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			DynamicJsonBuffer jb2;
    		JsonObject &obj2 = jb2.parseObject(payload.c_str());
			if (!obj2.success()) 
			{
				Serial.println("[JSON] parse at function AttemptRegisterDevice() failed, JSON string is " + payload);				
			}
			else
			{	
				g_deviceInfo.currentDeviceId = obj2["Sirmoto"]["Id"].as<int>();
				
				result = true;
				SaveDeviceState(g_deviceInfo);
			}
			Serial.println(payload);
		}
	}
	else
	{
		Serial.printf("[HTTP] AttemptRegisterDevice()  POST... code: %d\n", httpCode);
	}
	return result;
}

bool AttemptGetDeviceState()
{
 bool result = false;
	HTTPClient http;
	http.begin(URL_STATUS_DEVICE + String(g_deviceInfo.currentDeviceId)); //HTTP
	http.addHeader("Content-Type", "application/json");
	http.addHeader("Authorization", "Bearer " + g_deviceInfo.token);
    Serial.print("[HTTP] AttemptGetDeviceState() GET... with bearer: ");
    Serial.println(g_deviceInfo.token);
    int httpCode = http.GET();
	
    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] AttemptGetDeviceState() GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			DynamicJsonBuffer jb2;
    		JsonObject &obj2 = jb2.parseObject(payload.c_str());
			if (!obj2.success()) 
			{
				Serial.println("[JSON] parse at function AttemptGetDeviceState() failed, JSON string is " + payload);				
			}
			else
			{
				g_deviceInfo.currentInfo = obj2["DeviceInfo"].as<char*>();
				g_deviceInfo.currentdeviceName = obj2["DeviceName"].as<char*>();
				SaveDeviceState(g_deviceInfo);
				result = true;
			}
			
			Serial.println(payload);
		}
	}
	else
	{
		Serial.printf("[HTTP] AttemptGetDeviceState() GET... code: %d\n", httpCode);
	}
	if(g_deviceInfo.currentInfo == STATE_STATE_WATERING_ONDEMAND)
	{
		WateringProcedure();
	}
 Serial.print("[HTTP] AttemptGetDeviceState() ");
 Serial.println(g_deviceInfo.currentInfo);
	return result;
}

bool AttemptUpdateDeviceState()
{
	 bool result = false;
	HTTPClient http;
	http.begin(URL_STATUS_UPDATE_DEFAULT_DEVICE+ String(g_deviceInfo.currentDeviceId)); //HTTP
	http.addHeader("Content-Type", "application/json");
	http.addHeader("Authorization", "Bearer " + g_deviceInfo.token);
    Serial.print("[HTTP] AttemptUpdateDeviceState() GET...");
	Serial.println(g_deviceInfo.currentDeviceId);
    int httpCode = http.GET();
	
    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] AttemptUpdateDeviceState() GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			Serial.println(payload);
      result = true;
		}
	}
	else
	{
		Serial.printf("[HTTP] AttemptUpdateDeviceState() GET... code: %d\n", httpCode);
	}
	return result;
}

bool AttemptGetSchedules()
{
	LoadDeviceState();
 	bool result = false;
	HTTPClient http;
	//String url = "/" + dev.currentDeviceId;
	Serial.println(URL_SCHEDULE_DEVICE);
	
	Serial.println(URL_SCHEDULE_DEVICE + String(g_deviceInfo.currentDeviceId));
	http.begin(URL_SCHEDULE_DEVICE + String(g_deviceInfo.currentDeviceId)); //HTTP
	//http.addHeader("Content-Type", "application/json");
	http.addHeader("Authorization", "Bearer " + g_deviceInfo.token);
    Serial.print("[HTTP] AttemptGetSchedules() GET...\n");
    int httpCode = http.GET();
	
    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP]  AttemptGetSchedules() GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			DynamicJsonBuffer jb2;
    		JsonArray &arr = jb2.parseArray(payload.c_str());
			if (!arr.success()) 
			{
				Serial.println("[JSON] parse at function AttemptRegisterDevice() failed, JSON string is " + payload);				
			}
			else
			{
				auto count = arr.size();
				g_deviceSchedules.clear();
				for(auto i =0;i<count;i++)
				{
          			String id = arr[i].as<char*>();
					Serial.println("Getting schedule id nr " +id);
					DeviceSchedule sched = AttemptGetScheduleDetailById(arr[i].as<char*>());
					g_deviceSchedules.push_front(sched);
				}
				result = true;
			}
			Serial.println(payload);
		}
	}
	else
	{
		Serial.printf("[HTTP]  AttemptGetSchedules() GET... code: %d\n", httpCode);
	}
	return result;
}


DeviceSchedule AttemptGetScheduleDetailById(String id)
{
	HTTPClient http;
	http.begin(URL_DETAILED_SCHEDULE_DEVICE + id); //HTTP
	http.addHeader("Authorization", "Bearer " + g_deviceInfo.token);
    Serial.print("[HTTP]  AttemptGetScheduleDetailById() GET...\n");
    int httpCode = http.GET();
	DeviceSchedule schedule;
    // httpCode will be negative on error
    if(httpCode > 0) 
	{
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] AttemptGetScheduleDetailById() GET... code: %d\n", httpCode);

        // file found at server
        if(httpCode == HTTP_CODE_OK) 
		{
            String payload = http.getString();
			DynamicJsonBuffer jb2;
    		JsonObject &obj2 = jb2.parseObject(payload.c_str());
			if (!obj2.success()) 
			{
				Serial.println("[JSON] parse at function AttemptGetScheduleDetailById(" + id+") failed, JSON string is " + payload);				
			}
			else
			{
				schedule.Id = obj2["Id"].as<char*>();
				schedule.TimeStart = obj2["Time"].as<char*>();
				schedule.TimeDuration = obj2["Duration"].as<char*>();
			}

			Serial.println(payload);
		}
	}
	else
	{
		Serial.printf("[HTTP] AttemptGetScheduleDetailById() POST... code: %d\n", httpCode);
	}
	return schedule;
}

void SaveSchedulesId()
{

}

void SaveWifiPassword(String ssid, String pass)
{
	File file = SPIFFS.open(CONFIG_FILE, "w");
 	Serial.println("WRITING");
    if(!file)
	{
        Serial.println("There was an error opening the file for writing");
        return;
    }
	String out;
	DynamicJsonBuffer jb;
    JsonObject &obj = jb.createObject();
	obj["ssid"] = ssid;
	obj["password"] = pass;
	obj.printTo(out);
	file.println(out);
	Serial.println("DONE");
    file.close();
 
	Serial.println("DONE");
	ESP.restart();
}


void SaveDeviceState(DeviceInfo device)
{
	File file = SPIFFS.open(DEVICE_STATE_FILE, "w");
	String out;
	DynamicJsonBuffer jb;
	if(!file)
	{
		Serial.println("fail to open ");
		Serial.println(DEVICE_STATE_FILE);
		return;
	}
	JsonObject &obj = jb.createObject();
	obj["token"] = device.token;
	obj["DeviceId"] = device.currentDeviceId;
	obj["currentMode"] = device.currentMode;
	obj["currentWeatherLocation"] = device.currentWeatherLocation;
	obj.printTo(out);
	Serial.println( "SaveDeviceState() =>" + out + " <=eof");
	file.println(out);
	file.close();
}

void LoadDeviceState()
{
	File file = SPIFFS.open(DEVICE_STATE_FILE);
	DynamicJsonBuffer jb;
	if(!file)
	{
		Serial.print("fail to open ");
		Serial.println(DEVICE_STATE_FILE);
		return;
	}
	else
	{
		file.available();
		String out = file.readStringUntil('\0');
		Serial.println( "LoadDeviceState() =>" + out + " <=eof");
		JsonObject &obj = jb.parseObject(out.c_str());
		if (!obj.success()) 
		{
			Serial.println("[JSON]1 parse at procedure LoadDeviceState() failed, JSON string is " + out);
			file.close();
			return;
		}
		Serial.println( "LoadDeviceState(), obj[\"token\"].as<char*>() =>" + String(obj["token"].as<char*>()));
		g_deviceInfo.token = obj["token"].as<char*>();
		g_deviceInfo.currentDeviceId = obj["DeviceId"].as<int>();
		g_deviceInfo.currentdeviceName = obj["DeviceName"].as<char*>();
		g_deviceInfo.currentMode = obj["currentMode"].as<char*>();
		/*String mode = obj["currentMode"].as<char*>();
		DynamicJsonBuffer jb2;
		JsonObject &obj2 = jb2.parseObject(mode);
		if (!obj2.success()) 
		{
			Serial.println("[JSON]2 parse at procedure LoadDeviceState() failed, JSON string is " + mode);
			file.close();
			return device;
		}
		device.currentMode = obj2["Mode"].as<char*>();*/
		g_deviceInfo.currentWeatherLocation = obj["currentWeatherLocation"].as<char*>();
	}
	file.close();
	return;
}

WifiCredential LoadWifiConfig()
{
	WifiCredential cred;
	const int capacity=JSON_OBJECT_SIZE(2);
	DynamicJsonBuffer jb;
	File file = SPIFFS.open(CONFIG_FILE, "r");
	if (file)
	{
		file.available();
		String out = file.readStringUntil('\0');
		JsonObject &obj = jb.parseObject(out.c_str());
		if (!obj.success()) 
		{
			Serial.println("[JSON] parse at procedure LoadWifiConfig() failed, JSON string is " + out);
			file.close();
			return cred;
		}
		cred.ssid = obj["ssid"].as<char*>();
		cred.password = obj["password"].as<char*>();
		Serial.println(cred.ssid);
		Serial.println(cred.password);
		/*auto i = 0;
		
		cred.ssid = file.readStringUntil('\n');
		file.available();
		cred.password = file.readStringUntil('\n');
		cred.ssid.remove(cred.ssid.length()-1);
    	cred.password.remove(cred.password.length()-1);*/
	}
	file.close();
	return cred;
}

String printLocalTime()
{
	tm timeInfo;
	if(!getLocalTime(&timeInfo))
	{
		Serial.println("Failed to obtain time");
		return "";
	}
 String tm_wday = String(timeInfo.tm_wday);
 String month = String(timeInfo.tm_mon);
 String tm_year = String(timeInfo.tm_year);
 String tm_hour = String(timeInfo.tm_hour);
 String tm_min = String(timeInfo.tm_min);
 String tm_sec = String(timeInfo.tm_sec);
	return String(tm_wday+ "/" + month + "/"+ tm_year + " " + tm_hour + ":" + tm_min + ":" + tm_sec );
}

void TestJSON()
{
	const int capacity=JSON_OBJECT_SIZE(2);
	String out;
	StaticJsonBuffer<capacity> jb;
    JsonObject &obj = jb.createObject();
	obj["pass"] = "pass";
	obj["ssid"] = "ssid";
	obj.printTo(out);
	Serial.println(out);
}

void setup()
{
	Serial.begin(115200);
	//WiFi.begin(String("ProclubJuara").c_str(), String("udahdigantikemarin").c_str());
	
	String wifi, password;
	Serial.println();
	Serial.println("IP address: ");
	SPIFFS.begin(true);
	//SSIDReset();
	WifiCredential c;
	c = LoadWifiConfig();
	Serial.println(c.ssid);
	Serial.println(c.password);
	wifi = c.ssid;
	password = c.password;
	//SSIDReset();
	if(wifi != "")
	{
		WiFi.begin(wifi.c_str(), password.c_str());
		Serial.println(WiFi.BSSIDstr());
		//WiFi.softAPdisconnect(true);
		auto attempt = 0;
   		///WiFi.begin(String("ProclubJuara").c_str(), String("udahdigantikemarin").c_str());
		Serial.println("Connecting to WiFi..");
		while (WiFi.status() != WL_CONNECTED)
		{
    		delay(500);
			Serial.print(".");
			attempt++;
			if(attempt>100) break;
		}
		Serial.println("");
		if(attempt>100)
		{
			WiFi.softAP(g_ssid, g_password);		
			Serial.println(WiFi.softAPIP());
		}
		Serial.println(WiFi.localIP());
	}
	else
  	{
		Serial.println("This should not be executed!");
		  WiFi.softAP(g_ssid, "");    
    	Serial.println(WiFi.softAPIP());
      Serial.println("WIFI SHOULD BE ON NOW");
  	}
	Serial.println(WiFi.softAPIP());
	server.on("/hello", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(200, "text/plain", "Hello World");
	});

	server.on("/login", HTTP_GET,CredentialSetup );
	server.on("/setupWifi", HTTP_GET,SetupWifi );
	server.on("/post1", HTTP_POST, [](AsyncWebServerRequest *request){
    String message;
    if (request->hasParam("PARAM_MESSAGE", true))
	{
        message = request->getParam("PARAM_MESSAGE", true)->value();
    } 
	else
	{
        message = "No message sent";
    }
    request->send(200, "text/plain", "Hello, POST: " + message);
	});
	server.on("/post", HTTP_POST, PostTest);
	server.on("/saveWifi", HTTP_POST, SaveSetupWifi);
	server.on("/saveCredentials", HTTP_POST, SaveCredentialSetup); 
	server.on("/readConfig", HTTP_GET, ReadConfigFile);
	server.on("/testInternet", HTTP_GET, TestInternet);
	server.on("/checkSchedules", HTTP_GET, CheckSchedules);
	server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){
		ClearConfig();
		request->send(200, "text/plain", "cleared");
	});
	server.begin();
	configTime(TIME_DEFAULT_GMTOFFSET, TIME_DEFAULT_DAYLIGHTOFFSET, TIME_SERVER_NTP);
	LoadDeviceState();
	timer.setInterval(1000*60,AttemptGetDeviceState);
	Serial.println("READY");
}



void ReadConfigFile(AsyncWebServerRequest *request)
{
	String out;
	File file2 = SPIFFS.open(CONFIG_FILE,"r");
     
    if(!file2)
	{
        Serial.println("Failed to open file for reading");
        return;
    }
 
    Serial.println("File Content:");
     
    while(file2.available())
	{
        Serial.println(file2.readStringUntil('\0'));
    	out += file2.read();
    }
 
    file2.close();
	request->send(200, "text/html", out);
}

void TestInternet(AsyncWebServerRequest *request)
{
	InternetConnectionTest();
	request->send(200, "text/html", "ok this is test");
}

void SetupWifi(AsyncWebServerRequest *request)
{
	String out = Head();
	out += "<form method='post'  action='saveWifi'>";
		out += "Select: <select name='wifi'>";	
		auto n =WiFi.scanNetworks();
		for(auto i = 0; i < n; i++)
		{
			out += "<option value='"+WiFi.SSID(i)+"'>"; 
			out += WiFi.SSID(i);
			out += " (singal " + String(WiFi.RSSI(i));
			if(WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
				out += ") open ";  
			else 
				out += ") password ";	
			out += "</option>";
		}
		
		out += "</select>";
		out += "Password: <input type='password' name='password' />";
		out += "<button type='submit'>Save</button>";
	out += "</form>";
	out += Foot();
	request->send(200, "text/html", out);
}


void SaveCredentialSetup(AsyncWebServerRequest *request)
{
	String email, password;
	AsyncResponseStream *response = request->beginResponseStream("text/html");
	int params = request->params();
	for(int i=0;i<params;i++)
	{
		AsyncWebParameter* p = request->getParam(i);
		if (p->isPost())
		{
			response->printf("<li>POST[%s]: %s</li>", p->name().c_str(), p->value().c_str());
			if(p->name().c_str()=="email" && p->value().c_str()=="") 
			{
				response->printf("invalid wifi");
				request->send(response);
			}
			if(p->name()=="password") 
			{
				password = p->value().c_str();
				Serial.println("password = " + password);
			}
			if(p->name()=="email")
			{	
				email = p->value().c_str();
				Serial.println("email	 = " +email);
			} 


		} 
		else
		{
			response->printf("<li>GET[%s]: %s</li>", p->name().c_str(), p->value().c_str());
		}
	}
	request->send(response);
	AttemptLogin(email, password);
	//SaveWifiPassword(wifi, password);	
}

void SaveSetupWifi(AsyncWebServerRequest *request)
{
	String wifi, password;
	AsyncResponseStream *response = request->beginResponseStream("text/html");
	int params = request->params();
	for(int i=0;i<params;i++)
	{
		AsyncWebParameter* p = request->getParam(i);
		if (p->isPost())
		{
			response->printf("<li>POST[%s]: %s</li>", p->name().c_str(), p->value().c_str());
			if(p->name().c_str()=="wifi" && p->value().c_str()=="") 
			{
				response->printf("invalid wifi");
				request->send(response);
			}
			if(p->name()=="password") 
			{
				password = p->value().c_str();
				Serial.println("wifi = " + wifi);
				Serial.println("p->value().c_str()" + p->value());
			}
			if(p->name()=="wifi")
			{	
				wifi = p->value().c_str();
				Serial.println("password	 = " +password);
				Serial.println("p->value().c_str()" + p->value());
			} 


		} 
		else
		{
			response->printf("<li>GET[%s]: %s</li>", p->name().c_str(), p->value().c_str());
		}
	}
	request->send(response);
	SaveWifiPassword(wifi, password);
}



void PostTest(AsyncWebServerRequest *request)
{
	AsyncResponseStream *response = request->beginResponseStream("text/html");
	response->addHeader("Server","ESP Async Web Server");
	response->printf("<!DOCTYPE html><html><head><title>Webpage at %s</title></head><body>", request->url().c_str());

	response->print("<h2>Hello ");
	response->print(request->client()->remoteIP());
	response->print("</h2>");
	response->print("<h3>General</h3>");
	response->print("<ul>");
	response->printf("<li>Version: HTTP/1.%u</li>", request->version());
	response->printf("<li>Method: %s</li>", request->methodToString());
	response->printf("<li>URL: %s</li>", request->url().c_str());
	response->printf("<li>Host: %s</li>", request->host().c_str());
	response->printf("<li>ContentType: %s</li>", request->contentType().c_str());
	response->printf("<li>ContentLength: %u</li>", request->contentLength());
	response->printf("<li>Multipart: %s</li>", request->multipart()?"true":"false");
	response->print("</ul>");

	response->print("<h3>Headers</h3>");
	response->print("<ul>");
	int headers = request->headers();
	for(int i=0;i<headers;i++)
	{
		AsyncWebHeader* h = request->getHeader(i);
		response->printf("<li>%s: %s</li>", h->name().c_str(), h->value().c_str());
	}
	response->print("</ul>");

	response->print("<h3>Parameters</h3>");
	response->print("<ul>");
	int params = request->params();
	for(int i=0;i<params;i++)
	{
		AsyncWebParameter* p = request->getParam(i);
		if(p->isFile())
		{
			response->printf("<li>FILE[%s]: %s, size: %u</li>", p->name().c_str(), p->value().c_str(), p->size());
		}
		 else if (p->isPost())
		{
			response->printf("<li>POST[%s]: %s</li>", p->name().c_str(), p->value().c_str());
		} 
		else
		{
			response->printf("<li>GET[%s]: %s</li>", p->name().c_str(), p->value().c_str());
		}
	}
	response->print("</ul>");

	response->print("</body></html>");
	//send the response last
	request->send(response);
}


void CheckSchedules(AsyncWebServerRequest *request)
{
	String out = Head();
	if(InternetConnectionTest())
	{
		out += "Populating...<br>";
		if(AttemptGetSchedules())
			out += "Done.<br>";
		else
			out += "Problem detected.<br>";
		out += "<table style=\"width:100%; text-align: center;\">";
		out += "<tr>";
		out += "<th>No</th>";
		out += "<th>Schedule Starts</th> ";
		out += "<th>Duration</th> ";
		out += "</tr>";
		auto count = g_deviceSchedules.size();
		for(auto i = 0; i < count; i++)
		{
			out += "<tr>";
			out += "<td>" + String(i) + "</td>";
			out += "<td>" + g_deviceSchedules.get(i).TimeStart + "</td>";
			out += "<td>" + g_deviceSchedules.get(i).TimeDuration + "</td>";
			out += "</tr>";
		}
		out+= "</table>";
		out += "Please refer to sirmoto.net dashboard to add or remove the schedules";
		out += "<button>Update now</button>";
	}
	else
	{
		out += "<h1>No Internet connection</h1>";
	}
	out += Foot();
	request->send(200, "text/html", out);
}


void CredentialSetup(AsyncWebServerRequest *request)
{
	String out = Head();
	if(InternetConnectionTest())
	{
		out += "<form method='post' action='saveCredentials'>";
			out += "Email:<input type='text' name='email' />";
			out += "Password:<input type='text' name='password' />";
			out += "<button type='submit'>Login</button>";
		out += "</form>";
	}
	else
	{
		out += "<h1>No Internet connection</h1>";
	}
	out += Foot();
	request->send(200, "text/html", out);
}



void loop()
{
	timer.run();
}
