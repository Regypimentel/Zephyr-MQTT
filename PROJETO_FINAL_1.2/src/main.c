#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/drivers/pwm.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define WIFI_SSID           "Ultra_Anne"
#define WIFI_PASSWORD       "30280070@"
#define MQTT_KEEPALIVE      60
#define MQTT_BROKER_IP      "192.168.1.108"
#define MQTT_BROKER_PORT    1883

#define PWM_PERIOD          PWM_USEC(20000)
#define SERVO_DEFAULT_US    PWM_USEC(1500)

#define SERVO1_OPEN_US      PWM_USEC(1555)
#define SERVO1_CLOSE_US     PWM_USEC(2000)

#define SERVO2_DOWN_US      PWM_USEC(1900)
#define SERVO2_UP_US        PWM_USEC(1500)

#define SERVO3_DOWN_US      PWM_USEC(1900)
#define SERVO3_UP_US        PWM_USEC(1500)

#define SERVO4_LEFT_US      PWM_USEC(1800)
#define SERVO4_RIGHT_US     PWM_USEC(1200)

static const struct pwm_dt_spec servo1 = PWM_DT_SPEC_GET(DT_ALIAS(servo1));
static const struct pwm_dt_spec servo2 = PWM_DT_SPEC_GET(DT_ALIAS(servo2));
static const struct pwm_dt_spec servo3 = PWM_DT_SPEC_GET(DT_ALIAS(servo3));
static const struct pwm_dt_spec servo4 = PWM_DT_SPEC_GET(DT_ALIAS(servo4));

static uint32_t current_pulse_servo1 = SERVO1_OPEN_US;
static struct mqtt_client client;
static struct sockaddr_storage broker;
static bool wifi_connected = false;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct net_mgmt_event_callback wifi_mgmt_cb;

void move_servo_smoothly(const struct pwm_dt_spec *servo, uint32_t from_us, uint32_t to_us, int steps, int delay_ms) {
    if (from_us == to_us) return;
    int32_t delta = (int32_t)(to_us - from_us);
    int32_t step_size = delta / steps;
    int32_t current = from_us;

    for (int i = 0; i < steps; i++) {
        pwm_set_pulse_dt(servo, current);
        current += step_size;
        k_sleep(K_MSEC(delay_ms));
    }
    pwm_set_pulse_dt(servo, to_us);
}

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt) {
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        LOG_INF("MQTT client connected!");
        struct mqtt_topic subscribe_topic = {
            .topic = {
                .utf8 = "meutopico/comandos",
                .size = strlen("meutopico/comandos")
            },
            .qos = MQTT_QOS_1_AT_LEAST_ONCE
        };
        struct mqtt_subscription_list sub_list = {
            .list = &subscribe_topic,
            .list_count = 1,
            .message_id = 1
        };
        mqtt_subscribe(&client, &sub_list);
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected, retrying in 5s");
        mqtt_disconnect(&client, false);
        k_sleep(K_SECONDS(5));
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *p = &evt->param.publish;
        uint8_t payload_buf[256];
        int len = mqtt_read_publish_payload(c, payload_buf, sizeof(payload_buf));

        if (len > 0) {
            payload_buf[len] = '\0';
            LOG_INF("Mensagem recebida: %s", payload_buf);

            if (strstr(payload_buf, "Vermelho")) {
                move_servo_smoothly(&servo1, current_pulse_servo1, SERVO1_OPEN_US, 50, 30);
                current_pulse_servo1 = SERVO1_OPEN_US;
                move_servo_smoothly(&servo2, SERVO2_UP_US, SERVO2_DOWN_US, 50, 30);
                move_servo_smoothly(&servo3, SERVO3_UP_US, SERVO3_DOWN_US, 50, 30);
                move_servo_smoothly(&servo1, current_pulse_servo1, SERVO1_CLOSE_US, 50, 30);
                current_pulse_servo1 = SERVO1_CLOSE_US;
                move_servo_smoothly(&servo2, SERVO2_DOWN_US, SERVO2_UP_US, 50, 30);
                move_servo_smoothly(&servo3, SERVO3_DOWN_US, SERVO3_UP_US, 50, 30);
            } else if (strstr(payload_buf, "Amarelo")) {
                move_servo_smoothly(&servo4, SERVO_DEFAULT_US, SERVO4_LEFT_US, 50, 30);
            } else if (strstr(payload_buf, "Azul")) {
                move_servo_smoothly(&servo4, SERVO_DEFAULT_US, SERVO4_RIGHT_US, 50, 30);
            } else if (strstr(payload_buf, "Verde")) {
                move_servo_smoothly(&servo1, current_pulse_servo1, SERVO_DEFAULT_US, 50, 30);
                current_pulse_servo1 = SERVO_DEFAULT_US;
                move_servo_smoothly(&servo2, SERVO2_UP_US, SERVO_DEFAULT_US, 50, 30);
                move_servo_smoothly(&servo3, SERVO3_UP_US, SERVO_DEFAULT_US, 50, 30);
                move_servo_smoothly(&servo4, SERVO_DEFAULT_US, SERVO_DEFAULT_US, 1, 30);
            } else {
                LOG_WRN("Comando desconhecido: %s", payload_buf);
            }

            if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
                struct mqtt_puback_param ack = {
                    .message_id = evt->param.publish.message_id
                };
                mqtt_publish_qos1_ack(c, &ack);
            }
        }
        break;
    }

    default:
        break;
    }
}

void mqtt_init_and_connect(void) {
    struct mqtt_utf8 broker_host = {
        .utf8 = MQTT_BROKER_IP,
        .size = strlen(MQTT_BROKER_IP),
    };

    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(MQTT_BROKER_PORT);
    net_addr_pton(AF_INET, broker_host.utf8, &broker4->sin_addr);

    mqtt_client_init(&client);
    client.broker = &broker;
    client.client_id.utf8 = "zephyr_client";
    client.client_id.size = strlen(client.client_id.utf8);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    client.keepalive = MQTT_KEEPALIVE;
    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);
    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);
    client.evt_cb = mqtt_evt_handler;

    mqtt_connect(&client);
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) {
    const struct wifi_status *status = cb->info;

    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        if (!status->status) {
            wifi_connected = true;
            mqtt_init_and_connect();
        }
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        wifi_connected = false;
        break;
    default:
        break;
    }
}

int main(void) {
    if (!device_is_ready(servo1.dev) || !device_is_ready(servo2.dev) ||
        !device_is_ready(servo3.dev) || !device_is_ready(servo4.dev)) {
        return 0;
    }

    pwm_set_pulse_dt(&servo1, SERVO1_OPEN_US);
    pwm_set_pulse_dt(&servo2, SERVO2_UP_US);
    pwm_set_pulse_dt(&servo3, SERVO3_UP_US);
    pwm_set_pulse_dt(&servo4, SERVO_DEFAULT_US);
    current_pulse_servo1 = SERVO1_OPEN_US;

    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params cnx_params = {
        .ssid = WIFI_SSID,
        .ssid_length = strlen(WIFI_SSID),
        .psk = WIFI_PASSWORD,
        .psk_length = strlen(WIFI_PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };

    net_mgmt_init_event_callback(&wifi_mgmt_cb,
                                 wifi_mgmt_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_mgmt_cb);

    net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
             &cnx_params, sizeof(cnx_params));

    while (1) {
        if (wifi_connected) {
            mqtt_input(&client);
            mqtt_live(&client);
        }
        k_sleep(K_MSEC(1000));
    }
}
