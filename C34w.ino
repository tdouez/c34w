//--------------------------------------------------------------------
//                 +/
//                 `hh-
//        ::        /mm:
//         hy`      -mmd
//         omo      +mmm.  -+`
//         hmy     .dmmm`   od-
//        smmo    .hmmmy    /mh
//      `smmd`   .dmmmd.    ymm
//     `ymmd-   -dmmmm/    omms
//     ymmd.   :mmmmm/    ommd.
//    +mmd.   -mmmmm/    ymmd-
//    hmm:   `dmmmm/    smmd-
//    dmh    +mmmm+    :mmd-
//    omh    hmmms     smm+
//     sm.   dmmm.     smm`
//      /+   ymmd      :mm
//           -mmm       +m:
//            +mm:       -o
//             :dy
//              `+:
//--------------------------------------------------------------------
//   __|              _/           _ )  |
//   _| |  |   ` \    -_)   -_)    _ \  |   -_)  |  |   -_)
//  _| \_,_| _|_|_| \___| \___|   ___/ _| \___| \_,_| \___|
//--------------------------------------------------------------------
// Code mise à disposition selon les termes de la Licence Creative Commons Attribution
// Pas d’Utilisation Commerciale.
// Partage dans les Mêmes Conditions 4.0 International.
//--------------------------------------------------------------------
// 2023/11/10 - FB V1.0.0
// 2024/02/11 - FB V1.1.0 - Mise à jour toutes les 15 minutes
// 2024/02/26 - FB V1.2.0 - Correction sur les heures HP et HC inversées
//                          Passage mise à jour toules les 5 minutes.
//                          Ajout clignotement jour en HP  
// 2024/03/07 - FB V1.2.1 - Ajout test leds au démarrage              
//--------------------------------------------------------------------
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <WiFiManager.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebSrv.h>
#include <NTPClient.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "FS.h"
#include "SPIFFS.h"
#include "mdns.h"
#include <Ticker.h>
 
#define VERSION   "v1.2.1"
 
#define LED_DEMAIN  0
#define LED_JOUR    1
#define LED_WIFI    2
#define LED_RELAIS  3
 
#define PIN_LED     D1
#define PIN_BP      D3
#define PIN_RELAIS  D10
#define NUMPIXELS   4
 
#define DEF_URL_EDF_PART "https://particulier.edf.fr/services/rest/referentiel/searchTempoStore?dateRelevant="

#define MAX_BUFFER      32
#define MAX_BUFFER_URL  200

#define HC          0
#define HP          1
#define HORAIRE_MATIN 6
#define HORAIRE_SOIR  22
 
char module_name[MAX_BUFFER];
char url_edf_part[MAX_BUFFER_URL];
char couleur_jour[MAX_BUFFER];
char couleur_demain[MAX_BUFFER];
bool flag_call = true;
bool flag_first = true;
bool etat_relais = false;
bool maj_prog = false;
bool state_led = false;
String Reponse_tempo;
String Startup_date;
uint b_hc_name;
uint b_hp_name;
uint w_hc_name;
uint w_hp_name;
uint r_hc_name;
uint r_hp_name;
uint horaire = HC;
uint memo_minute = 0;
uint minute_courante = 0;
uint32_t val_couleur_jour;
uint32_t val_couleur_demain;

WiFiManager wm;
AsyncWebServer server(80);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
Ticker blinker;

const uint32_t black = pixels.Color(0, 0, 0);
const uint32_t red = pixels.Color(255, 0, 0);
const uint32_t green = pixels.Color(0, 255, 0);
const uint32_t blue = pixels.Color(0, 0, 255);
const uint32_t white = pixels.Color(255, 255, 255);
const uint32_t purple = pixels.Color(255, 0, 255);
const uint32_t yellow = pixels.Color(255, 255, 0);
const uint32_t orange = pixels.Color(255, 165, 0);


