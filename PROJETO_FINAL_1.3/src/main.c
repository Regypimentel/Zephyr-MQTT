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
#include "driver.h"
#include "defines.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define SERVO1 DT_ALIAS(servo1)
static const struct pwm_dt_spec servo1 = PWM_DT_SPEC_GET(SERVO1);

static struct mqtt_client client;
static struct sockaddr_storage broker;
static bool wifi_connected = false;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct net_mgmt_event_callback wifi_mgmt_cb;
static bool arm_mode_on = false; 
static bool item_grabbed = false; 

void move_servo_smoothly(const struct pwm_dt_spec *servo, uint64_t from_us, uint64_t to_us, int steps, int delay_ms) {
    if (from_us == to_us) return;
    int32_t delta = (int32_t)(to_us - from_us);
    int32_t step_size = delta / steps;
    int32_t current = from_us;

    for (int i = 0; i < steps; i++) {
        pwm_set_pulse_dt(servo, current);
        current += step_size;
        k_msleep(delay_ms);
    }
    pwm_set_pulse_dt(servo, to_us);
}

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt) {

    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        LOG_INF("Cliente MQTT conectado!");
        mqtt_subscribe(&client, &sub_list);
        mqtt_publish(&client, &pub_param);
        break;

    case MQTT_EVT_DISCONNECT:
        LOG_INF("MQTT client disconnected, retrying in 5s");
        mqtt_disconnect(&client, false);
        k_msleep(5000);
        mqtt_init_and_connect();
        break;

    case MQTT_EVT_PUBLISH: {
        const struct mqtt_publish_param *p = &evt->param.publish;
        uint8_t payload_buf[256];
        int len = mqtt_read_publish_payload(c, payload_buf, sizeof(payload_buf));

        if (len > 0) {
            payload_buf[len] = '\0';
            LOG_INF("Mensagem recebida: %s", payload_buf);

            if (strcmp(payload_buf, "ON") == 0) {
                LOG_INF("Comando ON recebido(Abrindo comedor)");
                arm_mode_on = true;
                pwm_set_pulse_dt(&servo1, SERVO1_OPEN_US);
                item_grabbed = false;
                mqtt_publish(&client, &publish_open);
            }
            else if (strcmp(payload_buf, "OFF") == 0) {
                LOG_INF("Comando OFF recebido(Fechando comedor)");
                arm_mode_on = false;
                pwm_set_pulse_dt(&servo1, SERVO1_CLOSE_US);
                mqtt_publish(&client, &publish_off);
            }else {
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
    
    int err;
    struct zsock_addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM
    };
    struct zsock_addrinfo *res;

    err = zsock_getaddrinfo(MQTT_BROKER_IP, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        LOG_ERR("Failed to resolve broker address: %d", err);
        freeaddrinfo(res);
        return;
    }

    struct sockaddr_in *addr4 = (struct sockaddr_in *)res->ai_addr;
    struct sockaddr_in *broker4 = (struct sockaddr_in *)&broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(MQTT_BROKER_PORT);
    broker4->sin_addr = addr4->sin_addr;

    char ipstr[NET_IPV4_ADDR_LEN];
    inet_ntop(AF_INET, &addr4->sin_addr, ipstr, sizeof(ipstr));
    LOG_INF("Conectando em %s (%s):%d", MQTT_BROKER_IP, ipstr, MQTT_BROKER_PORT);
    
    freeaddrinfo(res);
    
    mqtt_client_init(&client);
    client.broker = &broker;
    client.protocol_version = MQTT_VERSION_3_1_0;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    client.keepalive = MQTT_KEEPALIVE;
    client.client_id.utf8 = "zephyr_client";
    client.client_id.size = strlen("zephyr_client");
    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);
    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);
    client.evt_cb = mqtt_evt_handler;
   
    err = mqtt_connect(&client);
    if (err != 0)
    {
        LOG_ERR("Erro na conexão com o mqqt_connect: %d", err);
        return;
    }
    
    LOG_INF("MQTT_connect() OK enviando resposta via MQTT, aguardando CONNACK…");
    
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {

    const struct wifi_status *status = cb->info;

    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        if (!status->status) {
            wifi_connected = true;
            LOG_INF("Wi-Fi connected successfully to SSID: %s", WIFI_SSID);
            mqtt_init_and_connect();
        }
    break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        wifi_connected = false;
        LOG_INF("Wi-Fi disconnected, status: %d", status->status);
    break;
    default:
        LOG_WRN("Wi-Fi evento desconhecido: %d", status->status);
    break;
    }
}

int main(void) {

    LOG_INF("Starting Zephyr MQTT Client with Wi-Fi");

    if (!device_is_ready(servo1.dev)) {
        LOG_ERR("Failed to initialize PWM devices.");
        return 0;
    }
    LOG_INF("PWM devices initialized successfully.");

    pwm_set_pulse_dt(&servo1, SERVO1_CLOSE_US);  

    struct net_if *iface = net_if_get_default();
    struct wifi_connect_req_params cnx_params = {
        .ssid = WIFI_SSID,
        .ssid_length = sizeof(WIFI_SSID),
        .psk = WIFI_PASSWORD,
        .psk_length = sizeof(WIFI_PASSWORD),
        .channel = WIFI_CHANNEL_ANY,
        .security = WIFI_SECURITY_TYPE_PSK,
    };

    LOG_INF("Connecting to Wi-Fi SSID: %s", WIFI_SSID);

    net_mgmt_init_event_callback(&wifi_mgmt_cb, wifi_mgmt_event_handler, NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_mgmt_cb);

    net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(cnx_params));

    while (1) {
        if (wifi_connected) {
            mqtt_input(&client);
            mqtt_live(&client);
            LOG_DBG("Waiting for events...");
        }
        k_msleep(1000);
    }
}
