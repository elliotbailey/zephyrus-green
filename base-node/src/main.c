#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/rtc.h>
#include "rtc.h"
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "auth.h"
#include "zephyr/net/wifi.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// #define SERVER_NAME "lunar-river-product.glitch.me"
// #define SERVER_PORT "3000"


static struct sockaddr_storage broker;

// static const char http_request[] =
//     "GET / HTTP/1.1\r\n"
//     "Host: " SERVER_NAME "\r\n"
//     "Connection: close\r\n\r\n";

static int connect_wifi(void);
void wifi_status(void);
void setup_wifi(void);
int resolve_dns(void);

K_SEM_DEFINE(dns_sem, 0, 1);
K_SEM_DEFINE(wifi_sem, 0, 1);

static struct net_mgmt_event_callback wifi_cb;

int main(void) {

    init_rtc();
    setup_wifi();
    k_sem_take(&wifi_sem, K_FOREVER);
    printk("DNS return: %d\n", resolve_dns());


    while (1) {

        LOG_INF("Hello, World!");

        k_sleep(K_MSEC(30000));

        wifi_status();
    }
    return 0;
}


void dns_result_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data) {
    k_sem_give(&dns_sem);
	char hr_addr[NET_IPV4_ADDR_LEN];
	char *hr_family;
	void *addr;
    printk("Resolving ...\n");
	switch (status) {
	case DNS_EAI_CANCELED:
		printk("DNS query was canceled\n");
		return;
	case DNS_EAI_FAIL:
		printk("DNS resolve failed\n");
		return;
	case DNS_EAI_NODATA:
		printk("Cannot resolve address\n");
		return;
	case DNS_EAI_ALLDONE:
		printk("DNS resolving finished\n");
		return;
	case DNS_EAI_INPROGRESS:
		break;
	default:
		printk("DNS resolving error (%d)\n", status);
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
        printk("Resolved %s to IPv4 address: %s\n", HIVEMQ_HOSTNAME, hr_addr);
	} else if (info->ai_family == AF_INET6) {
		hr_family = "IPv6";
		addr = &net_sin6(&info->ai_addr)->sin6_addr;
	} else {
		printk("Invalid IP address family %d\n", info->ai_family);
		return;
	}

	printk("%s %s address: %s\n", user_data ? (char *)user_data : "<null>",
		hr_family, net_addr_ntop(info->ai_family, addr, hr_addr, sizeof(hr_addr)));
}


int resolve_dns(void) {
    printk("Attempting to resolve hostname: %s\n", HIVEMQ_HOSTNAME);
    struct dns_resolve_context *dns_ctx = dns_resolve_get_default();
    if (!dns_ctx) {
        printk("Error: DNS context not available\n");
        return -4;
    }
    printk("DNS Servers configured:\n");
    int ret = dns_get_addr_info(HIVEMQ_HOSTNAME, DNS_QUERY_TYPE_A, NULL, dns_result_cb, (void*)HIVEMQ_HOSTNAME, 10000);
    if (ret) {
        printk("Resolving Address Failed\n");
        return ret;
    }
    k_sem_take(&dns_sem, K_FOREVER);
    struct sockaddr_in *broker_addr = (struct sockaddr_in *)&broker;
    char addr_str[NET_IPV4_ADDR_LEN];
    net_addr_ntop(AF_INET, &broker_addr->sin_addr, addr_str, sizeof(addr_str));
    printk("Successfully resolved %s to %s\n", HIVEMQ_HOSTNAME, addr_str);
    return 0;
}


static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) {
    if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
        printk("WiFi connected!\n");
        k_sem_give(&wifi_sem);
    }
}


void wifi_status(void)
{
    struct net_if *iface = net_if_get_default();
    
    struct wifi_iface_status status = {0};

    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,	sizeof(struct wifi_iface_status)))
    {
        printk("WiFi Status Request Failed\n");
    }

    printk("\n");

    if (status.state >= WIFI_STATE_ASSOCIATED) {
        printk("SSID: %-32s\n", status.ssid);
        printk("Band: %s\n", wifi_band_txt(status.band));
        printk("Channel: %d\n", status.channel);
        printk("Security: %s\n", wifi_security_txt(status.security));
        printk("RSSI: %d\n", status.rssi);
    }
}


void setup_wifi(void) {
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler, NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&wifi_cb);

    connect_wifi();
}


static int connect_wifi(void) {
    
    static struct wifi_connect_req_params params;
    
    struct net_if *iface = net_if_get_default();
    printk("Setting up connection\n");
    if (!iface) {
        printk("No default network interface available\n");
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
    return 0;
}