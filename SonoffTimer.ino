/**
    Timer configurado via browser para o Sonoff Basic
    (C) 2020, Daniel Quadros
    https://dqsoft.blogspot.com
**/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <time.h>
#include "SNTP.h"

// Conexões do Sonoff Basic
const int sonoff_LED = 13;
const int sonoff_RELE = 12;

// Configurações do WiFi
// Preencha com os dados da sua rede!
// Escolha um IP fora da região de DHCP
char *ssid = "DQuadros";
char *passwd = "*****";
IPAddress ipserver = IPAddress(192,168,15,201);
IPAddress netmask = IPAddress(255,255,255,0);
IPAddress gateway = IPAddress(192,168,15,1);
IPAddress dns = IPAddress(192,168,15,1);

WiFiUDP wifiudp;
SNTP sntp;
ESP8266WebServer webserver(80);

time_t horaLiga = 0;
time_t horaDesliga = 0;
boolean ligado = false;

void setup() {
  // Inicia I/Os do Sonoff
  pinMode (sonoff_LED, OUTPUT);
  pinMode (sonoff_RELE, OUTPUT);
  digitalWrite (sonoff_LED, HIGH);  // LED acende com LOW
  digitalWrite (sonoff_RELE, LOW);  // Rele fecha com HIGH

  // Inicia serial para debug
  Serial.begin (115200);
  Serial.println();

  // Conecta a rede WiFi
  Serial.print ("Conectando... ");
  WiFi.mode (WIFI_STA);
  WiFi.begin (ssid, passwd);
  WiFi.config(ipserver, dns, gateway, netmask);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  Serial.println ("Conectado");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Inicia SNTP
  sntp.init (&wifiudp, -10800);

  // Inicia webserver
  webserver.on("/", trataCliente);
  webserver.onNotFound(trataNotFound);
  webserver.begin();
}

// Loop Principal
void loop() {
  // Trata o WiFi
  if (WiFi.status() == WL_CONNECTED) {
    sntp.update();
    webserver.handleClient();
    digitalWrite (sonoff_LED, LOW);
  } else {
    digitalWrite (sonoff_LED, HIGH);
  }

  // Trata o timer
  if (sntp.valid() && (horaLiga != horaDesliga)) {
    boolean dentro;
    time_t horaAtual = sntp.time() % (24L*60L*60L);
    if (horaDesliga > horaLiga) {
      dentro = (horaAtual > horaLiga) &&
               (horaAtual < horaDesliga);
    } else {
      dentro = (horaAtual > horaLiga) ||
               (horaAtual < horaDesliga);
    }
    if (ligado && !dentro) {
      Serial.println ("Desligando");
      digitalWrite (sonoff_RELE, LOW);
      ligado = false;
    } else if (!ligado && dentro) {
      Serial.println ("Ligando");
      digitalWrite (sonoff_RELE, HIGH);
      ligado = true;
    }
  }
}

// Trata uma conexão ao servidor web
void trataCliente() {
  Serial.println ("Conectou cliente");
  // Trata parametros
  for (int i = 0; i < webserver.args(); i++) {
    if (strcmp(webserver.argName(i).c_str(), "liga") == 0) {
      horaLiga = parseHora(webserver.arg(i).c_str());
    } else if (strcmp(webserver.argName(i).c_str(), "desliga") == 0) {
      horaDesliga = parseHora(webserver.arg(i).c_str());
    }
  }
  
  // Mostra situação atual
  char sHoraLiga[6], sHoraDesliga[6];
  fmtHora(sHoraLiga, horaLiga);
  fmtHora(sHoraDesliga, horaDesliga);
  String pagina = "<!DOCTYPE html> <html>\n";
  pagina += "<head>";
  pagina += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  pagina += "<title>Sonoff Timer</title>\n";
  pagina += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  pagina += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  pagina += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  pagina += "</style>\n";
  pagina += "</head>\n";
  pagina += "<body>\n";
  pagina += "<h1>Sonoff Timer </h1>\n";
  pagina += "<h3>DQSoft</h3>\n";
  pagina += "<p>Liga: ";
  pagina += sHoraLiga;
  pagina += "</p>\n";
  pagina += "<p>Desliga: ";
  pagina += sHoraDesliga;
  pagina += "</p>\n";
  if (ligado) {
    pagina += "<p>Rele LIGADO</p>\n";
  } else {
    pagina += "<p>Rele desligado</p>\n";
  }
  pagina += "</body>\n";
  pagina += "</html>\n";
  webserver.send(200, "text/html", pagina);
}

// Converte string com hora no formato [h]h[:m[m]] em
// valor em segundos desde a meia noite
time_t parseHora (const char *sHora) {
  unsigned int hora = 0;
  unsigned int minuto = 0;

  if (!isdigit(*sHora)) {
    return 0L;
  }
  hora = *sHora++ - '0';
  if (isdigit(*sHora)) {
    hora = 10*hora + *sHora++ - '0';
  }
  if (hora > 23) {
    return 0L;
  }
  if (*sHora == ':') {
    sHora++;
    if (!isdigit(*sHora)) {
      return 0L;
    }
    minuto = *sHora++ - '0';
    if (isdigit(*sHora)) {
      minuto = 10*minuto + *sHora++ - '0';
    }
    if ((minuto < 60) && (*sHora == '\0')) {
      return ((hora*60L) + minuto) * 60L;
    }
  }
  return 0L;
}

// Formata hora para apresentação
void fmtHora (char *sHora, timer_t hora) {
  hora = hora / 60L;
  sHora[0] = '0' + (hora / (10L*60L));
  sHora[1] = '0' + ((hora / 60L) % 10L);
  sHora[2] = ':';
  hora = hora % 60L;
  sHora[3] = '0' + ((hora / 10L) % 10L);
  sHora[4] = '0' + (hora % 10L);
  sHora[5] = '\0';
}

// Tentando acessar caminho desconhecido
void trataNotFound(){
  webserver.send(404, "text/plain", "Ops!");
}
