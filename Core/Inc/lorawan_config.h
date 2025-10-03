#ifndef LORAWAN_CONFIG_H
#define LORAWAN_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  const char *dev_eui;
  const char *join_eui;
} lorawan_credentials_t;

const lorawan_credentials_t *lorawan_config_get_credentials(void);
void lorawan_config_build_app_key(char *buffer, size_t buffer_len);
size_t lorawan_config_get_provisioning_command_count(void);
const char *lorawan_config_get_provisioning_command(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_CONFIG_H */
