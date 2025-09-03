/**********************WIFI PARAMS**************************/
#define WIFI_SSID           "LSE"
#define WIFI_PASSWORD       "HubLS3s2"

/******************MQTT PARAMS AND DEFINES*********************/
#define MQTT_KEEPALIVE          60
#define MQTT_BROKER_IP          "131.255.83.223"    //"mqtt://lse.dev.br" //"192.168.45.197"
#define MQTT_BROKER_PORT        1883
#define MQTT_CLIENT_ID          "zephyr_client"
#define MQTT_USER_CONECTED      "Cliente Conectado"
#define MQTT_USER_DISCONNECTED  "Cliente Desconectado"
#define MQTT_USER_COMEDOR_ON    "Comedor Aberto"
#define MQTT_USER_COMEDOR_OFF   "Comedor Fechado"


/************************SERVO MOTOR***************************/
#define PWM_PERIOD          PWM_USEC(20000)
#define SERVO1_OPEN_US      PWM_USEC(1555)
#define SERVO1_CLOSE_US     PWM_USEC(2000)

// ON
#define SERVO2_ON_US        PWM_USEC(1400)
#define SERVO3_ON_US        PWM_USEC(2000)
#define SERVO4_ON_US        PWM_USEC(2500)

// OFF
#define SERVO2_OFF_US       PWM_USEC(1000)
#define SERVO3_OFF_US       PWM_USEC(1000)
#define SERVO4_OFF_US       PWM_USEC(500)


