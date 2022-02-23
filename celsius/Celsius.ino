#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <max6675.h>
#include <Ticker.h>
#include <SPI.h>
#include <SD.h>

const String STOP = "stopExperiment";
const String SECRET = "esp8266";
const String DNS = "termo";

const char* ssid = "nombre del hotspot a conectar";
const char* password = "contraseña";

Ticker timerCrono;

int pinDO = 10;
int pinCLK = 16;
int thermoICCS = 0;
int thermoOCCS = 9;
int thermoIHCS = 4;
int thermoOHCS = 2;
int SDCS = 5;

MAX6675 thermocoupleIC(pinCLK, thermoICCS, pinDO);
MAX6675 thermocoupleOC(pinCLK, thermoOCCS, pinDO);
MAX6675 thermocoupleIH(pinCLK, thermoIHCS, pinDO);
MAX6675 thermocoupleOH(pinCLK, thermoOHCS, pinDO);

float currentIC = 0;
float currentOC = 0;
float currentIH = 0;
float currentOH = 0;

uint8_t hh, mm, ss;
float samples;
int counterTime = 0;

File dataFile;

bool play;
signed int admin;
String response;

const size_t CAPACITY = JSON_OBJECT_SIZE(5);
StaticJsonDocument<CAPACITY> doc;

ESP8266WebServer server(80);
WiFiClient client;
WebSocketsServer webSocket = WebSocketsServer(81);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch(type)
  {
    
    case WStype_CONNECTED:
    {
      response = getStatus();
      webSocket.sendTXT(num, response);
      if(!play)
      {
        response = getDownload();
        webSocket.sendTXT(num, response);
      }
      //response = isAdmin(num);
      //webSocket.sendTXT(num, response);
      response = getTime();
      webSocket.sendTXT(num, response);
      //response = getTemp();
      //webSocket.sendTXT(num, response);
      response = getSamples();
      webSocket.sendTXT(num, response);
      response = getIp();
      webSocket.sendTXT(num, response);
    }
    break;

    case WStype_DISCONNECTED:
    {
      if(admin == num)
      {
        SD.remove("temp.xls");
        stopExperiment();
        response = "{\"alert\": \"Oh, no! El administrador se ha ido\"}";
        webSocket.broadcastTXT(response.c_str(), response.length());
      }
      Serial.printf("Desconectando a [%u], admin [%u] \n", num, admin);
    }
    break;
    
    case WStype_TEXT:
    {
      Serial.printf("Número de conexión: %u  -  Carácteres recibidos: %s\n  ", num, payload);
      String request = (char*)payload;
      if(!play)
      {
        DeserializationError error = deserializeJson(doc, payload);
          
        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.c_str());
          break;
        }

        const char* pass = doc["pass"];
        Serial.print(pass);
        authenticate(pass, num);
        
      }
      else if(admin == num)
      {
        if(request.equals(STOP))
        {
          stopExperiment();
        }
      }
    }
    break;

    case WStype_ERROR:
      Serial.printf("Se ha recibido un error. \n");
    break;
  }

}

void setup()
{
  Serial.begin(115200);
  pinMode(thermoICCS, OUTPUT);
  pinMode(thermoOCCS, OUTPUT);
  pinMode(thermoIHCS, OUTPUT);
  pinMode(thermoOHCS, OUTPUT);
  delay(500);

  initSD();
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to "); Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.print("Server started in "); Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebServer iniciado...");

  if (!MDNS.begin(DNS))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
    {
      delay(1000);
    }
    ESP.reset();
  }
  Serial.println("mDNS responder started, Go to termo.local/");
  
  routes();

  play = false;
  samples = 1;
  hh, mm, ss = 0;
  admin = -1;
}

void initSD(){
  pinMode(SDCS, OUTPUT);
  if(SD.begin(SDCS))
  {
    Serial.println("SD card start Succesfully.");
  }
  else
  {
    Serial.println("an error has ocurred while starting sd card.");
    ESP.reset();
  }
}

void loop()
{
  MDNS.update();
  webSocket.loop();
  server.handleClient();
}

void routes(){
  server.on("/", HTTP_GET, []() {
    sendFile("index.htm", "text/html");
  });
  server.on("/script/main.js", HTTP_GET, []() {
    String path = server.uri().substring(1);
    Serial.println(path);
    sendFile(path, "application/javascript");
  });
  server.on("/script/Chart.js", HTTP_GET, []() {
    String path = server.uri().substring(1);
    Serial.println(path);
    sendFile(path, "application/javascript");
  });
  server.on("/style/styles.css", HTTP_GET, []() {
    String path = server.uri().substring(1);
    Serial.println(path);
    sendFile(path, "text/css");
  });
  server.on("/temp.xls", HTTP_GET, []() {
    String path = server.uri().substring(1);
    Serial.println(path);
    sendFile(path, "application/vnd.ms-excel");
  });
}

void sendFile(String p, String type)
{
  File file = SD.open(p, FILE_READ);
  if(file)
  {
    server.streamFile(file, type); 
    file.close();
    server.send(200);
  }
  else
  {
    Serial.print("an error has ocurred when attempting read the path "); Serial.println(p);
    server.send(500, "text/plain", "El Archivo no ta...");
  }
}

