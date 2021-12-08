#include <ESP8266WiFi.h> //Libreria WiFi para el ESP8266

#include <EEPROM.h> //Libreria EEPROM

#include <Wire.h> //Libreria I2C

byte modo; //Variable de control de estado
char ssid[30], pass[30], ipuser[30]; //Parametros de la red WiFi
IPAddress ip(192, 168, 0, 70); //Direccion IP fija establecida
IPAddress puerta(192, 168, 0, 1); //Puerta de la red
IPAddress mascara(255, 255, 255, 0); //Mascara de la red
IPAddress ipEE; //IP variable guardada en la EEPROM para utilizarla en ipconfig
WiFiServer servidor80(80); //El puerto 80 por defecto es el HTTP
WiFiServer servidor23(23); // Puerto 23, para comunicarse a traves del .exe
WiFiClient cliente; //Inicializa la libreria Client

byte Rojo_ON_MSB; //Variable empleada para transmitir la informacion del led rojo alto cuando esta encendido
byte Rojo_ON_LSB; //Variable empleada para transmitir la informacion del led rojo bajo cuando esta encendido
byte Rojo_OFF_MSB; //Variable empleada para transmitir la informacion del led rojo alto cuando esta apagado
byte Rojo_OFF_LSB; //Variable empleada para transmitir la informacion del led rojo bajo cuando esta apagado
char datos[4]; //Realiza la toma de Rojo_ON_MSB; Rojo_ON_LSB; Rojo_OFF_MSB y Rojo_OFF_LSB

void GuardaEE(String texto, int dir) //Funcion para escribir en la EEPROM (sin commit)
{
  for (int i = 0; i < texto.length(); i++) EEPROM.write(dir + i, texto[i]); //desde i=0 hasta la longitud de la ssid
  EEPROM.write(dir + texto.length(), 0); //se añade un 0 al final (En linux sería al principio seguramente)
}

void LeeEE() { //funcion para leer datos desde la EEPROM
  int i; //Iterador, itera 30 caracteres de cada elemento almacenado en la EEPROM
  for (i = 0; i < 30; i++) {
    ssid[i] = EEPROM.read(10 + i); //Nombre de la red
    pass[i] = EEPROM.read(40 + i); //Contraseña
    ipuser[i] = EEPROM.read(70 + i); //IP variable introducida por el usuario
  }
  Serial.println("\n");
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ipuser);
}

void enviarpagina() {
  String pagina = String("HTTP/1.1 200 OK\r\n") + //String de codigo de respuesta HTTP (la solicitud ha tenido éxito)
    "Content-Type: text/html\r\n"+
    "Connection: close\r\n"+
    "\r\n"+
    "<!DOCTYPE html>"+
    "<html>"+
    "<body>"+
    "<h1>Board Configuration</h1>"+
    "<form action=\"/\">"+
    "<label for=\"ssid\">Your SSID:</label><br>"+
    "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"ssid\" ><br>"+
    "<label for=\"pass\">Your password:</label><br>"+
    "<input type=\"password\" id=\"pass\" name=\"pass\" value=\"pass\"><br><br>"+
    "<label for=\"ip\">Your IP:</label><br>"+
    "<input type=\"text\" id=\"ip\" name=\"ip\" value=\"ip\"><br><br>"+
    "<input type=\"submit\" value=\"Submit\">"+
    "</form>"+
    "</body>"+
    "</html>";
  cliente.println(pagina); //Print en monitor serie
  cliente.stop();
}

void configsensorluz() { //Configuracion del sensor de luz mediante I2C
  //Configuracion ADC
  Wire.beginTransmission(B0111001); // Bit start + direccion del sensor I2C (slave) + Bit write
  Wire.write(B10000000); // Registro CONTROL (0x00)
  Wire.write(B00000011); // ADC activado (penultimo bit ADC_EN = 1) y alimentado (ultimo bit POWER = 1)
  Wire.endTransmission(true); // Bit stop

  //Configuracion de integracion
  Wire.beginTransmission(B0111001); // Bit start + direccion del sensor I2C (slave) + Bit write
  Wire.write(B10000001); // Registro TIMING (0x01)
  Wire.write(B00000000); // Integracion libre (tercero y cuarto bit MSB = 00) cada 12 ms (LSB=0000)
  Wire.endTransmission(true); // Bit stop

  //Configuracion de ganancia
  Wire.beginTransmission(B0111001); // Bit start + direccion del sensor I2C (slave) + Bit write
  Wire.write(B10000111); // Registro GAIN (0x07)
  Wire.write(B00110000); // Ganancia x64 (tercer y cuarto bit = 1) sin modo prescaler (tres LSB = 000)
  Wire.endTransmission(true); // Bit stop
}

