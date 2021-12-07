#include <ESP8266WiFi.h>

#include <EEPROM.h>

#include <Wire.h>

byte modo;
char ssid[30], pass[30], ipuser[30]; //Parametros de la red WiFi
IPAddress ip(192, 168, 0, 70); //Direccion IP fija establecida
IPAddress puerta(192, 168, 0, 1); //Puerta de la red
IPAddress mascara(255, 255, 255, 0); //Mascara de la red
IPAddress ipEE; //IP de la EEPROM para utilizarla en ipconfig
WiFiServer servidor(80); //El puerto 80 por defecto es el HTTP
WiFiClient cliente; //Generación de variable cliente con la libreria

void GuardaEE(String texto, int dir) //Funcion para escribir en la EEPROM (sin commit)
{
  for (int i = 0; i < texto.length(); i++) EEPROM.write(dir + i, texto[i]); //desde i=0 hasta la longitud de la ssid
  EEPROM.write(dir + texto.length(), 0); //se añade un 0 al final (En linux sería al principio seguramente)
}

void LeeEE() {
  int i;
  for (i = 0; i < 30; i++) {
    ssid[i] = EEPROM.read(10 + i); //Lee 30 caracteres (longitud establecida para valores guardadoes en la eeprom)
    pass[i] = EEPROM.read(40 + i);
    ipuser[i] = EEPROM.read(70 + i);
  }
  Serial.println(" ");
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ipuser);
}

void enviarpagina() {
  String pagina = String("HTTP/1.1 200 OK\r\n") + //String de codigo de respuesta HTTP (la solicitud ha tenido éxito)
    "Content-Type: text/html\r\n" +
    "Connection: close\r\n" +
    "\r\n" +
    "<!DOCTYPE html>" +
    "<html>" +
    "<body>" +
    "<h1>Board Configuration</h1>" +
    "<form action=\"/\">" +
    "<label for=\"ssid\">Your SSID:</label><br>" +
    "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"ssid\" ><br>" +
    "<label for=\"pass\">Your password:</label><br>" +
    "<input type=\"password\" id=\"pass\" name=\"pass\" value=\"pass\"><br><br>" +
    "<label for=\"ip\">Your IP:</label><br>" +
    "<input type=\"text\" id=\"ip\" name=\"ip\" value=\"ip\"><br><br>" +
    "<input type=\"submit\" value=\"Submit\">" +
    "</form>" +
    "</body>" +
    "</html>";
  cliente.println(pagina); //Print en monitor serie de los valores
  cliente.stop();

}
void setup() { //Loop setup (solo se inicializa una vez)

  Serial.begin(9600); //Setup baudios (para depurar solo)  
  pinMode(14, INPUT); //Puls1 declarado como input
  
  if (digitalRead(14) == 0){ //Puls1 esta conectado a masa
    modo = 0; //Si esta marcado el pin 14 (Puls1)
  }
  else{
    modo = 1; //variable de configuracion si esta presionado 0, si no 1
  }

  EEPROM.begin(512); //Arranca la eeprom
  //Setup conexion I2C
  Wire.setClock(400000); //Frecuencia de SCL (Fast mode)
  Wire.begin(); //Iniciacion con el slave (su direccion como parametro, 7 bits) o master (sin parametro)

  //Setup Wifi
  WiFi.mode(WIFI_OFF); //Para evitar errores se apaga el WiFi para ponerlo en modo inicial
  delay(100); //Espera de 100ms
  if (modo == 0) { //Si el modo es 0 (configuracion)
    WiFi.softAPConfig(ip, puerta, mascara);
    WiFi.softAP("Mario"); //Crea un punto de acceso de read llamada "Mario"
    Serial.println(WiFi.softAPIP()); //Se hace print de la IP
    servidor.begin(); //Servidor de puerto 80 iniciado (HTTP)
  } else { //Modo 1 (funcionamiento normal)
    LeeEE();
    ipEE.fromString(ipuser); //Saca la IP fija
    WiFi.config(ipEE, ipEE, mascara); //Ahora no son constantes, se leen de la EEPROM
    WiFi.begin(ssid, pass); //Inicializa conexion al WiFi con nombre de red y contraseña introducidos como parametros
    while (WiFi.status() != WL_CONNECTED) { //Si no se establece conexion
      delay(500); //Esperar medio segundo
      Serial.print("."); //Imprimir un punto
    }
    Serial.print("Conectado! IP address: "); //
    Serial.println(WiFi.localIP()); //
  }
}

void loop() {
  if (modo == 0) {  //modo 0
    if (!cliente) {
      cliente = servidor.available(); //Mira si se conecto algun cliente al servidor         
    }
    if (cliente) { //Si el servidor esta disponible
      while (cliente.available() > 0) { //Mira si el cliente envio bytes
        String linea = cliente.readStringUntil('\r'); //Hasta que mande un \r (retorno de carro)
        Serial.println(linea); //imprime linea
        Serial.println(linea.length()); //imprime la longitud de linea (para depurar solo)
        if (linea.length() == 1 && linea[0] == '\n') //Si se lee el 1 final que devuelve la pagina
          enviarpagina();
        // buscamos GET en posicion 0 (lineas que empiecen por GET cuando el cliente esta conectado (el primero del monitor serie no)
        int posicion;
        posicion = linea.indexOf("GET"); //Busca GET, si no lo encuentra da -1
        if (posicion == 0) { //es la linea de la info, la busco y la guardo
          posicion = linea.indexOf("ssid="); //para contar caracteres en terminos relativos
          if (posicion != -1) { //hay sssid en la linea
            int fincadena = posicion + 5; //'ssid=' son 5 caracteres
            while (linea[fincadena] != '&') fincadena++; //acaba cuando hay un &
            String aux = linea.substring(posicion + 5, fincadena); //Extrae como string unicamente lo contenido entre los parametros
            GuardaEE(aux, 10); //Funcion hecha porque la libreria de EEPROM falla al leer valores numericos (como la IP)
            Serial.println(aux);
            posicion = linea.indexOf("pass="); //Busca inicio de pass
            fincadena = posicion + 5; //'pass=' son 5 caracteres
            while (linea[fincadena] != '&') fincadena++; //continua iterando el final de la cadena hasta encontrarse un '&'
            aux = linea.substring(posicion + 5, fincadena);
            GuardaEE(aux, 40); //llamada a la funcion para hacer Write en la EEPROM
            Serial.println(aux);
            posicion = linea.indexOf("ip="); //Busca inicio de la ip
            fincadena = posicion + 3;
            while (linea[fincadena] != ' ') fincadena++;
            aux = linea.substring(posicion + 3, fincadena);
            GuardaEE(aux, 70);
            Serial.println(aux);
            EEPROM.commit(); //Para grabar en la EEPROM de verdad, guarda lo que paso por EEPROM.write()
          }
        }
      }
    } 
  }
  else //el modo normal (modo == 1)
  {
    ; 
  }

} //loop