void printTemp(){
  currentIC = thermocoupleIC.readCelsius();
  currentOC = thermocoupleOC.readCelsius();
  currentIH = thermocoupleIH.readCelsius();
  currentOH = thermocoupleOH.readCelsius();
  response = getTemp();
  webSocket.broadcastTXT(response.c_str(), response.length());
}

void printHeader(){
  dataFile = SD.open("temp.xls", FILE_WRITE);
  if(dataFile){
    dataFile.print("Tiempo");
    dataFile.print(";");
    dataFile.print("E.Fria");
    dataFile.print(";");
    dataFile.print("S.Fria");
    dataFile.print(";");
    dataFile.print("E.Caliente");
    dataFile.print(";");
    dataFile.println("S.Caliente");
    dataFile.close();
  }else{
    Serial.print("an error has ocurred writing the head in the data file.");
  }
}

void saveData(){
  response = String(hh) + ":" + String(mm) + ":" + String(ss);
  dataFile = SD.open("temp.xls", FILE_WRITE);
  if(dataFile){
    dataFile.print(response.c_str());
    dataFile.print(";");
    //Entrada Fria
    dataFile.print(currentIC);
    dataFile.print(";");
    //Salida Fria
    dataFile.print(currentOC);
    dataFile.print(";");
    //Entrada Caliente
    dataFile.print(currentIH);
    dataFile.print(";");
    //Salida Caliente
    dataFile.println(currentOH);
    dataFile.close();
  }else{
    Serial.print("an error has ocurred writing data in the file.");
  }
}

void authenticate(String p, uint8_t user){
  Serial.printf("Usuario [%u] enviando pass: ", user); Serial.println(p);
  Serial.printf("Usuario entrante: [%u] - Usuario admin: [%u] \n", user, admin);
  if (p.equals(SECRET)){
    admin = user;
    //Serial.print("La IP "); Serial.print(ipAdmin); Serial.println(" controla ahora este experimento");
    SD.remove("temp.xls");
    startExperiment();
  }else{
    response = "{\"alert\": \"Contraseña incorrecta\"}";
    webSocket.sendTXT(user, response);
  }
}

void stopExperiment(){
  timerCrono.detach();
  hh = mm = ss = 0; 
  admin = -1;
  play = !play;
  response = getStatus();
  webSocket.broadcastTXT(response.c_str(), response.length());
  response = getDownload();
  webSocket.broadcastTXT(response.c_str(), response.length());
  response = "{\"alert\": \"Experimento Detenido\"}";
  webSocket.broadcastTXT(response.c_str(), response.length());
}

void startExperiment(){
  samples = doc["samples"];
  hh = doc["hh"];
  mm = doc["mm"];
  ss = doc["ss"];
  counterTime = 0;
  Serial.printf("muestras cada %d * segundos \n", samples);
  response = getDownload();
  webSocket.broadcastTXT(response.c_str(), response.length());
  if (samples != 0){
    printHeader();
  }
  if((hh == 0) && (mm == 0) && (ss == 0)){
    timerCrono.attach_ms(500, updateTime, true);
  }else{
    timerCrono.attach_ms(500, updateTime, false);
  }
  play = !play;
  response = getStatus();
  webSocket.broadcastTXT(response.c_str(), response.length());
  response = getSamples();
  webSocket.broadcastTXT(response.c_str(), response.length());
  response = "{\"alert\": \"Experimento Iniciado\"}";
  webSocket.broadcastTXT(response.c_str(), response.length());
}

void updateTime(bool isUp){
  counterTime++;
  printTemp();
  if(counterTime%2 == 0){
    if(isUp){
      ascender();
    }else{
      descender();
    }
    response = getTime();
    webSocket.broadcastTXT(response.c_str(), response.length());
  }
  checkCounter();
}

void ascender(){
  if (ss < 59){
    ss++;
  }else{
    ss = 0;
    if (mm < 59){
      mm++;
    }else{
      mm = 0;
      if (hh < 23){
        hh++;
      }else{
        hh = 0;
      }
    }
  }
}

void descender(){
  if (ss > 0){
    ss--;
  }else{
    ss = 59;
    if (mm > 0){
      mm--;
    }else{
      mm = 59;
      if (hh > 0){
        hh--;
      }
    }
  }
  if((hh == 0) && (mm == 0) && (ss == 0)){
    stopExperiment();
  }
}

void checkCounter()
{
  if((samples != 0) && (counterTime%((int)(samples / .5)) == 0))
  {
    Serial.printf("muestra tomada en el timepo: %u:%u:%u \n", hh, mm, ss);
    saveData();
  }
}

String getStatus()
{
  return "{\"play\": " + String(play) + "}";
}

String getTime()
{
  return "{\"hh\": " + String(hh) + ", \"mm\": " + String(mm) + ", \"ss\": " + String(ss) + "}";
}

String getTemp()
{
  return "{\"tempIC\": " +  String(currentIC) + ", \"tempOC\": " +  String(currentOC) + ", \"tempIH\": " +  String(currentIH) + ", \"tempOH\": " +  String(currentOH) + "}";
}

String getSamples()
{
  return "{\"samples\": " +  String(samples) + "}";
}

String getDownload()
{
  if(SD.exists("temp.xls"))
  {
    return "{\"download\": 1, \"path\": \"temp.xls\"}";
  }
  else
  {
    return "{\"download\": 0}";
  }
}

String getIp()
{
  return "{\"ip\": \"" + parseIP(WiFi.localIP()) + "\" }";
}

String parseIP(IPAddress ip)
{
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}
