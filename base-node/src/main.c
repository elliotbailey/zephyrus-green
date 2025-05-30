#include <stddef.h>
#include <stdint.h>
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
#include "zephyr/drivers/usb/usb_dc.h"
#include "zephyr/sys/util.h"
#include <stdbool.h>
#include "ferry.h"
#include <zephyr/fs/fs.h>
#include <zephyr/device.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_msc.h>
#include <zephyr/storage/disk_access.h>
#include <ff.h>

#define MKFS_FS_TYPE FS_FATFS
#define MKFS_DEV_ID "NAND:"
#define MKFS_FLAGS 0

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);
static FATFS fat_fs;


static struct fs_mount_t fat_storage_mnt = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
    .storage_dev = (void *)"NAND",
    .mnt_point = "/NAND:",
    .flags = MKFS_FLAGS
};


void process_received_packet(char* packet_buf);
void handle_volume_change(void);
void send_volume(int volume);
void receive_ferry_packet(char* packet_buf, size_t packet_buf_size);
void send_arriving(int mmsi);
void send_departing(int mmsi);
void ferry_arriving_action(int mmsi);
void ferry_departing_action(int mmsi);
void fs_init(void);
void mount_fs();
static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param);
void sync_rtc_with_server(void);
void log_ferry_event(int mmsi, bool arriving);

int current_volume = 100;

struct coordinates UQ_FERRY_TERMINAL = {-27.496776268829635, 153.0195395998301};

/* HTTP get for ferry data */
static const char GET_REQ_FERRY[] =
    "GET /ferry HTTP/1.1\r\n"
    "Host: " SERVER_IP "\r\n"
    "Connection: close\r\n"
    "\r\n";

/* HTTP get for volume change data */
static const char GET_VOL_CHANGE[] =
    "GET /volumechange HTTP/1.1\r\n"
    "Host: " SERVER_IP "\r\n"
    "Connection: close\r\n"
    "\r\n";

/* HTTP get for RTC syncing */
static const char GET_RTC[] =
    "GET /rtc HTTP/1.1\r\n"
    "Host: " SERVER_IP "\r\n"
    "Connection: close\r\n"
    "\r\n";