//-----------------------------------------------------------------------
void test_led() {
  pixels.setPixelColor(LED_JOUR, red);
  pixels.setPixelColor(LED_DEMAIN, red);
  pixels.setPixelColor(LED_WIFI, red);
  pixels.setPixelColor(LED_RELAIS, red);
  pixels.show();

  delay(400);
  pixels.setPixelColor(LED_JOUR, green);
  pixels.setPixelColor(LED_DEMAIN, green);
  pixels.setPixelColor(LED_WIFI, green);
  pixels.setPixelColor(LED_RELAIS, green);
  pixels.show();

  delay(400);
  pixels.setPixelColor(LED_JOUR, blue);
  pixels.setPixelColor(LED_DEMAIN, blue);
  pixels.setPixelColor(LED_WIFI, blue);
  pixels.setPixelColor(LED_RELAIS, blue);
  pixels.show();

  delay(400);
  pixels.setPixelColor(LED_JOUR, black);
  pixels.setPixelColor(LED_DEMAIN, black);
  pixels.setPixelColor(LED_WIFI, black);
  pixels.setPixelColor(LED_RELAIS, black);
  pixels.show();
  
}

//-----------------------------------------------------------------------
void blink_led() {
  state_led = !state_led;
}

//-----------------------------------------------------------------------
void start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        printf("MDNS Init failed: %d\n", err);
        return;
    }

    //set hostname
    mdns_hostname_set("c34w");
    //set default instance
    mdns_instance_name_set("c34w TEMPO");
}

//-----------------------------------------------------------------------
bool checkRelais()
{
  bool r = false;

  Serial.print("Horaire:");
  Serial.println(horaire);
  Serial.print("Couleur:");
  Serial.println(couleur_jour);

  if (strstr(couleur_jour, "BLEU")) {
    switch (horaire) {
      case HC:
        if (b_hc_name == 1) r = true;
        break;

      case HP:
        if (b_hp_name == 1) r = true;
        break;
    }
  }
  else {
    if (strstr(couleur_jour, "BLANC")) {
      switch (horaire) {
        case HC:
          if (w_hc_name == 1) r = true;
          break;

        case HP:
          if (w_hp_name == 1) r = true;
          break;
      }
    }
    else {
      if (strstr(couleur_jour, "ROUGE")) {
        switch (horaire) {
          case HC:
            if (r_hc_name == 1) r = true;
            break;

          case HP:
            if (r_hp_name == 1) r = true;
            break;
        }
      }
    }
  }

  return r;
}

//-----------------------------------------------------------------------
uint checkHoraire()
{
  uint h = HC;

  uint heure_courante = timeClient.getHours();

  if (heure_courante >= 0 && heure_courante < HORAIRE_MATIN) h = HC; // 0h à 6h -> heures creuses
  if (heure_courante >= HORAIRE_MATIN && heure_courante < HORAIRE_SOIR) h = HP; // 6h à 22h -> heures pleines
  if (heure_courante >= HORAIRE_SOIR) h = HC; // 22h à 0h -> heures creuses

  return h;
}

//-----------------------------------------------------------------------
String return_current_date()
{
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);
  uint16_t year = ti->tm_year + 1900;
  String yearStr = String(year);
  uint8_t month = ti->tm_mon + 1;
  String monthStr = month < 10 ? "0" + String(month) : String(month);

  return yearStr + "-" + monthStr + "-" + String(ti->tm_mday);
}

//-----------------------------------------------------------------------
String return_current_time()
{
  return String(timeClient.getHours()) + String(":") + String(timeClient.getMinutes()) + String(":") + String(timeClient.getSeconds());
}

//-----------------------------------------------------------------------
void page_config_json(AsyncWebServerRequest *request)
{
String strJson = "{\n";

  Serial.println(F("Page config.json"));
  
  // url_edf_part ---------------------
  strJson += F("\"url_edf_part\": \"");
  strJson += url_edf_part;
  strJson += F("\",\n");

  strJson += F("\"b_hc_name\": \"");
  strJson += b_hc_name;
  strJson += F("\",\n");
  strJson += F("\"b_hp_name\": \"");
  strJson += b_hp_name;
  strJson += F("\",\n");

  strJson += F("\"w_hc_name\": \"");
  strJson += w_hc_name;
  strJson += F("\",\n");
  strJson += F("\"w_hp_name\": \"");
  strJson += w_hp_name;
  strJson += F("\",\n");

  strJson += F("\"r_hc_name\": \"");
  strJson += r_hc_name;
  strJson += F("\",\n");
  strJson += F("\"r_hp_name\": \"");
  strJson += r_hp_name;
  strJson += F("\"\n");
  
  strJson += F("}");

  request->send(200, "text/json", strJson);
}

