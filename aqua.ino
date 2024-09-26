#include <CO2Sensor.h>  
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <PubSubClient.h>
#include <MQUnifiedsensor.h>
#include <Buzzer.h>
#include <ArduinoHttpClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SDA_PIN 21
#define SCL_PIN 22 

#define NO2_PIN 34
#define BUZZER_PIN 2

Buzzer buzzer(2);

// Kalibrasi MQ2 & MQ131
#define Board ("ESP-32")
#define mq2 (33)
#define mq131 (35)
#define Type ("MQ-2")
#define Type2 ("MQ-131")
#define Voltage_Resolution (5)
#define ADC_Bit_Resolution (12)
#define RatioMQ2CleanAir (9.83)
#define RatioMQ131CleanAir (9.83)

// Kalibrasi MG811
CO2Sensor co2Sensor(32, 0.99, 100);

MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, mq2, Type);
MQUnifiedsensor MQ131(Board, Voltage_Resolution, ADC_Bit_Resolution, mq131, Type2);

char SSID[] = "oddo";
char PASS[] = "";

char mqttServer[] = "thingsboard.cloud";
const int mqttPort = 1883;

// String phoneNumber = "6282143231811";
// String apiKey = "5285162";

String phoneNumber = "628970907366";
String apiKey = "6456317";

// String phoneNumber = "6282133840889";
// String apiKey = "1383517";


char token[] = "SMKU1234567890";

// MQTT menggunakan WiFiClient biasa
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// HTTPS menggunakan WiFiClientSecure
WiFiClientSecure secureClient;
HttpClient httpClient = HttpClient(secureClient, "api.callmebot.com", 443);

const char*firmwareURL = "https://raw.githubusercontent.com/Tempebenguk/Capstone/main/aqua.bin";


void wifiSetup() {
  delay(10);

  Serial.println();
  Serial.print("Connect to ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32Client", token, NULL)) {
      Serial.println("connected to Thingsboard");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void OTA() {
  HTTPClient http;
  http.begin(firmwareURL);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();
    if (Update.begin(len)) {
      size_t written = Update.writeStream(http.getStream());
      if (written == len) {
        Serial.println("Update Success");
        Update.end(true);
      } else {
        Serial.println("Update Failed");
        Update.end(false);
      }
    } else {
      Serial.println("Not enough space for update");
    }
  } else {
    Serial.println("Failed to download firmware");
  }
  http.end();
}

// Fungsi untuk mengirim pesan WhatsApp
void sendWhatsAppNotification(String message) {
  secureClient.setInsecure();  // Abaikan sertifikat SSL

  String serverPath = "/whatsapp.php?phone=" + phoneNumber + "&text=" + message + "&apikey=" + apiKey;
  
  httpClient.get(serverPath.c_str());
  int httpResponseCode = httpClient.responseStatusCode();

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = httpClient.responseBody();
    Serial.println("Response: " + response);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(NO2_PIN, INPUT);

  //oled
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(BUZZER_PIN, OUTPUT);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); 
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  analogSetAttenuation(ADC_11db);
  Serial.println("Warming up the sensor");
  display.setCursor(0, 0);
  display.println("Warming up the sensor");
  display.display();
  delay(10000); 
  display.clearDisplay();

   //wifi
  wifiSetup();

  //OTA
  OTA();

  //mqtt
  client.setServer(mqttServer, mqttPort);

  //kalibrasi mq2
  MQ2.setRegressionMethod(1);
  MQ2.setA(36974); MQ2.setB(-3.109);
  MQ2.init();
  Serial.print("Calibrating MQ2 please wait.");
  float calcR0mq2 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ2.update();
    calcR0mq2 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ2.setR0(calcR0mq2/10);
  Serial.println("  done!.");
  if(isinf(calcR0mq2)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0mq2 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  MQ2.serialDebug(true);

  //kalibrasi mq131
  MQ131.setRegressionMethod(1);
  MQ131.setA(23.943); MQ131.setB(-1.11);
  MQ131.init();
  Serial.print("Calibrating MQ131 please wait.");
  float calcR0mq131 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ131.update();
    calcR0mq131 += MQ131.calibrate(RatioMQ131CleanAir);
    Serial.print(".");
  }
  MQ131.setR0(calcR0mq131/10);
  Serial.println("  done!.");
  if(isinf(calcR0mq131)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0mq131 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}
  MQ131.serialDebug(true);
  
}

