
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/sys/printk.h>
#include <string.h>
//#include "hive.h"
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>



#define HIVEMQ_PORT 1883

#define CLIENT_ID "esp32c3_mqtt_client"

#define MQTT_PUBLISH_TOPIC "esp32/receive"
#define MQTT_SUBSCRIBE_TOPIC "esp32/data"


#define WIFI_ID "user"
#define WIFI_PASSWORD "pass"


#define HIVEMQ_HOSTNAME "test.mosquitto.org"

#define MQTT_THREAD_STACK_SIZE 8192
#define MQTT_PRIORITY 4

static const struct device *mqtt_port = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static struct gpio_callback gpio_info;

static uint8_t rx_buff[1024];
static uint8_t tx_buff[1024];

static struct mqtt_client client;
static struct sockaddr_storage broker;
static struct pollfd fds[1];
static int nfds;
bool connected = false;

static struct net_mgmt_event_callback wifi_cb;
int gpio_vals[3] = {0, 0, 0};

K_SEM_DEFINE(mqtt_sem, 0, 1);
K_SEM_DEFINE(dns_sem, 0, 1);
const struct device *rtc_dev;
K_MUTEX_DEFINE(rtc_mutex);

static int mqtt_subscribe_topic(const char* topic);

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) {
    if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
        printf("WiFi connected!\n");
        k_sem_give(&mqtt_sem);
    }
}

static int connect_wifi(void) {
    
    static struct wifi_connect_req_params params;
    
    struct net_if *iface = net_if_get_default();
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, NET_EVENT_WIFI_CONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
    printf("Setting up connection\n");
    if (!iface) {
        printf("No default network interface available\n");
        return -1;
    }
    params.ssid = WIFI_ID;
    params.ssid_length = strlen(WIFI_ID);
    params.psk = WIFI_PASSWORD;
    params.psk_length = strlen(WIFI_PASSWORD);
    params.channel = WIFI_CHANNEL_ANY;
    params.security = WIFI_SECURITY_TYPE_PSK;
    params.mfp = WIFI_MFP_OPTIONAL;
    params.band = WIFI_FREQ_BAND_UNKNOWN;
    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) {
        printk("WiFi connection request failed: %d\n", ret);
        return ret;
    }
    k_sem_take(&mqtt_sem, K_FOREVER);
    if (ret) {
        printf("Timeout waiting for IP address\n");
        return -2;
    }
    printf("Connected to WiFi network: %s\n", WIFI_ID);
    return 0;
}

void dns_result_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data) {
	char hr_addr[NET_IPV4_ADDR_LEN];
	char *hr_family;
	void *addr;
    printf("Resolving ...\n");
	switch (status) {
	case DNS_EAI_CANCELED:
		printf("DNS query was canceled\n");
		return;
	case DNS_EAI_FAIL:
		printf("DNS resolve failed\n");
		return;
	case DNS_EAI_NODATA:
		printf("Cannot resolve address\n");
		return;
	case DNS_EAI_ALLDONE:
		printf("DNS resolving finished\n");
        k_sem_give(&dns_sem);
		return;
	case DNS_EAI_INPROGRESS:
		break;
	default:
		printf("DNS resolving error (%d)\n", status);
		return;
	}

	if (!info) {
		return;
	}

	if (info->ai_family == AF_INET) {
		hr_family = "IPv4";
		addr = &net_sin(&info->ai_addr)->sin_addr;
        struct sockaddr_in *broker_addr = (struct sockaddr_in *)&broker;
        broker_addr->sin_family = AF_INET;
        broker_addr->sin_port = htons(HIVEMQ_PORT);
        memcpy(&broker_addr->sin_addr, &net_sin(&info->ai_addr)->sin_addr, sizeof(struct in_addr));
        net_addr_ntop(AF_INET, &broker_addr->sin_addr, hr_addr, sizeof(hr_addr));
        printf("Resolved %s to IPv4 address: %s\n", HIVEMQ_HOSTNAME, hr_addr);
	} else if (info->ai_family == AF_INET6) {
		hr_family = "IPv6";
		addr = &net_sin6(&info->ai_addr)->sin6_addr;
	} else {
		printf("Invalid IP address family %d\n", info->ai_family);
		return;
	}

	printf("%s %s address: %s\n", user_data ? (char *)user_data : "<null>",
		hr_family, net_addr_ntop(info->ai_family, addr, hr_addr, sizeof(hr_addr)));
}

int resolve_dns(void) {
    printf("Attempting to resolve hostname: %s\n", HIVEMQ_HOSTNAME);
    struct dns_resolve_context *dns_ctx = dns_resolve_get_default();
    if (!dns_ctx) {
        printk("Error: DNS context not available\n");
        return -4;
    }
    printf("DNS Servers configured:\n");
    int ret = dns_get_addr_info(HIVEMQ_HOSTNAME, DNS_QUERY_TYPE_A, NULL, dns_result_cb, (void*)HIVEMQ_HOSTNAME, 10000);
    if (ret) {
        printf("Resolving Address Failed\n");
        return ret;
    }

    ret = k_sem_take(&dns_sem, K_FOREVER);
    struct sockaddr_in *broker_addr = (struct sockaddr_in *)&broker;
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &broker_addr->sin_addr, addr_str, sizeof(addr_str));
    printf("Successfully resolved %s to %s\n", HIVEMQ_HOSTNAME, addr_str);
    return 0;
}