//-----------------------------------------------------------------------
void loadPages()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.on("/w3.css", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/w3.css", "text/css");
  });

  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/script.js", "text/javascript");
  });

  server.on("/jquery.js", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/jquery.js", "text/javascript");
  });

  server.on("/notify.js", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/notify.js", "text/javascript");
  });

  server.on("/fb.svg", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/fb.svg", "image/svg+xml");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(SPIFFS, "/favicon.ico", "image/x-icon");
  });

  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    page_config_json(request);
  });

  server.on("/info.json", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    page_info_json(request);
  });

  server.on("/config.htm", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    page_config_htm(request);
  });
  
  server.on("/maj_tempo.htm", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    page_maj_tempo_htm(request);
    request->send(200, "text/plain", "maj_tempo OK");
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.println("Page not found");
    Serial.println(request->method());
    Serial.println(request->url());
    request->send(404, "text/plain", "The content you are looking for was not found.");
  });
}

//-----------------------------------------------------------------------
void page_info_json(AsyncWebServerRequest *request)
{
String strJson = "{\n";

  //Serial.println(F("Page info.json"));

  // module name---------------------
  strJson += F("\"module_name\": \"");
  strJson += module_name;
  strJson += F("\",\n");

  // version ---------------------
  strJson += F("\"version\": \"");
  strJson += VERSION;
  strJson += F("\",\n");
  
  // couleur_jour ---------------------
  strJson += F("\"couleur_jour\": \"");
  strJson += couleur_jour;
  strJson += F("\",\n");
  
  // couleur_demain ---------------------
  strJson += F("\"couleur_demain\": \"");
  strJson += couleur_demain;
  strJson += F("\",\n");

  // heure ---------------------
  strJson += F("\"heure\": \"");
  if (horaire == HC) strJson += "HC";
    else strJson += "HP";
  strJson += F("\",\n");

  // etat_relais ---------------------
  strJson += F("\"etat_relais\": \"");
  strJson += etat_relais;
  strJson += F("\",\n");

  // current date ---------------------
  strJson += F("\"current_date\": \"");
  strJson += return_current_time() + String(" ") + return_current_date();
  strJson += F("\",\n");

  // startup_date ---------------------
  strJson += F("\"startup_date\": \"");
  strJson += Startup_date;
  strJson += F("\"\n");

  strJson += F("}");

  request->send(200, "text/json", strJson);
}

//-----------------------------------------------------------------------
void page_config_htm(AsyncWebServerRequest *request)
{
boolean flag_restart = false;

  Serial.println(F("Page config htm"));

  b_hc_name=0;
  b_hp_name=0;
  w_hc_name=0;
  w_hp_name=0;
  r_hc_name=0;
  r_hp_name=0;

  int params = request->params();
  Serial.print("Nbr params:");
  Serial.println(params);

  for(int i=0;i<params;i++){
    AsyncWebParameter* p = request->getParam(i);
    Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
    
    if (strstr(p->name().c_str(), "url_edf_part")) strcpy(url_edf_part, p->value().c_str());
    if (strstr(p->name().c_str(), "b_hc_name")) b_hc_name = 1;
    if (strstr(p->name().c_str(), "b_hp_name")) b_hp_name = 1;
    if (strstr(p->name().c_str(), "w_hc_name")) w_hc_name = 1;
    if (strstr(p->name().c_str(), "w_hp_name")) w_hp_name = 1;
    if (strstr(p->name().c_str(), "r_hc_name")) r_hc_name = 1;
    if (strstr(p->name().c_str(), "r_hp_name")) r_hp_name = 1;
     
  }
  saveConfig();
  request->send (200, "text/plain", "OK");
  delay(500);

  if (flag_restart == true) {
    ESP.restart();
  }
}

//-----------------------------------------------------------------------
void page_maj_tempo_htm(AsyncWebServerRequest *request)
{

  Serial.println(F("Page maj_tempo"));

  interrogation_tempo();
}

//-----------------------------------------------------------------------
void printConfig() {

    Serial.println(F("------------------------------------"));
    Serial.println(F("Configuration:"));
    Serial.print(F("Module: "));
    Serial.println(module_name);
    Serial.print(F("Url_edf_part: "));
    Serial.println(url_edf_part);
    Serial.print(F("b_hc_name: "));
    Serial.println(b_hc_name);
    Serial.print(F("b_hp_name: "));
    Serial.println(b_hp_name);
    Serial.print(F("w_hc_name: "));
    Serial.println(w_hc_name);
    Serial.print(F("w_hp_name: "));
    Serial.println(w_hp_name);
    Serial.print(F("r_hc_name: "));
    Serial.println(r_hc_name);
    Serial.print(F("r_hp_name: "));
    Serial.println(r_hp_name);
    Serial.println(F("------------------------------------"));
}

