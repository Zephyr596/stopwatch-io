#pragma once
#define SW_WIFI_SSID ""
#define SW_WIFI_PASSWORD ""
// For Codex status over the LAN, set a shared 64-character hexadecimal key.
#define SW_CODEX_LAN_KEY ""
#define SW_API_URL ""
#define SW_API_TOKEN ""
// PEM trust root for the HTTPS endpoint; never use setInsecure().
#define SW_API_CA ""
// Only for a local, token-free fixture. Production must use HTTPS.
#define SW_ALLOW_HTTP_TEST false

#define SW_DEFAULT_API false
