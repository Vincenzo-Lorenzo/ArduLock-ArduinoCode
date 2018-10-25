#include <ArduinoJson.h>
#include "WiFiEsp.h"

// Emulate Serial2 on pins 6/7 if not present
#ifndef HAVE_HWSerial2
#include "SoftwareSerial.h"
//SoftwareSerial Serial2(6, 7); // RX, TX
#endif

#define SERIALE "01240009480124000966"

char ssid[] = "TIM-83406387";            // your network SSID (name)
char pass[] = "vodafonesmartandroid858wifi994!";        // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status

char server[] = "192.168.1.2";
int  porta = 9100;
boolean serialeInviatoAlServer=false;
boolean zona1Abilitata=false;
boolean zona2Abilitata=false;
//boolean dispositivoInizializzato=false;

WiFiEspClient client;

void setup()
{
  //Inizializzo La Seriale per il DEBUG
  Serial.begin(115200);
  //Inizializzo La Seriale per il modulo ESP
  Serial2.begin(115200);
  WiFi.init(&Serial2);

  //Attendo Fin Quando non leggo il modulo ESP
  while (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    delay(20);
    WiFi.init(&Serial2);
  }

  //Tento la connessione alla rete WiFi
  while (status != WL_CONNECTED) {
    Serial.print("Tento la connessione alla SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
  }

  Serial.println("Ora sei in rete !");
  
  printWifiStatus();
  Serial.println();
  
  tentaConnessione();
  
  /*if (client.connect(server, porta)) {
    Serial.println("Connected to server");
    // Make a HTTP request
    //client.println("GET /asciilogo.txt HTTP/1.1");
    //client.println("Host: arduino.cc");
    //client.println("Connection: close");
    //client.println();
  }*/
}

void loop()
{
  if(!serialeInviatoAlServer) {
    serialeInviatoAlServer = inviaSeriale();
  }
  
  // Se Arriva Qualcosa Dal Server
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  
  //String virgolette = (String)((char)34);
  //String str = "{" + virgolette + "Utente" + virgolette + ":" + virgolette + "Pippo" + virgolette +", " + virgolette + "Password" + virgolette + ":" + virgolette + "Baudo" + virgolette + "}";
  
  delay(3000);

  //Se il server si è spento tento la riconnessione
  if (!client.connected()) {
    Serial.println();
    Serial.println("Disconnesso dal server ...");
    client.stop();
    serialeInviatoAlServer=false;
    tentaConnessione();
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
  client.println(str);

  //Attendo Risposta
  delay(500);
  long int time = millis();
  
  while (((time + timeout) > millis()) && (risposto == false)) {
    if (client.available()) {
      resp = client.readStringUntil('\0');
      Serial.println(resp);
      risposto = true;
      
      StaticJsonBuffer<200> jsonBuffer;

      JsonObject& root = jsonBuffer.parseObject(response);
      if (root.success()) {
        if(root["Esito"].equals("OK")) esito = true;
        //Leggo Ora I parametri di configurazione
        zona1Abilitata = root["Zona1Abilitata"];
        zona2Abilitata = root["Zona2Abilitata"];
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
      resp = client.readStringUntil('\0');
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
  while(!client.connect(server, porta)) {
    delay(20000);
  }
  Serial.println("Connesso al Server");
}
