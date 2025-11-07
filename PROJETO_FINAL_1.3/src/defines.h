/**********************WIFI PARAMS**************************/
#define WIFI_SSID                           "LSE"
#define WIFI_PASSWORD                       "HubLS3s2"

/******************MQTT PARAMS AND DEFINES*********************/
#define MQTT_KEEPALIVE                      60
#define MQTT_BROKER_IP                      "186.209.102.89"    //"mqtt://lse.dev.br" //"192.168.45.197"
#define MQTT_BROKER_PORT                    1883
#define MQTT_CLIENT_ID                      "zephyr_client"
#define MQTT_USER_CONECTED                  "Cliente Conectado"
#define MQTT_USER_DISCONNECTED              "Cliente Desconectado"
#define MQTT_USER_COMEDOR_OPENING           "Abrindo Comedor"
#define MQTT_USER_COMEDOR_OPEN              "Comedor Aberto"
#define MQTT_USER_COMEDOR_CLOSING           "Fechando Comedor"
#define MQTT_USER_COMEDOR_CLOSE             "Comedor Fechado"
#define MQTT_USER_AUTO_ON                   "Comedor Modo Auto Ligado"
#define MQTT_USER_AUTO_OFF                  "Comedor Modo Auto Desligado"



/************************SERVO MOTOR***************************/
#define PWM_PERIOD                          PWM_USEC(20000)
#define SERVO1_OPEN_US                      PWM_USEC(1555)
#define SERVO1_CLOSE_US                     PWM_USEC(2000)

/***********************NTP DEFINES****************************/
#define NTP_UNIX_EPOCH_OFFSET 2208988800UL
#define SNTP_MAX_RETRY  5

/***********************THREAD DEFINES**************************/
#define STACKSIZE                          1024
#define PRIORITY                            5

/***********************INTERVALOR DE HORAS*********************/
#define INTERVALO_COMEDOR_HORAS   12
#define INTERVALO_COMEDOR_SEGUNDOS (INTERVALO_COMEDOR_HORAS * 3600)


