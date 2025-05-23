
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
#include <zephyr/drivers/rtc.h>
#include <hive.h>




#define HIVEMQ_PORT 8883

#define CLIENT_ID "esp32c3_mqtt_client"

#define MQTT_PUBLISH_TOPIC "esp32/data"
#define MQTT_SUBSCRIBE_TOPIC "esp32/receive"

#define MQTT_THREAD_STACK_SIZE 8192
#define MQTT_PRIORITY 4

#define HIVEMQ_CERTIFICATE_TAG 1

#define RTC_NODE DT_NODELABEL(rtc)

static const char hive_cert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";

static uint8_t rx_buff[32];
static uint8_t tx_buff[32];

static struct mqtt_client client;
static struct sockaddr_storage broker;
static struct pollfd fds[1];
static int nfds;

static struct net_mgmt_event_callback wifi_cb;

K_SEM_DEFINE(mqtt_sem, 0, 1);
K_SEM_DEFINE(dns_sem, 0, 1);
const struct device *rtc_dev;
K_MUTEX_DEFINE(rtc_mutex);

void set_rtc_time(struct rtc_time *time) {
    k_mutex_lock(&rtc_mutex, K_FOREVER);
    rtc_set_time(rtc_dev, time);
    k_mutex_unlock(&rtc_mutex);
    printf("RTC time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
        time->tm_year + 1900,
        time->tm_mon + 1,
        time->tm_mday,
        time->tm_hour,
        time->tm_min,
        time->tm_sec
    );
}

void init_rtc(void) {
    // init RTC device
    rtc_dev = DEVICE_DT_GET(RTC_NODE);
    if (rtc_dev == NULL) {
        printf("Failed to get RTC device\n");
        return;
    }
    // set initial time
    struct rtc_time set_time = {
        .tm_year = 2025 - 1900,  // Year since 1900
        .tm_mon = 5 - 1,         // Month (0-based)
        .tm_mday = 23,           // Day
        .tm_hour = 10,           // Hour
        .tm_min = 20,            // Minute
        .tm_sec = 0              // Second
    };
    set_rtc_time(&set_time);
    printf("RTC initialised");
}

static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) {
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        printf("WiFi connected!\n");
        k_sem_give(&mqtt_sem);
    }
}

static int connect_wifi(void) {
    
    static struct wifi_connect_req_params params;
    
    struct net_if *iface = net_if_get_default();
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, NET_EVENT_IPV4_ADDR_ADD);
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
    struct mqtt_puback_param puback;
    struct mqtt_pubrec_param pubrec;
    struct mqtt_pubrel_param pubrel;
    struct mqtt_pubcomp_param pubcomp;
    if (evt->type == MQTT_EVT_CONNACK) {
        if (evt->result != 0) {
			printf("Connection failed with status %d\n", evt->result);
		} else {
			printf("Client connected\n");
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

        printk("MQTT PUBLISH received: id=%d, qos=%d, length=%d\n",
               pub->message_id, pub->message.topic.qos, message_length);

        if (message_length > sizeof(rx_buff)) {
            message_length = sizeof(rx_buff);
        }

        /* Copy the received message to our buffer */
        int ret = mqtt_read_publish_payload(&client, rx_buff, message_length);
        if (ret < 0) {
            printf("Failed to read MQTT payload: %d\n", ret);
        }

        /* Null terminate the message for easier printing */
        rx_buff[message_length] = '\0';
        printk("Received: %s\n", rx_buff);
        /* Handle QoS levels */
        if (pub->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
            //puback.message_id = pub->message_id;
            //mqtt_publish_qos1_ack(client, &puback);
        }
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
    client.transport.tls.config.peer_verify = 2;
    //client.transport.tls.config.peer_verify = TLS_PEER_VERIFY_REQUIRED;
    client.transport.tls.config.cipher_list = NULL;
    client.transport.tls.config.sec_tag_list = (sec_tag_t []){ HIVEMQ_CERTIFICATE_TAG };
    client.transport.tls.config.sec_tag_count = 1;
    client.transport.tls.config.hostname = HIVEMQ_HOSTNAME;

    struct mqtt_utf8 username = {
        .utf8 = (uint8_t *)HIVEMQ_USERNAME,
        .size = strlen(HIVEMQ_USERNAME)
    };
    struct mqtt_utf8 password = {
        .utf8 = (uint8_t *)HIVEMQ_PASSWORD,
        .size = strlen(HIVEMQ_PASSWORD)
    };

    client.password = &password;
	client.user_name = &username;
    client.transport.type = MQTT_TRANSPORT_SECURE;
    ret = mqtt_connect(&client);
    if (ret < 0) {
        printf("Failed to connect to MQTT broker: %d\n", ret);
        return ret;
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
    param.message_id = 1;
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

void mqtt_thread(void* arg) {
    printf("Initialising MQTT Thread\n");
    char mqtt_message[32];
    //tls_credential_delete(HIVEMQ_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE);
    int ret;
    ret = tls_credential_add(HIVEMQ_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE, hive_cert, strlen(hive_cert) + 1);
    if (ret != 0) {
		printf("Failed to add credentials\n");
	}
    //init_rtc();
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
    
    
    if (client.transport.type == MQTT_TRANSPORT_SECURE) {
		fds[0].fd = client.transport.tls.sock;
	}
    fds[0].events = ZSOCK_POLLIN;
    nfds = 1;

    ret = mqtt_subscribe_topic(MQTT_SUBSCRIBE_TOPIC);
    if (ret < 0) {
        printf("Failed to subscribe to topic, error: %d\n", ret);
        return;
    }

    while(1) {
        /* Publish message */
        snprintf(mqtt_message, sizeof(mqtt_message), "Hello from ESP32C3 at %d", k_uptime_get_32());
        ret = mqtt_publish_message(MQTT_PUBLISH_TOPIC, mqtt_message);
        if (ret < 0) {
            printk("Failed to publish message, error: %d\n", ret);
        }

        /* Handle incoming MQTT events - received messages, etc. */
        if (poll(fds, nfds, 10000) < 0) {
            printf("Error in poll: %d\n", errno);
            break;
        }

        if (fds[0].revents & POLLIN) {
            ret = mqtt_input(&client);
            if (ret != 0) {
                printf("Error in mqtt_input: %d\n", ret);
                break;
            }
        }

        if (fds[0].revents & POLLERR) {
            printk("POLLERR\n");
            break;
        }

        if (fds[0].revents & POLLHUP) {
            printk("POLLHUP\n");
            break;
        }    
        
        k_msleep(10000);
    }
}


K_THREAD_DEFINE(mqtt_id, MQTT_THREAD_STACK_SIZE, mqtt_thread, NULL, NULL, NULL, MQTT_PRIORITY, 0, 0);