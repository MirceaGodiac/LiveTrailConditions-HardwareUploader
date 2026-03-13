#ifndef SENSORS_H
#define SENSORS_H

void sensors_init(void);
int sensors_read_moisture_percent(void);
int sensors_read_battery_percent(void);

#endif