void lecturasensor() { //Funcion encargada de realizar la medicion del sensor de luz roja
  //Medidas con el LED encendido
  digitalWrite(0, 1); //LED rojo ON
  delay(20); //Tiempo de espera 20ms (el LED se enciende mucho antes de finalizar los 20ms pero por si acaso)

  //Medicion del byte alto del canal de luz roja cuando el LED esta encendido
  Wire.beginTransmission(B0111001); //Bit start + direccion del sensor I2C + Bit write
  Wire.write(B10010011); //Acceso a byte alto del canal de luz roja (0x93)
  Wire.endTransmission(false); //Se inicia de nuevo el protocolo I2C (No se escribe, se lee)
  Wire.requestFrom(B0111001, 1); //Lectura de 1 byte (ultimo parametro) en este registro
  Rojo_ON_MSB = Wire.read(); //Almacenamiento del valor en una variable
  Serial.println(Rojo_ON_MSB);

  //Medicion del byte bajo del canal de luz roja cuando el LED esta encendido
  Wire.beginTransmission(B0111001); //Bit start + direccion del sensor I2C + Bit write
  Wire.write(B10010010); //Acceso al byte bajo del canal de luz roja (0x92)
  Wire.endTransmission(false); //Se inicia de nuevo el protocolo I2C (No se escribe, se lee)
  Wire.requestFrom(B0111001, 1); //Lectura de 1 byte (ultimo parametro) en este registro
  Rojo_ON_LSB = Wire.read(); //Almacenamiento del valor en una variable
  Serial.println(Rojo_ON_LSB);

  //Medidas con el LED apagado
  digitalWrite(0, 0); //LED rojo OFF
  delay(20); // Tiempo de espera 20ms (el LED se apaga mucho antes de finalizar los 20ms pero por si acaso)

  //Medicion del byte alto del canal de luz roja cuando el LED esta apagado
  Wire.beginTransmission(B0111001); //Bit start + Envio la direccion del sensor + write
  Wire.write(B10010011); //Acceso a byte alto del canal de luz roja (0x93)
  Wire.endTransmission(false); //Se inicia de nuevo el protocolo I2C (No se escribe, se lee)
  Wire.requestFrom(B0111001, 1); //Lectura de 1 byte (ultimo parametro) en este registro
  Rojo_OFF_MSB = Wire.read(); //Almacenamiento del valor en una variable
  Serial.println(Rojo_OFF_MSB);

  //Medicion del byte bajo del canal de luz roja cuando el LED esta apagado
  Wire.beginTransmission(B0111001); //Bit start + Envio la direccion del sensor + write
  Wire.write(B10010010); //Acceso al byte bajo del canal de luz roja (0x92)
  Wire.endTransmission(false); //Se inicia de nuevo el protocolo I2C (No se escribe, se lee)
  Wire.requestFrom(B0111001, 1); //Lectura de 1 byte (ultimo parametro) en este registro
  Rojo_OFF_LSB = Wire.read(); //Almacenamiento del valor en una variable
  Serial.println(Rojo_OFF_LSB);
}

void setup() { //Loop setup (solo se inicializa una vez)

  Serial.begin(9600); //Setup baudios (para depurar solo en el monitor serie)  
  pinMode(14, INPUT); //Puls1 declarado como input
  pinMode(0, OUTPUT); //Led rojo declarado como output

  if (digitalRead(14) == 0) { //Puls1 esta conectado a masa por lo que se lee un 0 cuando se pulsa
    modo = 0; //Si esta marcado el pin 14 (Puls1)
  } else {
    modo = 1; //variable de configuracion si esta presionado 0, si no 1
  }

  //Setup EEPROM
  EEPROM.begin(512); //Crea un buffer de RAM de 512 bytes y copia ahí los datos de la EEPROM
  
  //Setup conexion I2C
  Wire.setClock(400000); //Frecuencia de SCL indicada (Fast mode, 400kHz)
  Wire.begin(15,13); //Inicializacion de I2C con GPIO15 como SDA y GPIO13 como SCL

  //Setup Wifi
  WiFi.mode(WIFI_OFF); //Para evitar errores se apaga el WiFi para ponerlo en modo inicial
  delay(100); //Espera de 100ms
  
  if (modo == 0) { //Si el modo es 0 (configuracion)
    WiFi.softAPConfig(ip, puerta, mascara);
    WiFi.softAP("Mario"); //Crea un punto de acceso de read llamada "Mario"
    Serial.println("\nConfiguracion de la placa en: "); //Salto de linea y comentario
    Serial.println(WiFi.softAPIP()); //Se hace print de la IP
    servidor80.begin(); //Servidor de puerto 80 iniciado (HTTP)
  } else { //Modo 1 (funcionamiento normal)
    LeeEE(); //Llamada a la funcion de lectura de la EEPROM ya declarada
    ipEE.fromString(ipuser); //Saca la IP fija
    WiFi.config(ipEE, ipEE, mascara); //Ahora no son constantes, se leen de la EEPROM
    WiFi.begin(ssid, pass); //Inicializa conexion al WiFi con nombre de red y contraseña introducidos como parametros
    while (WiFi.status() != WL_CONNECTED) { //Si no se establece conexion
      delay(500); //Esperar medio segundo
      Serial.print("."); //Imprimir un punto
    }
    Serial.print("Conectado! IP address: "); //
    Serial.println(WiFi.localIP()); //
    servidor23.begin(); //Iniciacion del servidor en el puerto 23
  }
}

void loop() {
  if (modo == 0) { //modo 0
    if (!cliente) {
      cliente = servidor80.available(); //Mira si se conecto algun cliente al servidor        
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
            GuardaEE(aux, 70); //llamada a la funcion para hacer Write en la EEPROM
            Serial.println(aux);
            EEPROM.commit(); //Para grabar en la EEPROM de verdad, guarda lo que paso por EEPROM.write() grabando el buffer de RAM
          }
        }
      }
    }
  } else //el modo normal (modo == 1)
  {
    configsensorluz(); //Funcion para configurar y declarar los registros del sensor
    lecturasensor(); //Lectura de datos y procesamiento de bytes

    // Array datos[] compuesto por las medidas recogidas
    datos[0] = Rojo_ON_MSB;
    datos[1] = Rojo_ON_LSB;
    datos[2] = Rojo_OFF_MSB;
    datos[3] = Rojo_OFF_LSB;

    //En caso de que no haya ningun cliente conectado
    if (!cliente) { //Si no hay ningun cliente conectado estamos mirando hasta que alguien se conecte (o se conecta alguien o no hago nada)
      cliente = servidor23.available();
    }

    //En caso de que haya un cliente conectado es cuando ya realizamos la transmision de los datos
    if (cliente) {
      cliente.write(datos, 4); // Envío la medición
    }
  }

} //Fin loop()
