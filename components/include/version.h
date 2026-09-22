/* Firmware identity, as reported by AT+VER? and the +READY notification.
 *
 * The ESP32-S3 keys its compatibility checks off this string, so bump it in
 * the same change that alters docs/at-protocol.md. */
#ifndef X3_VERSION_H
#define X3_VERSION_H

#define X3_FW_NAME     "x3-switch"
#define X3_FW_VERSION  "0.1.0"

#endif /* X3_VERSION_H */
