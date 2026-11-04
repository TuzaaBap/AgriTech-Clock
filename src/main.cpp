#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include "build_time.h"
#include "display_config.h"
#include "secrets.h"
#include "weather_ca.h"
#include "india_aqi.h"

namespace {
constexpr bool TEMPERATURE_ONLY=false;
constexpr char FULL_WEATHER_URL[] = "https://api.open-meteo.com/v1/forecast?latitude=18.5177&longitude=73.8150&timezone=Asia%2FKolkata&timeformat=unixtime&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,pressure_msl,wind_speed_10m&hourly=visibility,uv_index&forecast_days=1";
constexpr char TEMP_WEATHER_URL[] = "https://api.open-meteo.com/v1/forecast?latitude=18.5177&longitude=73.8150&timezone=Asia%2FKolkata&timeformat=unixtime&current=temperature_2m&forecast_days=1";
constexpr const char* WEATHER_URL=TEMPERATURE_ONLY?TEMP_WEATHER_URL:FULL_WEATHER_URL;
constexpr char AIR_URL[] = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=18.5177&longitude=73.8150&timezone=Asia%2FKolkata&timeformat=unixtime&current=pm2_5&hourly=pm2_5,pm10,nitrogen_dioxide,sulphur_dioxide,ammonia&past_hours=24&forecast_hours=1";
constexpr uint32_t PAGE_MS = 5000, POLL_MS = TEMPERATURE_ONLY?60000:300000;
constexpr uint8_t PAGE_COUNT = TEMPERATURE_ONLY?3:14;
constexpr uint32_t pageDurationMs(uint8_t index) {
    return !TEMPERATURE_ONLY && index==13?2000:PAGE_MS;
}
struct Readings {
    uint32_t magic = 0x41475233;
    int64_t weatherTime = 0, airTime = 0;
    float temp=NAN, humidity=NAN, feels=NAN, rain=NAN, wind=NAN, pressure=NAN;
    float uv=NAN, visibility=NAN, aqi=NAN, pm25=NAN;
    int code=-1;
    bool aqiSevere=false;
    uint8_t aqiPollutants=0;
};
Readings readings;
SemaphoreHandle_t readingsMutex;
MatrixPanel_I2S_DMA* display = nullptr;
uint8_t page=0;
uint32_t pageStarted=0, lastDraw=0;
bool otaStarted=false, otaBusy=false;
volatile bool clockSynced=false;
char weatherStatus[20]="NOT FETCHED", airStatus[20]="NOT FETCHED";

void status(char* destination,const char* message) {
    xSemaphoreTake(readingsMutex,portMAX_DELAY);
    snprintf(destination,20,"%.19s",message);
    xSemaphoreGive(readingsMutex);
}
void syncFromDate(const String& date) {
    struct tm utc={};
    if (strptime(date.c_str(),"%a, %d %b %Y %H:%M:%S GMT",&utc)) {
        // TZ is fixed IST: mktime interprets these UTC fields as IST.
        const time_t epoch=mktime(&utc)+19800;
        if(epoch>1700000000) {
            struct timeval tv={epoch,0};settimeofday(&tv,nullptr);clockSynced=true;
        }
    }
}

Readings snapshot() {
    xSemaphoreTake(readingsMutex, portMAX_DELAY);
    Readings value = readings;
    xSemaphoreGive(readingsMutex);
    return value;
}
float number(JsonVariantConst v) { return v.isNull() ? NAN : v.as<float>(); }
bool request(const char* url, DynamicJsonDocument& json,char* state) {
    status(state,"FETCHING");
    Serial.printf("Request start: free heap=%u largest block=%u\n",ESP.getFreeHeap(),ESP.getMaxAllocHeap());
    WiFiClientSecure client;
    client.setCACert(WEATHER_CA);
    client.setHandshakeTimeout(12);
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(10000);
    if (!http.begin(client, url)) {status(state,"INIT ERROR");return false;}
    const char* headers[]={"Date"}; http.collectHeaders(headers,1);
    http.addHeader("User-Agent", "AgriTech-WF1/2.0");
    const int status=http.GET();
    bool ok=false;
    if (status==200 && http.getSize()<16000) {
        const String body=http.getString();
        const String responseDate=http.header("Date");
        // Release TLS buffers before parsing and storing the response in RAM.
        http.end();client.stop();
        if(TEMPERATURE_ONLY) Serial.printf("Temperature response: %.1200s\n",body.c_str());
        StaticJsonDocument<256> filter;
        filter["current"]=true;filter["hourly"]=true;
        filter["error"]=true;filter["reason"]=true;
        const DeserializationError error=deserializeJson(json,body,DeserializationOption::Filter(filter));
        ok = body.length()<16000 && !error && !json["error"].as<bool>();
        ::status(state,ok?"OK":error==DeserializationError::NoMemory?"JSON MEMORY":"BAD RESPONSE");
        if(ok) syncFromDate(responseDate);
    } else {
        char reason[20];snprintf(reason,sizeof(reason),"HTTP %d",status);::status(state,reason);
        char detail[128]={};client.lastError(detail,sizeof(detail));
        Serial.printf("HTTP failure %d: %s; TLS: %s; free heap %u\n",status,HTTPClient::errorToString(status).c_str(),detail,ESP.getFreeHeap());
    }
    Serial.printf("Weather request: HTTP %d, parsed=%d\n",status,ok);
    http.end();
    return ok;
}
bool fetchWeather() {
    DynamicJsonDocument json(TEMPERATURE_ONLY?2048:4096);
    if (!request(WEATHER_URL,json,weatherStatus)) return false;
    if(json["current"]["time"].isNull()) {status(weatherStatus,"NO CURRENT");return false;}
    const auto c=json["current"].as<JsonObjectConst>();
    if(!isfinite(number(c["temperature_2m"]))) {status(weatherStatus,"TEMP MISSING");return false;}
    const int64_t observed=c["time"].as<int64_t>();
    float uv=NAN, visibility=NAN;
    const auto hours=json["hourly"]["time"].as<JsonArrayConst>();
    int chosen=-1;
    for (size_t i=0;i<hours.size();++i) if (hours[i].as<int64_t>()<=observed) chosen=i;
    if (chosen>=0 && observed-hours[chosen].as<int64_t>()<3600) {
        uv=number(json["hourly"]["uv_index"][chosen]);
        visibility=number(json["hourly"]["visibility"][chosen])/1000.0f;
    }
    if(!TEMPERATURE_ONLY &&
       (!isfinite(uv) || !isfinite(visibility) ||
        !isfinite(number(c["relative_humidity_2m"])) ||
        !isfinite(number(c["apparent_temperature"])) ||
        !isfinite(number(c["precipitation"])) ||
        !isfinite(number(c["wind_speed_10m"])) ||
        !isfinite(number(c["pressure_msl"])) || c["weather_code"].isNull())) {
        status(weatherStatus,"FIELDS MISSING");return false;
    }
    xSemaphoreTake(readingsMutex,portMAX_DELAY);
    readings.temp=number(c["temperature_2m"]);
    readings.humidity=number(c["relative_humidity_2m"]);
    readings.feels=number(c["apparent_temperature"]);
    readings.rain=number(c["precipitation"]);
    readings.wind=number(c["wind_speed_10m"]);
    readings.pressure=number(c["pressure_msl"]);
    readings.code=c["weather_code"] | -1;
    readings.uv=uv; readings.visibility=visibility; readings.weatherTime=observed;
    xSemaphoreGive(readingsMutex);
    Serial.printf("Parsed temperature: %.1f C, API timestamp=%lld\n",number(c["temperature_2m"]),observed);
    return true;
}
bool fetchAir() {
    DynamicJsonDocument json(4096);
    if (!request(AIR_URL,json,airStatus)) return false;
    if(json["current"]["time"].isNull()) {status(airStatus,"NO CURRENT");return false;}
    const auto c=json["current"].as<JsonObjectConst>();
    const int64_t observed=c["time"].as<int64_t>();
    const char* pollutants[]={"pm2_5","pm10","nitrogen_dioxide","sulphur_dioxide","ammonia"};
    const auto hours=json["hourly"]["time"].as<JsonArrayConst>();
    float means[5]={};uint8_t counts[5]={};int64_t latest=0;
    for(size_t i=0;i<hours.size();++i) {
        const int64_t t=hours[i].as<int64_t>();
        if(t>observed || t<=observed-86400) continue;
        if(t>latest) latest=t;
        for(unsigned p=0;p<5;++p) {
            const float v=number(json["hourly"][pollutants[p]][i]);
            if(isfinite(v) && v>=0) {means[p]+=v;++counts[p];}
        }
    }
    for(unsigned p=0;p<5;++p) means[p]=counts[p]?means[p]/counts[p]:NAN;
    const auto estimate=india_aqi::calculate(means,counts);
    if(!isfinite(estimate.value) || !isfinite(number(c["pm2_5"])) || observed-latest>=3600) {
        status(airStatus,"AQI DATA SHORT");return false;
    }
    Serial.printf("India AQI model: %.0f%s pollutants=%u; 24h PM25=%.2f PM10=%.2f NO2=%.2f SO2=%.2f NH3=%.2f; hours=%u,%u,%u,%u,%u\n",estimate.value,estimate.severe?"+":"",estimate.pollutants,means[0],means[1],means[2],means[3],means[4],counts[0],counts[1],counts[2],counts[3],counts[4]);
    xSemaphoreTake(readingsMutex,portMAX_DELAY);
    readings.aqi=estimate.value;readings.aqiSevere=estimate.severe;
    readings.aqiPollutants=estimate.pollutants;readings.pm25=number(c["pm2_5"]);
    readings.airTime=c["time"].as<int64_t>();
    xSemaphoreGive(readingsMutex);
    return true;
}
void networkTask(void*) {
    Preferences cache;
    cache.begin("agri-weather",false);
    uint32_t attempted=0, saved=0;
    bool first=true;
    for (;;) {
        const uint32_t now=millis();
        if (!otaBusy && WiFi.status()==WL_CONNECTED &&
            (first || uint32_t(now-attempted)>=POLL_MS)) {
            first=false; attempted=now;
            const bool weatherOk=fetchWeather();
            const bool airOk=TEMPERATURE_ONLY?true:fetchAir();
            if (weatherOk && airOk && (saved==0 || uint32_t(now-saved)>=900000)) {
                const Readings value=snapshot();
                const size_t written=cache.putBytes("snapshot",&value,sizeof(value));
                Serial.printf("Flash cache saved: %u/%u bytes\n",unsigned(written),unsigned(sizeof(value)));
                if(written==sizeof(value)) saved=now;
            }
            if (!weatherOk || !airOk) attempted=millis()-POLL_MS+(TEMPERATURE_ONLY?20000:60000);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
void line(const char* text,int y,uint16_t color,uint8_t size=1) {
    display->setTextSize(size); display->setTextColor(color,0);
    int16_t x1,y1; uint16_t w,h;
    display->getTextBounds(text,0,0,&x1,&y1,&w,&h);
    display->setCursor((int(DISPLAY_WIDTH)-int(w))/2-x1,y);
    display->print(text);
}
const char* condition(int c) {
    if(c==0) return "CLEAR";
    if(c==1) return "MAINLY CLEAR";
    if(c==2) return "PARTLY CLOUDY";
    if(c==3) return "OVERCAST";
    if(c==45 || c==48) return "FOG";
    if(c>=51 && c<=57) return "DRIZZLE";
    if(c>=61 && c<=67) return "RAIN";
    if(c>=71 && c<=77) return "SNOW";
    if(c>=80 && c<=82) return "RAIN SHOWERS";
    if(c==85 || c==86) return "SNOW SHOWERS";
    if(c>=95 && c<=99) return "THUNDERSTORM";
    return "--";
}
void metric(const char* title,float value,const char* unit,int decimals,uint16_t color,bool stale) {
    char heading[20],body[24];
    snprintf(heading,sizeof(heading),"%s%s",title,stale?" *":"");
    if(isfinite(value)) snprintf(body,sizeof(body),"%.*f %s",decimals,value,unit);
    else snprintf(body,sizeof(body),"-- %s",unit);
    line(heading,0,color); line(body,8,display->color565(255,255,255));
}
void draw(bool clockOnly=false) {
    const Readings r=snapshot();
    const time_t now=time(nullptr);
    const bool weatherOld = !r.weatherTime || !clockSynced || now-r.weatherTime>1800;
    const bool airOld = !r.airTime || !clockSynced || now-r.airTime>7200;
    const uint16_t green=display->color565(0,255,40), cyan=display->color565(0,190,255);
    const uint16_t amber=display->color565(255,160,0), white=display->color565(255,255,255);
    if(!clockOnly) display->fillScreen(0);
    if (otaBusy) {line("UPDATING",0,amber);line("KEEP POWER ON",8,white);}
    else if (millis()<5000) line("AGRITECH",1,green,2);
    else if(TEMPERATURE_ONLY) {
        if(page==0) metric("MIT-WPU TEMP",r.temp,"C",1,green,weatherOld);
        else if(page==1) {
            char state[20];
            xSemaphoreTake(readingsMutex,portMAX_DELAY);snprintf(state,sizeof(state),"%s",weatherStatus);xSemaphoreGive(readingsMutex);
            line(state,0,cyan);
            line(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString().c_str():"WIFI CONNECTING",8,white);
        } else {
            char clock[12]="--:--:--";
            if(clockSynced) {struct tm local;localtime_r(&now,&local);strftime(clock,sizeof(clock),"%H:%M:%S",&local);}
            line(clock,1,cyan,2);
        }
    }
    else switch(page) {
        case 0: {
            char clock[12]="--:--:--";
            if(clockSynced) {struct tm local;localtime_r(&now,&local);strftime(clock,sizeof(clock),"%H:%M:%S",&local);}
            line(clock,1,cyan,2);break;
        }
        case 1: metric("TEMPERATURE",r.temp,"C",1,green,weatherOld);break;
        case 2: metric("UV INDEX",r.uv,"",1,amber,weatherOld);break;
        case 3: {
            const uint16_t color=!isfinite(r.aqi)?cyan:r.aqi<=50?green:r.aqi<=100?display->color565(180,255,0):r.aqi<=200?amber:display->color565(255,40,40);
            char text[24];
            if(isfinite(r.aqi)) snprintf(text,sizeof(text),"PUNE AQI %.0f%s%s",r.aqi,r.aqiSevere?"+":"",airOld?" *":"");
            else snprintf(text,sizeof(text),"PUNE AQI --");
            line(text,4,color);break;
        }
        case 4: metric("VISIBILITY",r.visibility,"km",1,cyan,weatherOld);break;
        case 5: metric("HUMIDITY",r.humidity,"% RH",0,cyan,weatherOld);break;
        case 6: metric("FEELS LIKE",r.feels,"C",1,green,weatherOld);break;
        case 7: metric("WIND",r.wind,"km/h",1,cyan,weatherOld);break;
        case 8: metric("PRECIPITATION",r.rain,"mm",1,cyan,weatherOld);break;
        case 9: metric("SEA LEVEL PRESS",r.pressure,"hPa",0,cyan,weatherOld);break;
        case 10: metric("PM2.5",r.pm25,"ug/m3",1,amber,airOld);break;
        case 11: line(weatherOld?"WEATHER *":"WEATHER",0,green);line(condition(r.code),8,white);break;
        case 12: line("AgriTech",0,green);line("MITWPU Pune",8,white);break;
        case 13:
            line(WiFi.status()==WL_CONNECTED?"WIFI CONNECTED":"WIFI RETRYING",0,cyan);
            if(WiFi.status()==WL_CONNECTED) line(WiFi.localIP().toString().c_str(),8,white);
            else line("CHECK 2.4G WIFI",8,white);
            break;
    }
    display->flipDMABuffer();
}
}
void setup() {
    Serial.begin(115200);
    pinMode(TEST_BUTTON_PIN,INPUT_PULLUP);
    HUB75_I2S_CFG::i2s_pins pins={R1_PIN,G1_PIN,B1_PIN,R2_PIN,G2_PIN,B2_PIN,A_PIN,B_PIN,C_PIN,-1,-1,LAT_PIN,OE_PIN,CLK_PIN};
    HUB75_I2S_CFG config(PANEL_WIDTH,PANEL_HEIGHT,PANEL_COUNT,pins);
    config.i2sspeed=HUB75_I2S_CFG::HZ_10M;config.clkphase=false;
    config.latch_blanking=2;config.double_buff=false;
    config.setPixelColorDepthBits(4);
    display=new MatrixPanel_I2S_DMA(config);
    if(!display->begin()) {while(true) delay(1000);}
    display->setBrightness8(DISPLAY_BRIGHTNESS); display->setTextWrap(false);
    Serial.printf("Display: brightness=%u/255 pages=%u clock=5000ms wifi=2000ms\n",DISPLAY_BRIGHTNESS,PAGE_COUNT);
    readingsMutex=xSemaphoreCreateMutex();
    Preferences cache;
    if(cache.begin("agri-weather",true)) {
        Readings stored;
        if(cache.getBytesLength("snapshot")==sizeof(stored)) {
            cache.getBytes("snapshot",&stored,sizeof(stored));
            if(stored.magic==readings.magic) {readings=stored;Serial.println("Restored readings from flash cache");}
        }
        cache.end();
    }
    draw();
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setHostname("agritech-wf1");
    WiFi.setAutoReconnect(true);WiFi.setSleep(false);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
    configTzTime("IST-5:30","pool.ntp.org","time.google.com","time.cloudflare.com");
    sntp_set_time_sync_notification_cb([](struct timeval*){clockSynced=true;});
    if(time(nullptr)<BUILD_EPOCH) {struct timeval tv={BUILD_EPOCH,0};settimeofday(&tv,nullptr);}
    ArduinoOTA.setHostname("agritech-wf1");
    ArduinoOTA.setPassword(WIFI_PASSWORD);
    ArduinoOTA.onStart([](){otaBusy=true;});
    ArduinoOTA.onError([](ota_error_t){otaBusy=false;});
    if(xTaskCreate(networkTask,"weather",12288,nullptr,1,nullptr)!=pdPASS) {
        status(weatherStatus,"TASK MEMORY");Serial.println("Weather worker allocation failed");
    }
    pageStarted=5000;
}
void loop() {
    const uint32_t now=millis();
    static uint32_t lastReconnect=0, changedAt=0;
    static uint32_t lastReport=0;
    if(uint32_t(now-lastReport)>=10000) {
        lastReport=now;
        const Readings value=snapshot();
        Serial.printf("Status: WiFi=%d RSSI=%d clock=%d heap=%u largest=%u WX=%s AIR=%s\n",WiFi.status(),WiFi.RSSI(),clockSynced,ESP.getFreeHeap(),ESP.getMaxAllocHeap(),weatherStatus,airStatus);
        Serial.printf("Readings: TEMP=%.1f C UV=%.1f PUNE_AQI_IN_EST=%.0f%s VIS=%.1f km HUM=%.0f%% FEELS=%.1f C WIND=%.1f km/h RAIN=%.1f mm PRESS=%.0f hPa PM25=%.1f ug/m3 CODE=%d weather_time=%lld air_time=%lld\n",value.temp,value.uv,value.aqi,value.aqiSevere?"+":"",value.visibility,value.humidity,value.feels,value.wind,value.rain,value.pressure,value.pm25,value.code,value.weatherTime,value.airTime);
    }
    static bool raw=HIGH,stable=HIGH;
    if(WiFi.status()==WL_CONNECTED) {
        if(!otaStarted) {ArduinoOTA.begin();otaStarted=true;Serial.print("Weather IP: ");Serial.println(WiFi.localIP());}
        ArduinoOTA.handle();
    } else if(uint32_t(now-lastReconnect)>=30000) {lastReconnect=now;WiFi.reconnect();}
    if(now>=5000 && int32_t(now-pageStarted)>=int32_t(pageDurationMs(page))) {
        page=(page+1)%PAGE_COUNT;pageStarted=now;
    }
    const bool input=digitalRead(TEST_BUTTON_PIN);
    if(input!=raw) {raw=input;changedAt=now;}
    if(input!=stable && uint32_t(now-changedAt)>=40) {
        stable=input;
        if(stable==LOW) {page=(page+1)%PAGE_COUNT;pageStarted=now;}
    }
    // Single-buffer DMA keeps TLS memory available. Do not blank and repaint
    // unchanged pages five times a second: it creates visible software flicker.
    static uint8_t renderedPage=255;
    static bool renderedOta=false;
    const bool pageChanged=renderedPage!=page || renderedOta!=otaBusy;
    const bool isClock=TEMPERATURE_ONLY?page==2:page==0;
    if(now>=5000 && (pageChanged || (isClock && now/1000!=lastDraw/1000))) {
        draw(!pageChanged && isClock && !otaBusy);
        renderedPage=page;renderedOta=otaBusy;lastDraw=now;
    }
    delay(5);
}
