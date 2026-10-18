#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <time.h> // Necessário para o Timestamp exigido pelo FIWARE

//  Configurações de Rede Wi-Fi
const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

//  Configurações do Broker MQTT (FIWARE)
const char* mqtt_server = "IP_DO_SEU_BROKER"; // Ex: IP da sua máquina virtual Ubuntu/FIWARE
const int mqtt_port = 1883;

//  Configurações FIWARE (IoT Agent JSON)
// O FIWARE utiliza a estrutura /<API_KEY>/<DEVICE_ID>/attrs para receber dados
const String apiKey = "vinheria_key";  // Substitua pela API Key do seu grupo
const String deviceId = "esp32_adega"; // Substitua pelo ID registado no Orion
String topicAtributos = "/" + apiKey + "/" + deviceId + "/attrs";
String topicComandos = "/" + apiKey + "/" + deviceId + "/cmd";

//  Definição de Pinos da Shield (Baseado no diagrama do slide 10)
#define DHTPIN 4       // Fio Azul no diagrama (DHT11)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int ldrPin = 34; // Fio Roxo no diagrama (Sensor de Luz / LDR)

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
const long interval = 5000; // Envia dados a cada 5 segundos

//  Configuração NTP para obter o Timestamp
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // Utilizar UTC (0) é o padrão exigido no FIWARE/NGSIv2
const int   daylightOffset_sec = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("A ligar a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi ligado!");
}

void setup_time() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("A sincronizar relógio NTP para geração do Timestamp");
  while (time(nullptr) < 100000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nRelógio sincronizado com sucesso!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("Comando recebido do FIWARE [%s]: %s\n", topic, message.c_str());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("A tentar ligação ao Broker MQTT...");
    if (client.connect("ESP32_VinheriaAgnello")) {
      Serial.println("Ligado!");
      client.subscribe(topicComandos.c_str()); // Subscreve o tópico de comandos do FIWARE
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" Nova tentativa em 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(ldrPin, INPUT);

  setup_wifi();
  setup_time(); // Inicia a sincronização de tempo na internet
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;

    // Leitura dos sensores
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    int ldrValue = analogRead(ldrPin);

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Erro na leitura do sensor DHT11!");
      return;
    }

    //  Obter o Timestamp atual e formatar em ISO 8601 (Compatível com FIWARE)
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Falha ao obter o tempo do servidor NTP");
      return;
    }
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);

    //  Construir o Payload JSON (NGSIv2) para o IoT Agent
    String payload = "{";
    payload += "\"temperature\":" + String(temperature) + ",";
    payload += "\"humidity\":" + String(humidity) + ",";
    payload += "\"luminosity\":" + String(ldrValue) + ",";
    payload += "\"timestamp\":\"" + String(timeString) + "\"";
    payload += "}";

    //  Publicar no broker MQTT
    client.publish(topicAtributos.c_str(), payload.c_str());

    Serial.println("--- Dados Publicados no FIWARE ---");
    Serial.println(payload);
  }
}