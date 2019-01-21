#include "table.h"

#include "table.h"
#include "array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int CountStrokeResult(Table *l)
{
	return l->arr->length;
}

// ch is used in stroketable?
int IsCharStroke(Table *l, const char ch)
{
	return l->strokeChar[(int)ch];
}

void AddStrokeChar(Table *l, const char *s)
{
	while (*s) {
		l->strokeChar[(int)*s] = 1;
		s++;
	}
}

// return 0 if not matched
// pattern may contain * or ?
int CompareStroke(const char *stroke, const char *pattern)
{
	while (1) {
		if (!*stroke) {
			while (*pattern == '*') { pattern++; }
			return (*pattern == '\0');
		}
		// not matched
		if (!*pattern) { return 0; }

		if (*pattern == '*') {
			// match test for each character
			return (CompareStroke(stroke, pattern + 1) ||
					CompareStroke(stroke + 1, pattern));
		}

		if ((*stroke == *pattern) || (*pattern == '?')) {
			stroke++;
			pattern++;
			continue;
		}
		// not matched
		return 0;
	}
}

// caller must array_free return array
Array *GetStrokeResult(Table *list, char *stroke)
{
	// if no '*' in stroke, append this
	char buf[STROKE_MAX_CHAR + 1];
	strcpy(buf, stroke);
	if (!strchr(buf, '*')) {
		strcat(buf, "*");
	}
	Array *arr2 = array_new(sizeof(StrokeResult));
	for (int i = 0; i < list->arr->length; i++) {
		StrokeResult *sr = array_get(list->arr, i);
		//fprintf(stderr, "compare: %s, %s\n", sr->stroke, stroke);
		if (CompareStroke(sr->stroke, buf)) {
			array_add(arr2, sr);
		}
	}
	return arr2;
}

// read from table-file
void LoadTable(Table *srlist, const char filename[])
{
	srlist->arr = array_new(sizeof(StrokeResult));

	StrokeResult sr;

	FILE *fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "can not open %s\n", filename);
	}
	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		int n = sscanf(line, "%15s %15s", sr.stroke, sr.result);
		if (n == 2) {
			//fprintf(stderr, "%s, %s\n", sr.stroke, sr.result);
			array_add(srlist->arr, &sr);
			AddStrokeChar(srlist, sr.stroke);
		}
	}
	fclose(fp);
}

// free and reset allocated table
void UnloadTable(Table *t)
{
	if (t && t->arr) {
		array_free(t->arr);
	}
	t->arr = NULL;
	memset(t->strokeChar, 0, STROKECHAR_MAX);
}
