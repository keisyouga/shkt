#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ini.h"

#if DBG_PRINT_ENABLED
#define DBG_TEST 1
#else
#define DBG_TEST 0
#endif

#define DBG_PRINT(...) do { if (DBG_TEST) { fprintf(stderr, "%s:%d:%s():", __FILE__, __LINE__, __func__); fprintf(stderr, __VA_ARGS__); }} while(0)


// line start with '#' or ';' is comment
static int
IsComment(char *s)
{
	return (s && ((s[0] == '#') || s[0] == ';'));
}


void
Inif_printall(Ini *ini)
{
	DBG_PRINT("%p\n", ini);

	while (ini) {
		printf("%p: ", ini);
		printf("[%s] ", ini->section);
		printf("%s=", ini->key);
		printf("%s\n", ini->data);

		ini = ini->next;
	}
}

static Ini *
Ini_find(Ini *ini, const char *section, const char *key)
{
	for (; ini; ini = ini->next) {
		if ((strcmp(ini->section, section) == 0) && (strcmp(ini->key, key) == 0)) {
			return ini;
		}
	}
	return NULL;
}

// malloc and strcpy; section, key, data
static void
Ini_set2(Ini *ini, const char *section, const char *key, const char *data)
{
	DBG_PRINT("%p\n", ini);
	int len_sec = strlen(section);
	int len_key = strlen(key);
	int len_data = strlen(data);

	if (!ini->section) {
		ini->section = malloc(len_sec + 1);
		strcpy(ini->section, section);
	}
	if (!ini->key) {
		ini->key = malloc(len_key + 1);
		strcpy(ini->key, key);
	}
	if (ini->data) {
		free(ini->data);
	}

	ini->data = malloc(len_data + 1);
	strcpy(ini->data, data);
}

// if ini == NULL, create Ini and return it
// otherwise, create at last, and return created one
static Ini *
Ini_newini(Ini *ini)
{
	if (!ini) {
		ini = calloc(1, sizeof(Ini));
	} else {
		while (ini->next) {
			ini = ini->next;
		}
		ini->next = calloc(1, sizeof(Ini));
		ini = ini->next;
	}
	return ini;
}

Ini *
Ini_set(Ini *ini, const char *section, const char *key, const char *data)
{
	DBG_PRINT("%p, %s, %s, %s\n", ini, section, key, data);
	// initial call
	if (!ini) {
		ini = Ini_newini(NULL);
		Ini_set2(ini, section, key, data);
		return ini;
	}
	// if has section alreadly, use it, othrerwise create one
	Ini *iniound = Ini_find(ini, section, key);
	if (!iniound) {
		iniound = Ini_newini(ini);
	}
	Ini_set2(iniound, section, key, data);

	// return first one
	return ini;
}

const char *
Ini_get(Ini *ini, const char *section, const char *key)
{
	ini = Ini_find(ini, section, key);
	if (ini) {
		return ini->data;
	}
	return NULL;
}

void
Ini_removeall(Ini *ini)
{
	Ini *nxt;
	while (ini) {
		nxt = ini->next;
		free(ini->section);
		free(ini->key);
		free(ini->data);
		free(ini);
		ini = nxt;
	}
}

// if str is section, that start with '[', end with ']',
// copy section-name to sec and return 1
// if str is not section, return 0
static int
IsSection(char *str, char *sec)
{
	if (!str) { return 0; }
	if (str[0] == '[') {
		char *p = strchr(&str[1], ']');
		if (p) {
			*p = '\0';
			strcpy(sec, &str[1]);
			return 1;
		}
	}
	return 0;
}

Ini *
Ini_readfile(const char *filename)
{
	DBG_PRINT("%s\n", filename);
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "can not open: %s\n", filename);
		return NULL;
	}

	Ini *ini = NULL;

	char line[256] = "";
	char section[256] = "";
	char *key = NULL;
	char *data = NULL;
	while (fgets(line, sizeof(line), fp)) {
		if (IsComment(line)) { continue; }

		if (IsSection(line, section) != 1) {
			// not section
			key = strtok(line, "=");
			data = strtok(NULL, "\n");
			if (key && data && (strlen(key) > 0) && (strlen(data) > 0)) {
				ini = Ini_set(ini, section, key, data);
			}
		}
	}
	fclose(fp);
	return ini;
}