static void mqtt_event_handler(struct mqtt_client *client_ptr, const struct mqtt_evt *evt) {
    printf("Handling ...\n");
    if (evt->type == MQTT_EVT_CONNACK) {
        if (evt->result != 0) {
			printf("Connection failed with status %d\n", evt->result);
		} else {
			printf("Client connected\n");
            connected = true;
            int ret = mqtt_subscribe_topic(MQTT_SUBSCRIBE_TOPIC);
            if (ret < 0) {
                printf("Failed to subscribe to topic, error: %d\n", ret);
            }
		}
    } else if (evt->type == MQTT_EVT_DISCONNECT) {
        printf("Client disconnected\n");
    } else if (evt->type == MQTT_EVT_PUBACK) {
        printf("PUBACK received for message ID %u\n", evt->param.puback.message_id);
    } else if (evt->type == MQTT_EVT_PUBREC) {
        printf("PUBREC received for message ID %u\n", evt->param.pubrec.message_id);
    } else if (evt->type == MQTT_EVT_PUBLISH) {
        const struct mqtt_publish_param *pub = &evt->param.publish;
        uint16_t message_length = pub->message.payload.len;

        printf("MQTT PUBLISH received: id=%d, qos=%d, length=%d\n",
               pub->message_id, pub->message.topic.qos, message_length);

        if (message_length > sizeof(rx_buff)) {
            message_length = sizeof(rx_buff);
        }

        // Copy the received message to buffer
        int ret = mqtt_read_publish_payload(&client, rx_buff, message_length);
        if (ret < 0) {
            printf("Failed to read MQTT payload: %d\n", ret);
        }

        
        rx_buff[message_length] = '\0';
        printk("Received: %s\n", rx_buff);
        
    } else {
        printf("Invalid Event Type\n");
    }
}

int broker_connect(void) {

    int ret = resolve_dns();
    if (ret) {
        return ret;
    }

    mqtt_client_init(&client);
    client.broker = &broker;
    client.evt_cb = mqtt_event_handler;
    client.client_id.utf8 = (uint8_t *)CLIENT_ID;
    client.client_id.size = strlen(CLIENT_ID);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.rx_buf = rx_buff;
	client.rx_buf_size = ARRAY_SIZE(rx_buff);
	client.tx_buf = tx_buff;
	client.tx_buf_size = ARRAY_SIZE(tx_buff);

    client.password = NULL;
	client.user_name = NULL;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    ret = mqtt_connect(&client);
    if (ret != 0) {
        printf("Failed to connect to MQTT broker: %d\n", ret);
        return ret;
    }
    while (!connected) {
        mqtt_input(&client);
        mqtt_live(&client);
        k_sleep(K_MSEC(100));
        printf("Nothing received yet\n");
    }
    return 0;
}

static int mqtt_publish_message(const char *topic, const char *message) {
    struct mqtt_publish_param param;

    param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *)topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *)message;
    param.message.payload.len = strlen(message);
    param.message_id = 7311;
    param.dup_flag = 0;
    param.retain_flag = 0;

    printf("Publishing message to topic %s: %s\n", topic, message);
    return mqtt_publish(&client, &param);
}

static int mqtt_subscribe_topic(const char* topic) {
    struct mqtt_topic topics[1];
    struct mqtt_subscription_list subscription;
    topics[0].topic.utf8 = (uint8_t *)topic;
    topics[0].topic.size = strlen(topic);
    topics[0].qos = MQTT_QOS_1_AT_LEAST_ONCE;

    subscription.list = topics;
    subscription.list_count = 1;
    subscription.message_id = 1;
    printf("Subscribing to topic: %s\n", topic);
    return mqtt_subscribe(&client, &subscription);

}

void gpio_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printf("Handler called\n");
    if (pins & BIT(4)) {
        mqtt_publish_message(MQTT_PUBLISH_TOPIC, "Volume Down");
    }
    if (pins & BIT(5)) {
        mqtt_publish_message(MQTT_PUBLISH_TOPIC, "Volume Up");
    }
    if (pins & BIT(6)) {
        mqtt_publish_message(MQTT_PUBLISH_TOPIC, "Ping!");
    }
}

void mqtt_thread(void* arg) {
    printf("Initialising MQTT Thread\n");
    //char mqtt_message[32];
    
    int ret;
    
    ret = connect_wifi();
    if (ret < 0) {
        printk("Failed to connect to WiFi network, error: %d\n", ret);
        return;
    }
    
    ret = broker_connect();
    if (ret < 0) {
        printk("Failed to initialize and connect to MQTT broker, error: %d\n", ret);
        return;
    }
    
    
    fds[0].fd = client.transport.tcp.sock;
    fds[0].events = ZSOCK_POLLIN;
    nfds = 1;
    gpio_pin_configure(mqtt_port, 4, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure(mqtt_port, 5, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure(mqtt_port, 6, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_interrupt_configure(mqtt_port, 4, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mqtt_port, 5, GPIO_INT_EDGE_BOTH);
    gpio_pin_interrupt_configure(mqtt_port, 6, GPIO_INT_EDGE_BOTH);
    gpio_init_callback(&gpio_info, gpio_handler, BIT(4) | BIT(5) | BIT(6));
    gpio_add_callback(mqtt_port, &gpio_info);
    while(1) {
        
        mqtt_live(&client);
        
        k_msleep(50);
    }
}


K_THREAD_DEFINE(mqtt_id, MQTT_THREAD_STACK_SIZE, mqtt_thread, NULL, NULL, NULL, MQTT_PRIORITY, 0, 0);