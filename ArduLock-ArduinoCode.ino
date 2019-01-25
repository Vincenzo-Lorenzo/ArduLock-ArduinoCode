#include <ArduinoJson.h>
#include "WiFiEsp.h"
#include <EEPROM.h>      //Per Gestione EEPROM

//#include "SoftwareSerial.h"
//SoftwareSerial Serial2(6, 7); // RX, TX

#define SERIALE "01240009480124000966"
#define BUTTONAP    8
#define LEDAP       9
#define LEDALLARME  4
#define LEDALLARME1 10
#define LEDALLARME2 11
#define PINALLARME1 5
#define PINALLARME2 12
#define BUZZER      3

char *ssid;
char *pass;
int status = WL_IDLE_STATUS;// Wifi radio's status
char MYssid[] = "ArduLock";
//char MYpass[] = "0000";

char *serverWIFI;
int porta = 9100;
boolean serialeInviatoAlServer=false;
boolean allarmeAttivoZona1=false;
boolean allarmeAttivoZona2=false;
boolean allarmeInCorso=false;

WiFiEspClient client;
WiFiEspServer server(80);

RingBuffer buf(8);

String jsonServerFirstConfiguration = "";

void(* Riavvia)(void) = 0;

void setup()
{
  /*
  Serial.begin(57600);
  Serial2.begin(57600);
  delay(500);
  Serial2.println("AT+GMR");  
  while(1==1) loopProva();
  */
  
  //https://www.maffucci.it/2010/12/06/arduino-lezione-03-controlliamo-un-led-con-un-pulsante/
  pinMode(BUTTONAP, INPUT);
  pinMode(LEDAP, OUTPUT);  digitalWrite(LEDAP, LOW); 
  pinMode(PINALLARME1, INPUT);
  pinMode(PINALLARME2, INPUT);
  pinMode(LEDALLARME, OUTPUT); digitalWrite(LEDALLARME, LOW);
  pinMode(LEDALLARME1, OUTPUT); digitalWrite(LEDALLARME1, LOW);
  pinMode(LEDALLARME2, OUTPUT); digitalWrite(LEDALLARME2, LOW);
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);
  
  //Inizializzo La Seriale per il DEBUG
  Serial.begin(57600); 
  //Inizializzo La Seriale per il modulo ESP
  //Serial2.begin(115200);
  Serial2.begin(57600);
  Serial2.setTimeout(10000),
  WiFi.init(&Serial2);
  
  client.setTimeout(10000);
  
  //Attendo Fin Quando non leggo il modulo ESP
  while (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    delay(2000);
    WiFi.init(&Serial2);
  }

  //----------------------------------- Controllo che l'input sia HIGH (pulsante premuto)
  if (digitalRead(BUTTONAP) == HIGH) {
    digitalWrite(LEDAP, HIGH);
    InizializzaESPComeAP();

    //Forzo il LoopAP
    while(true) loopAP();
  }
  //------------------------------ FINE Controllo che l'input sia HIGH (pulsante premuto)

  LeggiMemoria();
  //Serial2.println("AT+UART_DEF=57600,8,1,0,0");
  //Tento la connessione alla rete WiFi
  while (status != WL_CONNECTED) {
    Serial.print("Tento la connessione alla SSID: ");
    Serial.println((String)ssid);
    status = WiFi.begin(ssid, pass);
    delay(1000);
  }

  Serial.println("Ora sei in rete !");
  
  printWifiStatus();
  Serial.println();
  
  tentaConnessione();

  Serial.print("JSON Prima Config: ");
  Serial.println(jsonServerFirstConfiguration);

  if (jsonServerFirstConfiguration != "") {
    inviaPrimaConfigigurazioneAlServer();
    jsonServerFirstConfiguration = "";
  }
  
  AttivaDisattivaAllarme(false, 1);
  AttivaDisattivaAllarme(false, 2);

  
  /*if (client.connect(server, porta)) {
    Serial.println("Connected to server");
    // Make a HTTP request
    //client.println("GET /asciilogo.txt HTTP/1.1");
    //client.println("Host: arduino.cc");
    //client.println("Connection: close");
    //client.println();
  }*/

}

