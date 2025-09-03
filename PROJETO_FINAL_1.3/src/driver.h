/****************INCLUDES*****************/
#include <zephyr/net/mqtt.h>
#include <zephyr/drivers/pwm.h>
#include <string.h>
#include <stdio.h>
#include <zephyr/net/socket.h> 
#include "defines.h"


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
};

const struct mqtt_publish_param publish_open = {
    .message = {
        .topic = publish_topic,
        .payload = {
            .data = (uint8_t *)MQTT_USER_COMEDOR_ON,
            .len = strlen(MQTT_USER_COMEDOR_ON)
        }
    },
};

void mqtt_init_and_connect(void);

void move_servo_smoothly(const struct pwm_dt_spec *servo, uint64_t from_us, uint64_t to_us, int steps, int delay_ms);

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt);

void mqtt_init_and_connect(void);