int main(void) {

    mount_fs();
    usb_enable(usb_status_cb);

    init_rtc();
    setup_wifi();

    sync_rtc_with_server();

    while(1) {
        char packet_buf[512];
        receive_ferry_packet(packet_buf, sizeof(packet_buf));

        process_received_packet(packet_buf);

        k_sleep(K_MSEC(100));
        handle_volume_change();
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
    int closeret = zsock_close(sock);
    if (closeret < 0) {
        printk("Socket close error rx ferry: %d\n", closeret);
    }
    printk("\n");
}


void handle_volume_change(void) {
    char packet_buf[128];
    int sock = connect_to_ip();
    int ret = zsock_send(sock, GET_VOL_CHANGE, strlen(GET_VOL_CHANGE), 0);
    if (ret < 0) {
        printk("HTTP send failed: %d\n", ret);
        return;
    }
    
    while (true) {
        int rx = zsock_recv(sock, packet_buf, sizeof(packet_buf) - 1, 0);
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
    int closeret = zsock_close(sock);
    if (closeret < 0) {
        printk("Socket close error handle volume change: %d\n", closeret);
    }
    printk("\n");

    // process packet
    int status, change;
    if (sscanf(packet_buf, "{\"status\":%d,\"change\":%d", &status, &change) == 2) {
        printk("parsed: status=%d, change=%dn", status, change);

        if (change == -1 || status == 404) return;   // no change required

        if (change == 0) {
            // decrease volume
            current_volume = MAX(0, current_volume - 10);
        } else {
            // increase volume
            current_volume = MIN(255, current_volume + 10);
        }

        // send change back
        send_volume(current_volume);
    } else {
        printk("JSON parse failed for volume change\n");
    }
}

void sync_rtc_with_server(void) {
    char packet_buf[128];
    int sock = connect_to_ip();
    int ret = zsock_send(sock, GET_RTC, strlen(GET_RTC), 0);
    if (ret < 0) {
        printk("HTTP send failed: %d\n", ret);
        return;
    }
    
    while (true) {
        int rx = zsock_recv(sock, packet_buf, sizeof(packet_buf) - 1, 0);
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
    int closeret = zsock_close(sock);
    if (closeret < 0) {
        printk("Socket close error handle volume change: %d\n", closeret);
    }
    printk("\n");

    // process packet
    int year, mon, day, hour, min, sec;
    if (sscanf(packet_buf, "{\"year\":%d,\"mon\":%d,\"day\":%d,\"hour\":%d,\"min\":%d,\"sec\":%d", &year, &mon, &day, &hour, &min, &sec) == 6) {
        printk("parsed time: %d %d %d %d %d %d\n", year, mon, day, hour, min, sec);;

        struct rtc_time set_time = {
            .tm_year = year - 1900,  // Year since 1900
            .tm_mon = mon - 1,         // Month (0-based)
            .tm_mday = day,           // Day
            .tm_hour = hour,           // Hour
            .tm_min = min,            // Minute
            .tm_sec = sec              // Second
        };
        set_rtc_time(&set_time);

    } else {
        printk("JSON parse failed for RTC sync\n");
    }
}


void process_received_packet(char* packet_buf) {
    double lat, lon;
    int status;
    int32_t mmsi;
    if (sscanf(packet_buf,
           "{\"status\":%d,\"mmsi\":%d,\"lat\":%lf,\"lon\":%lf",
           &status, &mmsi, &lat, &lon) == 4) {
        printk("parsed: status=%d, mmsi=%d, lat=%f, lon=%f\n", status, mmsi, lat, lon);

        if (status == 404) return;
        
        struct coordinates ferryCoords = {lat, lon};
        // attempt to get ferry
        struct ferry *existing_ferry = get_ferry_by_mmsi(mmsi);
        if (existing_ferry == NULL) {
            // add new ferry
            double dist = distance_between_coords(ferryCoords, UQ_FERRY_TERMINAL);
            bool near = dist < 100;
            struct ferry new_ferry = {mmsi, near, ferryCoords};
            track_new_ferry(new_ferry);
            if (near) {
                // new ferry is near terminal
                ferry_arriving_action(mmsi);
            }
        } else{
            // update existing ferry coords
            existing_ferry->coords.lat = lat;
            existing_ferry->coords.lon = lon;

            bool near = is_ferry_near_coords(*existing_ferry, UQ_FERRY_TERMINAL);
            if (near && !existing_ferry->near_terminal) {
                // This means that an existing ferry has now moved close to a terminal and it wasn't before
                ferry_arriving_action(mmsi);
            } else if (!near && existing_ferry->near_terminal) {
                // existing ferry has moved away from a terminal
                ferry_departing_action(mmsi);
            }
            existing_ferry->near_terminal = near;
        }
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
    int closeret = zsock_close(sock);
    if (closeret < 0) {
        printk("Socket close error send post: %d\n", closeret);
    }
}

void send_volume(int volume) {
    char body[64];
    snprintf(body, sizeof(body), "{\"volume\":\"%d\"}", volume);

    send_post("volume", body);
}


void send_arriving(int mmsi) {
    char body[64];
    snprintf(body, sizeof(body), "{\"mmsi\":\"%d\"}", mmsi);

    send_post("arriving", body);
}


void send_departing(int mmsi) {
    char body[64];
    snprintf(body, sizeof(body), "{\"mmsi\":\"%d\"}", mmsi);

    send_post("departing", body);
}


void ferry_arriving_action(int mmsi) {
    printk("Ferry %d has arrived at terminal\n", mmsi);
    // send update to webserver
    send_arriving(mmsi);
    log_ferry_event(mmsi, true);
}

void ferry_departing_action(int mmsi) {
    printk("Ferry %d has left terminal\n", mmsi);
    // send update to webserver
    send_departing(mmsi);
    log_ferry_event(mmsi, false);
}


void fs_init(void) {
    int rc = fs_mkfs(MKFS_FS_TYPE, (uintptr_t)MKFS_DEV_ID, NULL, MKFS_FLAGS);
    if (rc != 0) {
        printk("FS MKFS Failed: %d\n", rc);
    } else {
        printk("FS MKFS Success\n");
    }
}

/**
 * Mounts the file system
 */
void mount_fs() {

    int rc;
    rc = fs_mount(&fat_storage_mnt);

    if (rc != 0) {
        printk("Failed to mount FS: %d\n", rc);
    }

}


static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
    printk("USB status: %d\n", status);
    switch (status) {
    case USB_DC_CONFIGURED:
        printk("USB CONNECTED\n");
        break;

    case USB_DC_DISCONNECTED:
    case USB_DC_SUSPEND:
        printk("USB DISCONNECTED/SUSPENDED\n");
        break;

    default:
        break;
    }
}


void log_ferry_event(int mmsi, bool arriving) {
    // generate line to write
    char entry_buf[128];
    char formatted_time[32];
    format_rtc_time(formatted_time, sizeof(formatted_time));

    char *status;
    if (arriving) {
        status = "ARRIVING at";
    } else {
        status = "DEPARTING from";
    }

    snprintf(entry_buf, sizeof(entry_buf), "%s: MMSI %d is %s terminal.", formatted_time, mmsi, status);

    int rc;
    struct fs_file_t file;
    fs_file_t_init(&file);
    // open file in append mode
    rc = fs_open(&file, "/NAND:/FERRYLOG.txt", FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
    fs_write(&file, entry_buf, strlen(entry_buf));
    fs_write(&file, "\n", 1);
    fs_close(&file);
}