/*
void loopProva() {
   // Se Arriva Qualcosa Dal Server
  while (Serial2.available()) {
    char c = Serial2.read();
    Serial.write(c);
  }
}*/

void loop()
{
  if (allarmeInCorso) {
    if (digitalRead(BUTTONAP) == HIGH) {
      AttivaDisattivaAllarme(false, 1);
      AttivaDisattivaAllarme(false, 2);
    }
  } else {
    if(allarmeAttivoZona1) {
      Serial.println((String)"leggo pin " + PINALLARME1);
      
      if (digitalRead(PINALLARME1) == HIGH) {
        allarmeInCorso = true;
        Serial.println("pin 1 alto !");
        digitalWrite(LEDALLARME, HIGH);
        digitalWrite(BUZZER, HIGH);
        inviaLog("P1ALTO");
      }
    }

    if(allarmeAttivoZona2) {
      Serial.println((String)"leggo pin " + PINALLARME2);
      
      if (digitalRead(PINALLARME2) == HIGH) {
        allarmeInCorso = true;
        Serial.println("pin 2 alto !");
        digitalWrite(LEDALLARME, HIGH);
        digitalWrite(BUZZER, HIGH);
        inviaLog("P2ALTO");
      }
    }
  }
  
  if(!serialeInviatoAlServer) {
    serialeInviatoAlServer = inviaSeriale();
  }
  
  // Se Arriva Qualcosa Dal Server
  if (client.available()) {
    leggiClient();
  }
  
  delay(1000);

  //Se il server si è spento tento la riconnessione
  if (!client.connected()) {
    Serial.println();
    Serial.println("Disconnesso dal server ...");
    client.stop();
    serialeInviatoAlServer=false;
    tentaConnessione();
  }

}

void AttivaDisattivaAllarme(boolean attivare, int zona) {
  switch(zona) {
    case 1: 
       allarmeAttivoZona1 = attivare;
       digitalWrite(LEDALLARME1, attivare ? HIGH: LOW);
    case 2:
      allarmeAttivoZona2 = attivare;
      digitalWrite(LEDALLARME2, attivare ? HIGH: LOW);
  }

  if (!allarmeAttivoZona1 && !allarmeAttivoZona2) {
    digitalWrite(LEDALLARME, LOW);
    digitalWrite(BUZZER, LOW);
    allarmeInCorso = false;
    inviaLog("Disatt");
  }
}

void printWifiStatus()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void printWifiStatusAP()
{
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  Serial.println();
  Serial.print("To see this page in action, connect to ");
  Serial.print(MYssid);
  Serial.print(" and open a browser to http://");
  Serial.println(ip);
  Serial.println();
}

void leggiClient() {
  boolean esito = false;
  boolean risposto = false;
  long timeout = 10000; //Attendo la risposta del server entro massimo 10 secondi
  String str;
  String resp = "";
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  //Leggo La richiesta
  long int time = millis();
  
  while (((time + timeout) > millis()) && (risposto == false)) {
    if (client.available()) {
      char c = client.read();
      resp += c;
      if (c == '}') risposto = true;
    }
  }
  
  Serial.println(resp);
      
  //Vediamo il tipo di Richiesta che ci è stata fatta
  if (resp.indexOf("AbZ") != -1) {
    StaticJsonBuffer<200> jsonBuffer;

    JsonObject& rootR = jsonBuffer.parseObject(resp);
    if (rootR.success()) {
      AttivaDisattivaAllarme(rootR["AbZ1"], 1);
      AttivaDisattivaAllarme(rootR["AbZ2"], 2);
    }

    root["Seriale"] = SERIALE;
    root["Esito"] = "OK";
    root.printTo(str);
  
    Serial.print("Invio "); Serial.println(str);
    client.print(str);
  
  } else if (resp == "STATO_ALLARME}") {
    root["Seriale"] = SERIALE;
    root["Z1"] = allarmeAttivoZona1;
    root["Z2"] = allarmeAttivoZona2;
    root["Allarme"] = allarmeInCorso;
    root.printTo(str);
 
    Serial.print("Invio "); Serial.println(str);
    client.print(str);
  } 
  
  
  
}


