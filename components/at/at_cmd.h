/* Internal: command table and dispatch, split from the line assembly so the
 * parser can be read without the handlers and vice versa. */
#ifndef X3_AT_CMD_H
#define X3_AT_CMD_H

/* `line` is trimmed, NUL-terminated and mutable — handlers chop it in place
 * on the argument separators. Always emits exactly one OK or ERROR. */
void at_cmd_dispatch(char *line);

#endif /* X3_AT_CMD_H */