void loop() {
  //mqtt
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();

  co2Sensor.calibrate();

  MQ2.update();
  MQ131.update(); 

  int gasValue = MQ2.readSensor();
  int ozonValue = MQ131.readSensorR0Rs();
  int co2Value = co2Sensor.read();
  int no2Value = analogRead(NO2_PIN);

  float ozon = ozonValue * 0.000108;

  // Threshold untuk buzzer dan pengiriman notifikasi
  int asapThreshold = 100;
  float ozonThreshold = 0.08;
  int co2Threshold = 1000;
  int no2Threshold = 200;

  String alertMessage = "";

  // Kondisi untuk Asap
  if (gasValue > asapThreshold) {
      alertMessage += "⚠️+*PERINGATAN!*+🚨🚨🚨%0A+";
      alertMessage += "Asap+terdeteksi+di+laboratorium.%0A+";
      alertMessage += "*Kondisi*:+Melebihi+ambang+batas!%0A+";
      alertMessage += "*Tindakan*:+Segera+tinggalkan+laboratorium+sekarang+juga!%0A+";
      for (int i = 0; i < 5; i++) {
          tone(BUZZER_PIN, 2000);  
          delay(500);              
          tone(BUZZER_PIN, 1000);
          delay(500); 
      }
  }

  // Kondisi untuk Ozon
  if (ozon > ozonThreshold) {
      alertMessage += "⚠️+*WASPADA+OZON!*+🚨🚨🚨%0A+";
      alertMessage += "Kadar+Ozon+melebihi+ambang+batas.%0A+";
      alertMessage += "*Kondisi*:+Ozon+terdeteksi+dalam+jumlah+berbahaya!%0A+";
      alertMessage += "*Tindakan*:+Hindari+paparan+dan+segera+berikan+ventilasi.%0A+";
      tone(BUZZER_PIN, 1200);
  }

  // Kondisi untuk CO2
  if (co2Value > co2Threshold) {
      alertMessage += "⚠️+*PERINGATAN+CO2!*+🚨🚨🚨%0A+";
      alertMessage += "Kadar+CO2+di+laboratorium+melebihi+batas+aman.%0A+";
      alertMessage += "*Kondisi*:+Bahaya!+Segera+ambil+tindakan.%0A+";
      alertMessage += "*Tindakan*:+Pastikan+ventilasi+udara+baik!%0A+";
      tone(BUZZER_PIN, 1400);
  }

  // Kondisi untuk NO2
  if (no2Value > no2Threshold) {
      alertMessage += "⚠️+*WASPADA+NO2!*+🚨🚨🚨%0A+";
      alertMessage += "Kadar+NO2+terdeteksi+di+laboratorium.%0A+";
      alertMessage += "*Kondisi*:+Melebihi+ambang+batas!%0A+";
      alertMessage += "*Tindakan*:+Segera+tinggalkan+area+ini+sekarang!%0A+";
      tone(BUZZER_PIN, 2000);
  }



  // Jika ada pesan peringatan, kirim notifikasi WhatsApp
  if (alertMessage != "") {
    sendWhatsAppNotification(alertMessage);
  } else {
    noTone(BUZZER_PIN);
  }

  // Tampilkan di OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Air Quality Monitor");
  
  display.setCursor(0, 20);
  display.print("Asap : ");
  display.print(gasValue);
  display.print(" ppm");

  display.setCursor(0, 30);
  display.print("CO2 : ");
  display.print(co2Value);
  display.print(" ppm");

  display.setCursor(0, 40);
  display.print("Ozon : ");
  display.print(ozon);
  display.print(" ppm");

  display.setCursor(0, 50);
  display.print("Nitrogen : ");
  display.print(no2Value);
  display.print(" µg/m3");

  display.display();

  // Membuat string JSON
  String payload = "{";
  payload += "\"asap\":"; payload += gasValue; payload += ",";
  payload += "\"co2\":"; payload += co2Value; payload += ",";
  payload += "\"no2\":"; payload += no2Value; payload += ",";
  payload += "\"ozon\":"; payload += ozon;
  payload += "}";

  // Tampilkan payload di Serial Monitor
  Serial.print("Data JSON: ");
  Serial.println(payload);

  // Publish data ke Thingsboard
  client.publish("v1/devices/me/telemetry", payload.c_str());
  delay(2500);
}