boolean inviaSeriale() {
  boolean esito = false;
  boolean risposto = false;
  long timeout = 10000; //Attendo la risposta del server entro massimo 10 secondi
  String str;
  String resp;
  StaticJsonBuffer<200> jsonBuffer;
  
  JsonObject& root = jsonBuffer.createObject();
  root["Seriale"] = SERIALE;
  //root["DispositivoInizializzato"] = dispositivoInizializzato;
  root.printTo(str);

  Serial.print("Invio "); Serial.println(str);
  client.print(str);

  //Attendo Risposta
  delay(500);
  long int time = millis();
  
  while (((time + timeout) > millis()) && (risposto == false)) {
    if (client.available()) {
      resp = client.readStringUntil('}');
      resp += "}";
      Serial.println(resp);
      risposto = true;
      
      StaticJsonBuffer<200> jsonBuffer;

      JsonObject& root = jsonBuffer.parseObject(resp);
      if (root.success()) {
        if(root["Esito"] == "OK") {
          esito = true;
          //Leggo Ora I parametri di configurazione
          //zona1Abilitata = root["Zona1Abilitata"];
          //zona2Abilitata = root["Zona2Abilitata"];
        }
      }
    }
  }

  Serial.println((String)"Esito " + esito);
  return esito;
  
}

boolean inviaPrimaConfigigurazioneAlServer() {
  boolean esito = false;
  boolean risposto = false;
  long timeout = 10000; //Attendo la risposta del server entro massimo 10 secondi
  String resp;
  
  Serial.print("Invio "); Serial.println(jsonServerFirstConfiguration);
  client.print(jsonServerFirstConfiguration);

  //Attendo Risposta
  delay(500);
  long int time = millis();
  
  while (((time + timeout) > millis()) && (risposto == false)) {
    if (client.available()) {
      resp = client.readStringUntil('}');
      resp += "}";
      Serial.println(resp);
      risposto = true;
      
      StaticJsonBuffer<200> jsonBuffer;

      JsonObject& root = jsonBuffer.parseObject(resp);
      if (root.success()) {
        if(root["Esito"] == "OK") {
          esito = true;
          //Leggo Ora I parametri di configurazione
          //zona1Abilitata = root["Zona1Abilitata"];
          //zona2Abilitata = root["Zona2Abilitata"];
        }
      }
    }
  }

  Serial.println((String)"Esito " + esito);
  return esito;
  
}

boolean inviaLog(String descLog) {
  boolean esito = false;
  boolean risposto = false;
  long timeout = 10000; //Attendo la risposta del server entro massimo 10 secondi
  String str;
  String resp;
  StaticJsonBuffer<200> jsonBuffer;
  
  JsonObject& root = jsonBuffer.createObject();
  root["Seriale"] = SERIALE;
  root["Log"] = descLog;
  root.printTo(str);

  Serial.print("Invio il seguente Log "); Serial.println(str);
  client.println(str);

  //Attendo Risposta
  delay(500);
  long int time = millis();
  
  while (((time + timeout) > millis()) && (risposto == false)) {
    if (client.available()) {
      resp = client.readStringUntil('}');
      resp += "}";
      if ((resp.indexOf("OK") != -1)) {
        risposto = true;
        esito = true;
      }
    }
  }

  Serial.println((String)"Esito: " + esito);
  return esito;
  
}

void tentaConnessione() {
  Serial.println("Tento la Connessione al Server...");
  while(!client.connect(serverWIFI, porta)) {
    delay(20000);
  }
  Serial.println("Connesso al Server");
}

//----- ROUTNE PER ACCESS POINT

