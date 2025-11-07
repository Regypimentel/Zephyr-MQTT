/****************INCLUDES*****************/
#include <zephyr/net/mqtt.h>
#include <zephyr/drivers/pwm.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/net/socket.h> 
#include "defines.h"

#define SERVO1 DT_ALIAS(servo1)
static const struct pwm_dt_spec servo1 =    PWM_DT_SPEC_GET(SERVO1);
static const uint32_t close_position =      DT_PROP(SERVO1, min_pulse);
static const uint32_t open_position =       DT_PROP(SERVO1, max_pulse);

enum {
    OK,
    ERR_CONNECT_WI_FI,
    ERR_CONNECT_MQTT,
    ERR_MQTT_ADRESS,
    ERR_SERVO_MOTOR,
};

struct wifi_connect_req_params cnx_params = {
    .ssid = WIFI_SSID,
    .ssid_length = strlen(WIFI_SSID),
    .psk = WIFI_PASSWORD,
    .psk_length = strlen(WIFI_PASSWORD),
    .channel = WIFI_CHANNEL_ANY,
    .security = WIFI_SECURITY_TYPE_PSK,
};

const struct mqtt_topic subscribe_topic = {
    .topic = {
        .utf8 = "meutopico/comandos",
        .size = strlen("meutopico/comandos")
    },
    .qos = MQTT_QOS_1_AT_LEAST_ONCE
};

const struct mqtt_subscription_list sub_list = {
    .list = (struct mqtt_topic *)&subscribe_topic,
    .list_count = 1,
    .message_id = 1
};
 

const struct mqtt_topic publish_topic = {
    .topic = {
        .utf8 = "meutopico/status",
        .size = strlen("meutopico/status")
    },
    .qos = MQTT_QOS_1_AT_LEAST_ONCE
};

const struct mqtt_publish_param pub_param = {
    .message = {
        .topic = publish_topic,
        .payload = {
            .data = (uint8_t *)MQTT_USER_CONECTED,
            .len = strlen(MQTT_USER_CONECTED)
        }
    },
    .message_id = 1,
    .dup_flag = 0,
    .retain_flag = 0
};

const struct mqtt_publish_param publish_opening = {
    .message = {
        .topic = publish_topic,
        .payload = {
            .data = (uint8_t *)MQTT_USER_COMEDOR_OPENING,
            .len = strlen(MQTT_USER_COMEDOR_OPENING)
        }
    },
    .message_id = 1,
    .dup_flag = 0,
    .retain_flag = 0    
};

const struct mqtt_publish_param publish_open = {
    .message = {
        .topic = publish_topic,
        .payload = {
            .data = (uint8_t *)MQTT_USER_COMEDOR_OPEN,
            .len = strlen(MQTT_USER_COMEDOR_OPEN)
        }
    },
    .message_id = 1,
    .dup_flag = 0,
    .retain_flag = 0    
};

const struct mqtt_publish_param publish_closing = {
    .message = {
        .topic = publish_topic,
        .payload = {
            .data = (uint8_t *)MQTT_USER_COMEDOR_CLOSING,
            .len = strlen(MQTT_USER_COMEDOR_CLOSING)
        }
    },
    .message_id = 1,
    .dup_flag = 0,
    .retain_flag = 0    
};

const struct mqtt_publish_param publish_close = {
    .message = {
        .topic = publish_topic,
        .payload = {
            .data = (uint8_t *)MQTT_USER_COMEDOR_CLOSE,
            .len = strlen(MQTT_USER_COMEDOR_CLOSE)
        }
    },
    .message_id = 1,
    .dup_flag = 0,
    .retain_flag = 0    
};

void modo_auto(int arg);

int mqtt_init_and_connect(void);

int move_servo_smoothly(const struct pwm_dt_spec *servo, uint32_t from_us, uint32_t to_us, int steps, int delay_ms);

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt);

