#ifndef MODEM_H
#define MODEM_H

#include <stdbool.h>

void modem_init(void);
bool modem_connect_network(void);
bool modem_http_post_json(const char *url, const char *api_key, const char *json_payload);
void modem_shutdown_for_sleep(void);

#endif
