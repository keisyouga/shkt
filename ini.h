#ifndef _INI_H
#define _INI_H

typedef struct _Ini {
	char *section;
	char *key;
	char *data;
	struct _Ini *next;
} Ini;

Ini *Ini_set(Ini *Ini, const char *section, const char *key, const char *data);
const char *Ini_get(Ini *Ini, const char *section, const char *key);
Ini *Ini_readfile(const char *filename);
void Ini_printall(Ini *Ini);
void Ini_removeall(Ini *ini);

#endif /* _INI_H */