//-----------------------------------------------------------------------
void loadConfig() {

  sprintf(url_edf_part, "%s", DEF_URL_EDF_PART);

  if (SPIFFS.exists("/config.json")) {
    Serial.println(F("Lecture config.json"));
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, configFile);
      if (error) Serial.println(F("Impossible de parser config.cfg"));

      strcpy(url_edf_part, doc["url_edf_part"] | DEF_URL_EDF_PART);
      b_hc_name = doc["b_hc_name"] | 0;
      b_hp_name = doc["b_hp_name"] | 0;
      w_hc_name = doc["w_hc_name"] | 0;
      w_hp_name = doc["w_hp_name"] | 0;
      r_hc_name = doc["r_hc_name"] | 0;
      r_hp_name = doc["r_hp_name"] | 0;
      		
      configFile.close();
    }
    else Serial.println(F("Impossible de lire config.json !!"));
  }
}

//-----------------------------------------------------------------------
void saveConfig() {
  
  pixels.setPixelColor(LED_WIFI, purple);
  pixels.show();
  Serial.println(F("Sauvegarde config.json"));

  File configFile = SPIFFS.open("/config.json", "w");
  StaticJsonDocument<1024> doc;

  doc["url_edf_part"] = url_edf_part;
  doc["b_hc_name"] = b_hc_name;
  doc["b_hp_name"] = b_hp_name;
  doc["w_hc_name"] = w_hc_name;
  doc["w_hp_name"] = w_hp_name;
  doc["r_hc_name"] = r_hc_name;
  doc["r_hp_name"] = r_hp_name;

  if (serializeJson(doc, configFile) == 0) {
    Serial.println("!! Erreur d'ecriture");
  }
  else Serial.println("Fichier sauvegardé");
  configFile.close();
  delay(100);
  pixels.setPixelColor(LED_WIFI, green);
  pixels.show();

  maj_prog = true;
}

//-----------------------------------------------------------------------
void configModeCallback (WiFiManager *myWiFiManager) {
  pixels.setPixelColor(LED_WIFI, blue);
  pixels.show();
}

//-----------------------------------------------------------------------
void checkButton(){
  // check for button press
  if ( digitalRead(PIN_BP) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if( digitalRead(PIN_BP) == LOW ){
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(PIN_BP) == LOW ){
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        for(int i=0; i<10; i++) {
          if (i%2) pixels.setPixelColor(LED_WIFI, yellow);
            else pixels.setPixelColor(LED_WIFI, black);
          pixels.show();
          delay(120);
        }
        wm.resetSettings();
        ESP.restart();
      }
     
      // start portal w delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);
      pixels.setPixelColor(LED_JOUR, black);
      pixels.setPixelColor(LED_DEMAIN, black);
      pixels.setPixelColor(LED_WIFI, purple);
      pixels.show();
     
      if (!wm.startConfigPortal(module_name,"password")) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        pixels.setPixelColor(LED_WIFI, red);
        pixels.show();
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...:)");
        pixels.setPixelColor(LED_WIFI, green);
        pixels.show();
        flag_first = true;
      }
    }
  }
}

//-----------------------------------------------------------------------
void interrogation_tempo()
{
  String Url;
  HTTPClient http;

  Url = String(url_edf_part) + return_current_date();

  pixels.setPixelColor(LED_WIFI, yellow);
  pixels.show();

  Serial.println(Url);
  http.begin(Url);

  int httpCode = http.GET();
  if(httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
        Reponse_tempo = http.getString();
        Serial.println(Reponse_tempo);

        DynamicJsonDocument jsonDoc(512);
        DeserializationError error = deserializeJson(jsonDoc, Reponse_tempo);
        if (error) Serial.println(F("Impossible de parser Reponse_tempo"));
        JsonObject json = jsonDoc.as<JsonObject>();
        //Serial.println(F("Contenu:"));
        //serializeJson(json, Serial);

        val_couleur_jour = black;
        strcpy(couleur_jour, json["couleurJourJ"]);
        if (strstr(couleur_jour, "BLEU")) val_couleur_jour = blue;
        if (strstr(couleur_jour, "BLANC")) val_couleur_jour = white;
        if (strstr(couleur_jour, "ROUGE")) val_couleur_jour = red;
        
        val_couleur_demain = black;
        strcpy(couleur_demain, json["couleurJourJ1"]);
        if (strstr(couleur_demain, "BLEU")) val_couleur_demain = blue;
        if (strstr(couleur_demain, "BLANC")) val_couleur_demain = white;
        if (strstr(couleur_demain, "ROUGE")) val_couleur_demain = red;

        pixels.setPixelColor(LED_WIFI, green);      
    }
    else pixels.setPixelColor(LED_WIFI, orange);  
  }
  else pixels.setPixelColor(LED_WIFI, orange);

  http.end();

  pixels.show();
}

