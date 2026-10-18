#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// 🌐 Configurações de Rede Wi-Fi
const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

// 📡 Configurações do Broker MQTT (Ex: Broker público HiveMQ)
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// 📌 Definição de Pinos e Sensores
#define DHTPIN 4       // Pino digital do DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int ldrPin = 34; // Pino analógico do LDR (ADC1)

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;
const long interval = 5000; // Envia a cada 5 segundos

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Ação ao receber comando do MyMQTT
  if (String(topic) == "vinheria/agnello/comando") {
    if (message == "LED_ON") {
      Serial.println("Comando executado: Atuação acionada via MyMQTT!");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    String clientId = "ESP32Client-VinheriaAgnello";
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
      // Inscreve-se no tópico para receber comandos do MyMQTT
      client.subscribe("vinheria/agnello/comando");
    } else {
      Serial.print("falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(ldrPin, INPUT);

  setup_wifi();
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
      Serial.println("Erro ao ler dados do sensor DHT11!");
      return;
    }

    // Conversão e publicação MQTT
    char tempStr[8];
    dtostrf(temperature, 1, 2, tempStr);
    client.publish("vinheria/agnello/temperatura", tempStr);

    char humStr[8];
    dtostrf(humidity, 1, 2, humStr);
    client.publish("vinheria/agnello/umidade", humStr);

    char ldrStr[8];
    sprintf(ldrStr, "%d", ldrValue);
    client.publish("vinheria/agnello/luminosidade", ldrStr);

    Serial.println("--- Dados Publicados ---");
    Serial.printf("Temperatura: %s °C\n", tempStr);
    Serial.printf("Umidade: %s %%\n", humStr);
    Serial.printf("Luminosidade: %s\n", ldrStr);
  }
}