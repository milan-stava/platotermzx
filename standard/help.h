#ifndef STANDARD_HELP_H
#define STANDARD_HELP_H
void help_clear(void);
void help_prompt_input(const char *text);
void help_prompt_key_display(const char *text, char extend);
void help_keys(void);
int help_matches(char *entry, const char *match);
void help_run(void);
#endif
