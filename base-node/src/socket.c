#include <zephyr/net/socket.h>
#include <zephyr/sys/printk.h>

#include "socket.h"

#include "auth.h"


int connect_to_ip(void) {
    // create socket
    int sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printk("Socket creation failed: %d\n", sock);
        return -1;
    }
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    zsock_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

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