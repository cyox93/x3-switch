/* Internal line-output helpers shared by the AT command handlers. */
#ifndef X3_AT_IO_H
#define X3_AT_IO_H

/* Every line this firmware sends ends with CRLF; these add it. */
void at_send_line(const char *text);
void at_sendf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void at_reply_ok(void);
void at_reply_error(void);

#endif /* X3_AT_IO_H */
