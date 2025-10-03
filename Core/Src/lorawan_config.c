#include "lorawan_config.h"

#include <string.h>

static const char g_lorawan_dev_eui[] = "0025CA00000056F7";
static const char g_lorawan_join_eui[] = "0025CA00000055F7";

static const char *const g_lorawan_setup_cmds[] = {
    "ATS 602=1\r\n", // Activation Mode OTAA (0 = ABP, 1 = OTAA)
    "ATS 603=0\r\n", // Class A
    "ATS 604=1\r\n", // Confirmed uplinks
    "ATS 605=3\r\n", // Retry count
    "ATS 611=9\r\n"  // Region AS923-1
};

static const lorawan_credentials_t g_lorawan_credentials = {
    .dev_eui = g_lorawan_dev_eui,
    .join_eui = g_lorawan_join_eui};

const lorawan_credentials_t *lorawan_config_get_credentials(void)
{
  return &g_lorawan_credentials;
}

void lorawan_config_build_app_key(char *buffer, size_t buffer_len)
{
  if (!buffer || buffer_len == 0u)
  {
    return;
  }

  buffer[0] = '\0';

  if (buffer_len <= strlen(g_lorawan_dev_eui) + strlen(g_lorawan_join_eui))
  {
    return;
  }

  strcpy(buffer, g_lorawan_dev_eui);
  strcat(buffer, g_lorawan_join_eui);
}

size_t lorawan_config_get_provisioning_command_count(void)
{
  return sizeof(g_lorawan_setup_cmds) / sizeof(g_lorawan_setup_cmds[0]);
}

const char *lorawan_config_get_provisioning_command(size_t index)
{
  size_t count = lorawan_config_get_provisioning_command_count();
  if (index >= count)
  {
    return NULL;
  }
  return g_lorawan_setup_cmds[index];
}
