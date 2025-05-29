#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>

struct mqtt_t {
  uint8_t rx_buff[1024];
};
extern struct k_msgq mqtt_msgq;
extern struct mqtt_t;

#endif // MQTT_H_