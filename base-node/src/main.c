#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/rtc.h>
#include "rtc.h"
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "auth.h"
#include "zephyr/net/net_ip.h"
#include "zephyr/net/wifi.h"
#include <string.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

static int connect_wifi(void);
void wifi_status(void);
void setup_wifi(void);
int connect_to_ip(void);

K_SEM_DEFINE(wifi_sem, 0, 1);

/* Simple HTTP GET for "/hello" */
static const char http_request[] =
    "GET / HTTP/1.1\r\n"
    "Host: " SERVER_IP "\r\n"
    "Connection: close\r\n"
    "\r\n";

static struct net_mgmt_event_callback wifi_cb;

int main(void) {

    init_rtc();
    setup_wifi();
    printk("Here\n");
    k_sem_take(&wifi_sem, K_FOREVER);

    printk("Here 2\n");
    int sock = connect_to_ip();
    int ret = zsock_send(sock, http_request, strlen(http_request), 0);
    if (ret < 0) {
        printk("HTTP send failed: %d\n", ret);
        return -1;
    }

    char buf[512];
    while (true) {
        int rx = zsock_recv(sock, buf, sizeof(buf) - 1, 0);
        if (rx > 0) {
            buf[rx] = '\0';
            printk("%s", buf);
        } else if (rx == 0) {
            /* peer closed cleanly */
            break;
        } else {
            printk("HTTP recv error: %d\n", rx);
            break;
        }
    }

    printk("\n");


    while (1) {

        LOG_INF("Hello, World!");

        k_sleep(K_MSEC(30000));

        wifi_status();
    }
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

    printk("Attempting to connect to %s\n", WIFI_ID);
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


static void http_response_cb(struct http_response *rsp,
			enum http_final_call final_data,
			void *user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		//printk("Partial data received (%zd bytes)\n", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		//printk("All the data received (%zd bytes)\n", rsp->data_len);
	}

	//printk("Bytes Recv %zd\n", rsp->data_len);
	//printk("Response status %s\n", rsp->http_status);
	//printk("Recv Buffer Length %zd\n", rsp->recv_buf_len);
	printk("%.*s", rsp->data_len, rsp->recv_buf);
}


void http_get(int sock, char * hostname, char * url)
{
	struct http_request req = { 0 };
	static uint8_t recv_buf[512];
	int ret;

	req.method = HTTP_GET;
	req.url = url;
	req.host = hostname;
	req.protocol = "HTTP/1.1";
	req.response = http_response_cb;
	req.recv_buf = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	/* sock is a file descriptor referencing a socket that has been connected
	* to the HTTP server.
	*/
	ret = http_client_req(sock, &req, 5000, NULL);
}


int connect_to_ip(void) {
    // create socket
    int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printk("Socket creation failed: %d\n", sock);
        return -1;
    }

    // generate address info
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
    };

    if (zsock_inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) != 1) {
        printk("inet_pton failed\n");
        zsock_close(sock);
        return -1;
    }

    printk("Connecting to %s:%u… ", SERVER_IP, SERVER_PORT);

    int ret = zsock_connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret) {
        printk("Failed to connect: %d\n", ret);
        zsock_close(sock);
        return -1;
    }

    printk("Successfully connected to socket!\n");
    return sock;
}