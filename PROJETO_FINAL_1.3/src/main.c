#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/drivers/pwm.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define WIFI_SSID           "S21 FE de Mariana"
#define WIFI_PASSWORD       "pevo1234"
#define MQTT_KEEPALIVE      60
#define MQTT_BROKER_IP      "192.168.45.197"
#define MQTT_BROKER_PORT    1883

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

// GET
#define SERVO2_GET_US       PWM_USEC(2000)
#define SERVO3_GET_US       PWM_USEC(2000)

// 1
#define SERVO2_ONE_US       PWM_USEC(1600)
#define SERVO3_ONE_US       PWM_USEC(1600)
#define SERVO4_ONE_US       PWM_USEC(1900)

// 2
#define SERVO2_TWO_US       PWM_USEC(1800)
#define SERVO3_TWO_US       PWM_USEC(1700)
#define SERVO4_TWO_US       PWM_USEC(2100)

static struct mqtt_client client;
static struct sockaddr_storage broker;
static bool wifi_connected = false;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct net_mgmt_event_callback wifi_mgmt_cb;

// PWM configuration for the servos
static const struct pwm_dt_spec servo1 = PWM_DT_SPEC_GET(DT_ALIAS(servo1));
static const struct pwm_dt_spec servo2 = PWM_DT_SPEC_GET(DT_ALIAS(servo2));
static const struct pwm_dt_spec servo3 = PWM_DT_SPEC_GET(DT_ALIAS(servo3));
static const struct pwm_dt_spec servo4 = PWM_DT_SPEC_GET(DT_ALIAS(servo4));

static bool arm_mode_on = false; // Controla se o braço está em modo ON ou OFF
static bool item_grabbed = false; // Controla se o item foi pego

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

            if (strcmp(payload_buf, "ON") == 0) {
                LOG_INF("Comando ON recebido");
                arm_mode_on = true;
                pwm_set_pulse_dt(&servo1, SERVO1_OPEN_US);
                pwm_set_pulse_dt(&servo2, SERVO2_ON_US);
                pwm_set_pulse_dt(&servo3, SERVO3_ON_US);
                pwm_set_pulse_dt(&servo4, SERVO4_ON_US);
                item_grabbed = false;
            }
            else if (strcmp(payload_buf, "OFF") == 0) {
                LOG_INF("Comando OFF recebido");
                arm_mode_on = false;
                pwm_set_pulse_dt(&servo1, SERVO1_CLOSE_US);
                pwm_set_pulse_dt(&servo2, SERVO2_OFF_US);
                pwm_set_pulse_dt(&servo3, SERVO3_OFF_US);
                pwm_set_pulse_dt(&servo4, SERVO4_OFF_US);
            }
            else if (strcmp(payload_buf, "GET") == 0 && arm_mode_on) {
                LOG_INF("Comando GET recebido - Pegando objeto");
                pwm_set_pulse_dt(&servo4, SERVO4_ON_US);
		pwm_set_pulse_dt(&servo2, SERVO3_GET_US);
                pwm_set_pulse_dt(&servo3, SERVO2_GET_US);
		k_sleep(K_SECONDS(2));


                pwm_set_pulse_dt(&servo1, SERVO1_CLOSE_US);

                k_sleep(K_SECONDS(1));
                item_grabbed = true;

                pwm_set_pulse_dt(&servo2, SERVO2_ON_US);
                pwm_set_pulse_dt(&servo3, SERVO3_ON_US);
            }
            else if (strcmp(payload_buf, "1") == 0 && arm_mode_on && item_grabbed) {
                LOG_INF("Comando 1 recebido - Deixando objeto no destino 1");
                pwm_set_pulse_dt(&servo2, SERVO2_ONE_US);
                pwm_set_pulse_dt(&servo3, SERVO3_ONE_US);
                pwm_set_pulse_dt(&servo4, SERVO4_ONE_US);

		k_sleep(K_SECONDS(5));
		pwm_set_pulse_dt(&servo1, SERVO1_OPEN_US);
		k_sleep(K_SECONDS(3));
		pwm_set_pulse_dt(&servo4, SERVO4_ON_US);
		pwm_set_pulse_dt(&servo2, SERVO2_ON_US);
		pwm_set_pulse_dt(&servo3, SERVO3_ON_US);


                item_grabbed = false;
            }
            else if (strcmp(payload_buf, "2") == 0 && arm_mode_on && item_grabbed) {
                LOG_INF("Comando 2 recebido - Deixando objeto no destino 2");
                pwm_set_pulse_dt(&servo2, SERVO2_TWO_US);
                pwm_set_pulse_dt(&servo3, SERVO3_TWO_US);
                pwm_set_pulse_dt(&servo4, SERVO4_TWO_US);

		k_sleep(K_SECONDS(5));
		pwm_set_pulse_dt(&servo1, SERVO1_OPEN_US);

		k_sleep(K_SECONDS(3));
		pwm_set_pulse_dt(&servo4, SERVO4_ON_US);
		pwm_set_pulse_dt(&servo2, SERVO2_ON_US);
		pwm_set_pulse_dt(&servo3, SERVO3_ON_US);



                item_grabbed = false;
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
    if (!device_is_ready(servo1.dev) || !device_is_ready(servo2.dev) || !device_is_ready(servo3.dev) || !device_is_ready(servo4.dev)) {
        LOG_ERR("Failed to initialize PWM devices.");
        return 0;
    }

    pwm_set_pulse_dt(&servo1, SERVO1_CLOSE_US); // Inicializa no modo OFF
    pwm_set_pulse_dt(&servo2, SERVO2_OFF_US);
    pwm_set_pulse_dt(&servo3, SERVO3_OFF_US);
    pwm_set_pulse_dt(&servo4, SERVO4_OFF_US); // Começa em posição de descanso

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