void InizializzaESPComeAP() {
  Serial.print("Attempting to start AP ");

  //Gli Assegno un ip Statico
  IPAddress localIp(192, 168, 0, 1);
  WiFi.configAP(localIp);
  
  //Start access point
  Serial.println(MYssid);
  //status = WiFi.beginAP(MYssid, 10, MYpass, ENC_TYPE_WPA2_PSK);
  status = WiFi.beginAP(MYssid);
  
  Serial.println("Access point started");
  printWifiStatusAP();
  
  //start the web server on port 80
  server.begin();
  Serial.println("Server started");
}

void loopAP() {
  WiFiEspClient client = server.available();  // listen for incoming clients

  if (client) {                               // if you get a client,
    Serial.println("New client");             // print a message out the serial port
    //buf.init();                               // initialize the circular buffer

    boolean isJsonCharacter = false;
    String jBuff = "";
    int cont = 0;
    
    while (client.connected()) {              // loop while the client's connected
      if (client.available()) {               // if there's bytes to read from the client,
        char c = client.read();               // read a byte, then
        //buf.push(c);                          // push it to the ring buffer

        if (c == '{') isJsonCharacter = true;
        if (isJsonCharacter) jBuff += c;
        if (c == '}') isJsonCharacter = false;

        Serial.write(c);

        // if is end of the HTTP request then send a response
        if (jBuff.endsWith("}")) {
          Serial.println(jBuff);
          
          StaticJsonBuffer<1000> jsonBuffer;

          JsonObject& root = jsonBuffer.parseObject(jBuff);
          if (root.success()) {
            if (root["isAvaiable"] == "getStatus") {
              client.flush();
  
              //client.println("HTTP/1.1 200 OK");
              client.println("HTTP/1.1 200 OK");
              client.println("Access-Control-Allow-Origin: *");
              client.println("Content-Type: application/json");
              client.println("");
            } else {
              ScriviInMemoria(root["SSID"], root["pwd"], root["IPserver"]);
              client.flush();
  
              client.println("POST / HTTP/1.1 200 OK");              
              client.println("Access-Control-Allow-Origin: *");
              client.println("Content-Type: application/json");
              client.println("");
              client.println("{\"Seriale\":" SERIALE ", \"Esito\":\"OK\"}");
              client.println("");
              
              jsonServerFirstConfiguration = jBuff;
              delay(1000);
              //setup();
              //Riavvia();
              status = WL_IDLE_STATUS;// Wifi radio's status
              WiFi.reset();
              //server.end();
              setup();
              return;
            }
          }

          break;
          
        }
      }
    }
    
    // give the web browser time to receive the data
    delay(10);

    // close the connection
    client.stop();
    Serial.println("Client disconnected");
  }
}

void LeggiMemoria() {
  int i;
  String appStr;
 
  for (i = 1, appStr = ""; i < 51; appStr += (char)EEPROM.read(i++));
  appStr.trim();
  ssid = new char[appStr.length()+1];
  appStr.toCharArray(ssid, appStr.length()+1);

  for (i = 51, appStr = ""; i < 101; appStr += (char)EEPROM.read(i++));
  appStr.trim();
  pass = new char[appStr.length()+1];
  appStr.toCharArray(pass, appStr.length()+1);

  for (i = 101, appStr = ""; i < 116; appStr += (char)EEPROM.read(i++));
  appStr.trim();
  serverWIFI = new char[appStr.length()+1];
  appStr.toCharArray(serverWIFI, appStr.length()+1);
}

void ScriviInMemoria(String strSSID, String strPass, String strServer) {
  int i, j; 
  String appStr = "";

  Serial.print("Memorizzo i seguenti valori:  ");
  Serial.print(strSSID);    Serial.print(" ");
  Serial.print(strPass);  Serial.print(" ");
  Serial.println(strServer);
  
  appStr += strSSID + Spazi(50 - strSSID.length());
  appStr += strPass + Spazi(50 - strPass.length());
  appStr += strServer + Spazi(15 - strServer.length());  
  
  for (i = 1, j = 0; j < appStr.length(); i++, j++)
    EEPROM.write(i, (int)appStr.charAt(j));
}

String Spazi(int n) {
  String str = "";
  for(int i=0; i<n; i++, str+=" ");
  return str;
}
