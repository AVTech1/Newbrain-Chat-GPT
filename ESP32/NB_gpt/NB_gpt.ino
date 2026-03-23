// ***************************************************************************
// ***
// ***  ChatGPT box om aan een NewBrain te hangen.        B+_Westland@20251201
// ***  met de ESP32-Wroom                                  AV_Retro @ AV_Tech
// ***
// ***  dd 20251201:  communicatie met OpenAI werkt, mijn crediet is te laag :-(
// ***                nog te doen - 2e comm-poort aanmaken en die mbv handshake
// ***                signalen naar de NewBrain brengen op 300 baud. 
// ***  dd 20251211   In principe werkt het goed, met 2e seriele poort, 
// ***                wel handshake nog testen en netjes inbouwen.
// ***                Ook nog 2 ledjes er nog op aan sluiten met status weergeven.
// ***  dd 20260103   Led's aangepast, alles werkt !!! Maar maken (nog) geen gebruik van
// ***                handshake signalen. Dit nog niet getest.
// ***
// ***  Gebaseerd op: https://mciau.com/building-an-interactive-chatbot-with-arduino-esp32-and-chatgpt/
// ***
// ***************************************************************************

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "credentials.h"      // inlog gegevens mbt Wifi en Open-AI

#include <HardwareSerial.h>   // deze libary kan gebruik maken van de handshake signalen

const int MySerialRX = 16;    // RxT
const int MySerialTX = 17;    // TxD
const int MySerialRTS = 5;    // Handshake
const int MySerialCTS = 4;    // Handshake

const int ledPinG = 12;       // Led Groen
const int ledPinR = 13;       // Led Rood

int led = 1;

HardwareSerial Serial_NB(2); // 2e seiele poort 

WiFiClientSecure httpClient;
/* Function to connect to WiFi */
void connectToWiFi();

/* Function to make HTTP request*/
bool sendHTTPRequest(String prompt, String *result);

/* Function to pass user prompt and send it to OpenAI API*/
String getGptResponse(String prompt, bool parseMsg = true);

void setup()
{
  pinMode(ledPinR, OUTPUT);
  pinMode(ledPinG, OUTPUT);

  Serial.begin(115200);                   // "normale" seriele poort
  Serial_NB.begin(300, SERIAL_8N2);       // extra seriele poort naar Newbrain toe.  8N2 maakt niet veel uit
  Serial_NB.setTimeout(15000);            // Set timeout to 15 seconds

  Serial_NB.setPins(MySerialRX, MySerialTX, MySerialCTS, MySerialRTS);
  // Serial_NB.setHwFlowCtrlMode(UART_HW_FLOWCTRL_CTS_RTS);   // Als je deze uitvinkt werkt de handshaking NIET

  // Serial_NB.setRxInvert(true);  // Set the serial port to invert incoming data. Werkt dit ?? NEE dus

  digitalWrite(ledPinR, 1);
  digitalWrite(ledPinG, 0);
  connectToWiFi();
  Serial_NB.print("HCC!Retro >> ");   // Als we opgestart zijn sturen we "de prompt" naar de Newbrain.
}

void loop()
{
  if (Serial_NB.available() > 0)
  {
    String prompt = Serial_NB.readStringUntil('\n');
    prompt.trim();  // haal de /n aan het einde eraf
    // Serial_NB.println(prompt);        // Misschien toch erbij zetten ??????? 
    //   --> Ik denk het niet, gaan we in asm aanpassen in de Newbrain
    String response = getGptResponse(prompt);
    Serial_NB.println();
    Serial_NB.println("Newbr.AI.n >> " + response);
    delay(1000);        // 3 seconde wachten ?? Heb er 1 van gemaakt.
    Serial_NB.print("HCC!Retro >> ");
  }
  delay(10);
}

void connectToWiFi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Verbinding maken met WiFi...");
    if (led == 1) {
      digitalWrite(ledPinR, 0);
      led = 0;                    // Toggle Rode LED
    } else {
      digitalWrite(ledPinR, 1);
      led = 1;                    // zolang er geen wifi verbinding is
    }
    // toggle hier een LED
  }
  Serial.println("Succesvolle WiFi-verbinding");
  digitalWrite(ledPinR, 0);       // Zet de Rode Led UIT
  digitalWrite(ledPinG, 1);       // Zet de Groene LED AAN
  // zet hier de LED AAN
}

bool sendHTTPRequest(String prompt, String *result)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("FOUT: Apparaat niet verbonden met wifi");
    digitalWrite(ledPinG, 0);     // Zet de Groene Led UIT
    digitalWrite(ledPinR, 1);     // Zet de Rode Led AAN
    return false;
  }

  // Connect to OpenAI API URL
  httpClient.setInsecure();
  if (!httpClient.connect(host, httpsPort))
  {
    Serial.println("Fout bij het verbinden met OpenAI API");
    return false;
  }
  // Build Payload
  String payload = "{\"model\": \"gpt-3.5-turbo\",\"messages\": [{\"role\": \"user\", \"content\": \"" + prompt + "\"}]}";
  Serial.println(payload);

  // Build HTTP Request
  String request = "POST /v1/chat/completions HTTP/1.1\r\n";
  request += "Host: " + String(host) + "\r\n";
  request += "Authorization: Bearer " + String(api_key) + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n" + payload + "\r\n";
  // Send HTTP Request
  httpClient.print(request);

  // Get Response
  String response = "";
  while (httpClient.connected())
  {
    if (httpClient.available())
    {
      response += httpClient.readStringUntil('\n');
      response += String("\r\n");
    }
  }
  httpClient.stop();

  // Parse HTTP Response Code
  int responseCode = 0;
  if (response.indexOf(" ") != -1)
  {                                                                                                  // If the first space is found
    responseCode = response.substring(response.indexOf(" ") + 1, response.indexOf(" ") + 4).toInt(); // Get the characters following the first space and convert to integer
  }

  if (responseCode != 200)
  {
    Serial.println("Het verzoek is mislukt. Info:" + String(response));
    return false;
  }

  // Get JSON Body
  int start = response.indexOf("{");
  int end = response.lastIndexOf("}");
  String jsonBody = response.substring(start, end + 1);

  if (jsonBody.length() > 0)
  {
    *result = jsonBody;
    return true;
  }
  Serial.println("Fout: De informatie kon niet worden gelezen.");
  return false;
}

String getGptResponse(String prompt, bool parseMsg)
{
  String resultStr;
  bool result = sendHTTPRequest(prompt, &resultStr);
  if (!result) return "Error : sendHTTPRequest";      // Hier(boven) gaat het niet goed !!!
  if (!parseMsg) return resultStr;
  DynamicJsonDocument doc(resultStr.length() + 200);
  DeserializationError error = deserializeJson(doc, resultStr.c_str());
  if (error)
  {
    return "[ERR] deserializeJson() failed: " + String(error.f_str());
  }
  const char *_content = doc["choices"][0]["message"]["content"];
  return String(_content);
}