//-----------------------------------------------------------------------
void setup() {
 
  pinMode(PIN_BP, INPUT_PULLUP);
  pinMode(PIN_RELAIS, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
 
  Serial.begin(115200);
 
  Serial.println(F("   __|              _/           _ )  |"));
  Serial.println(F("   _| |  |   ` \\    -_)   -_)    _ \\  |   -_)  |  |   -_)"));
  Serial.println(F("  _| \\_,_| _|_|_| \\___| \\___|   ___/ _| \\___| \\_,_| \\___|"));
  Serial.print(F("   C34w                                        "));
  Serial.println(VERSION);
 
  pixels.begin(); // INITIALIZE NeoPixel
  pixels.clear(); // Set all pixel colors to 'off'
  pixels.setBrightness(100);

  test_led();
  
  pixels.setPixelColor(LED_WIFI, purple);
  pixels.show();
  
  sprintf(module_name, "C34w_%06X", ESP.getEfuseMac());
  
  //----------------------------------------------------SPIFFS
  if(!SPIFFS.begin()) {
    Serial.println("Erreur montage SPIFFS !");
    pixels.setPixelColor(LED_WIFI, red);
	  pixels.show();
	  while (1) {};
  }
  else {
    loadConfig();
    printConfig();
    delay(1000);
  }

  //----------------------------------------------------WIFI
  wm.setDebugOutput(true);
  wm.debugPlatformInfo();
  wm.setConfigPortalTimeout(120);
  wm.setAPCallback(configModeCallback);

  std::vector<const char *> menu = {"wifi","sep","update","restart","exit"};
  wm.setMenu(menu);
 
  bool res = wm.autoConnect(module_name,"fumeebleue");
   
  if(!res) {
      Serial.println("Failed to connect");
      // ESP.restart();
      pixels.setPixelColor(LED_WIFI, red);
      pixels.show();
      delay(5000);
      ESP.restart();
  }
  else {
      //if you get here you have connected to the WiFi   
      Serial.println("connected... :)");
      pixels.setPixelColor(LED_WIFI, green);
      timeClient.begin();
      flag_first = true;
      flag_call = true;
      pixels.show();
  }
 
  //----------------------------------------------------SERVER
  loadPages();
  server.begin();
  Serial.println("Serveur actif!");

  //----------------------------------------------------MDNS
  start_mdns_service();
  
  timeClient.update();
  Startup_date = return_current_date() + String (" ") + return_current_time();

  blinker.attach(1, blink_led);
}
 
//-----------------------------------------------------------------------
void loop() {

  timeClient.update();

  if (flag_first) {
    flag_first = false;
    interrogation_tempo();
  }

  minute_courante = timeClient.getMinutes();
  if ((minute_courante % 5) == 0) {  // maj toutes les 5 minutes
    if (flag_call == true) interrogation_tempo();
    flag_call = false;
  }
  else flag_call = true;

  horaire = checkHoraire();

  if (minute_courante != memo_minute || maj_prog == true) {
    maj_prog = false;

    etat_relais = checkRelais();
    if (etat_relais) {
      pixels.setPixelColor(LED_RELAIS, yellow);
      pixels.show();
      digitalWrite(PIN_RELAIS, HIGH);
    }
    else {
      pixels.setPixelColor(LED_RELAIS, black);
      pixels.show();
      digitalWrite(PIN_RELAIS, LOW);
    }
    memo_minute = minute_courante;
  }

  checkButton();

  if (horaire == HC) pixels.setPixelColor(LED_JOUR, val_couleur_jour);
    else {
      if (state_led == true) pixels.setPixelColor(LED_JOUR, val_couleur_jour);
        else pixels.setPixelColor(LED_JOUR, black);
    }
  pixels.setPixelColor(LED_DEMAIN, val_couleur_demain);
  pixels.show();
  
 
}
