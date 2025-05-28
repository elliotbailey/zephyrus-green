#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/rtc.h>
#include "rtc.h"
#include <zephyr/net/socket.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include "auth.h"
#include <string.h>
#include <stdio.h>
#include "wifi.h"
#include "socket.h"
#include <stdbool.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

void process_received_packet(char* packet_buf);
void send_volume_change(bool up);
void receive_ferry_packet(char* packet_buf, size_t packet_buf_size);

/* Simple HTTP GET for "/hello" */
static const char GET_REQ_FERRY[] =
    "GET /ferry HTTP/1.1\r\n"
    "Host: " SERVER_IP "\r\n"
    "Connection: close\r\n"
    "\r\n";

int main(void) {

    init_rtc();
    setup_wifi();

    while(1) {
        char packet_buf[512];
        receive_ferry_packet(packet_buf, sizeof(packet_buf));

        process_received_packet(packet_buf);

        k_sleep(K_MSEC(3000));
        send_volume_change(true);
    }


    while (1) {

        LOG_INF("Hello, World!");

        k_sleep(K_MSEC(30000));

        wifi_status();
    }
    return 0;
}


void receive_ferry_packet(char* packet_buf, size_t packet_buf_size) {
    int sock = connect_to_ip();
    int ret = zsock_send(sock, GET_REQ_FERRY, strlen(GET_REQ_FERRY), 0);
    if (ret < 0) {
        printk("HTTP send failed: %d\n", ret);
        return;
    }
    
    while (true) {
        int rx = zsock_recv(sock, packet_buf, packet_buf_size - 1, 0);
        if (rx > 0) {
            packet_buf[rx] = '\0';
            printk("%s", packet_buf);
        } else if (rx == 0) {
            /* peer closed cleanly */
            break;
        } else {
            printk("HTTP recv error: %d\n", rx);
            break;
        }
    }
    zsock_close(sock);
    printk("\n");
}


void process_received_packet(char* packet_buf) {
    double lat, lon;
    int status;
    if (sscanf(packet_buf,
           "{\"status\":%d,\"lat\":%lf,\"lon\":%lf",
           &status, &lat, &lon) == 3) {
        printk("parsed: status=%d, lat=%f, lon=%f\n", status, lat, lon);
    } else {
        printk("JSON parse failed\n");
    }
}


void send_post(char* endpoint, char* body) {
    char msg[256];

    int len = snprintf(msg, sizeof(msg), 
        "POST /%s HTTP/1.1\r\n"
        "Host: " SERVER_IP "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        endpoint,
        (unsigned)strlen(body),
        body);

    if (len < 0 || len >= sizeof(msg)) {
        printk("Can't format packet\n");
        return;
    }

    int sock = connect_to_ip();
    int ret = zsock_send(sock, msg, len, 0);

    if (ret < 0) {
        printk("HTTP POST send failed: %d\n", ret);
        return;
    }
    zsock_close(sock);
}

void send_volume_change(bool up) {
    char* body;
    if (up) {
        body = "{\"direction\":\"up\"}";
    } else {
        body = "{\"direction\":\"down\"}";
    }

    send_post("volume", body);
}