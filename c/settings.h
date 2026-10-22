/* ~/.clawd-room/settings.conf: lines `key = value`, `#` starts a comment. Read when the program starts and again when the file changes.
 * Keys so far: gate_theme (a folder of anims/gates/: simple, construct, ...). A flag or an environment variable beats the file. */
#ifndef SETTINGS_H
#define SETTINGS_H

void settings_init(const char *path);        /* the file ("" or NULL: none); read now */
int settings_poll(void);                     /* read again when the file changed (looks at most twice a second): 1 when it did */
const char *settings_get(const char *key);   /* the value, or NULL */
void settings_apply(const char *gate_flag);  /* the gate theme: the flag (--gate-theme), else CLAWD_GATE_THEME, else the file, else simple */
const char *settings_path(void);
#endif
