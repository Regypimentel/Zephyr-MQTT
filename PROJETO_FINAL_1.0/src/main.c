#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <stdio.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define WIFI_SSID           "Ultra_Anne"
#define WIFI_PASSWORD       "30280070@"
#define MQTT_KEEPALIVE      60
#define MQTT_BROKER_IP      "192.168.1.105"
#define MQTT_BROKER_PORT    1883

static struct mqtt_client client;
static struct sockaddr_storage broker;
static bool wifi_connected = false;
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];
static struct net_mgmt_event_callback wifi_mgmt_cb;

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
    switch (evt->type) {
    case MQTT_EVT_CONNACK:
        LOG_INF("MQTT client connected!");

        // Subscribe to a topic
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

        int sub_rc = mqtt_subscribe(&client, &sub_list);
        if (sub_rc != 0) {
            LOG_ERR("Failed to subscribe: %d", sub_rc);
        } else {
            LOG_INF("Subscribed to topic: %s", subscribe_topic.topic.utf8);
        }

        // Publish a test message
        const char *topic = "meutopico/status";
        const char *payload = "Teste de mensagem";

        struct mqtt_publish_param pub_param = {
            .message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
            .message.topic.topic.utf8 = (char *)topic,
            .message.topic.topic.size = strlen(topic),
            .message.payload.data = (uint8_t *)payload,
            .message.payload.len = strlen(payload),
            .message_id = 1,
            .dup_flag = 0,
            .retain_flag = 0
        };

        int pub_rc = mqtt_publish(&client, &pub_param);
        if (pub_rc != 0) {
            LOG_ERR("Failed to publish message: %d", pub_rc);
        } else {
            LOG_INF("Message published to topic: %s", topic);
        }

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
            payload_buf[len] = '\0';  // Termina a string

            // Extrair a cor do botão
            char *color_ptr = strstr((char *)payload_buf, "Botão ");
            if (color_ptr) {
                color_ptr += strlen("Botão ");  // Avança para a cor
                char *space_ptr = strchr(color_ptr, ' ');
                if (space_ptr) {
                    *space_ptr = '\0';
                }
                printf("Botão: %s\n", color_ptr);

                // Verificar se a cor é Vermelho
                if (strcmp(color_ptr, "Vermelho") == 0) {
                    gpio_pin_set_dt(&led, 1);  // Liga LED
                } else {
                    gpio_pin_set_dt(&led, 0);  // Apaga LED
                }
            } else {
                printf("Botão: [Mensagem não reconhecida]\n");
                gpio_pin_set_dt(&led, 0);  // Apaga LED por segurança
            }

            if (p->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
                struct mqtt_puback_param ack = {
                    .message_id = evt->param.publish.message_id
                };
                mqtt_publish_qos1_ack(c, &ack);
            }
        } else {
            LOG_WRN("Received empty payload or failed to read payload");
        }
        break;
    }

    default:
        break;
    }
}

void mqtt_init_and_connect(void)
{
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

    int rc = mqtt_connect(&client);
    if (rc != 0) {
        LOG_ERR("Failed to connect to broker: %d", rc);
    }
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface)
{
    const struct wifi_status *status = cb->info;

    switch (mgmt_event) {
    case NET_EVENT_WIFI_CONNECT_RESULT:
        if (status->status) {
            LOG_ERR("Connection failed: %d", status->status);
        } else {
            LOG_INF("Successfully connected to WiFi!");
            wifi_connected = true;
            mqtt_init_and_connect();
        }
        break;

    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        if (status->status) {
            LOG_ERR("Disconnection failed: %d", status->status);
        } else {
            LOG_INF("Successfully disconnected from WiFi");
            wifi_connected = false;
        }
        break;

    default:
        LOG_INF("Unhandled event: 0x%08X", mgmt_event);
        break;
    }
}

int main(void)
{
    if (!device_is_ready(led.port)) {
        LOG_ERR("LED device not ready");
        return 0;
    }

    if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) < 0) {
        LOG_ERR("Failed to configure LED pin");
        return 0;
    }

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        LOG_ERR("No network interface available");
        return 0;
    }

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

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
                       &cnx_params, sizeof(cnx_params));
    if (ret) {
        LOG_ERR("Connection request failed: %d", ret);
    } else {
        LOG_INF("Connection requested");
    }

    while (1) {
        if (wifi_connected) {
            mqtt_input(&client);
            mqtt_live(&client);
        }
        k_sleep(K_MSEC(1000));
    }
}
