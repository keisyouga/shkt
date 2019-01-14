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

// compare method
// return 0 if matched
int CompareStroke(const char *s1, const char *s2)
{
	while (1) {
		// matched
		if (!*s2) { return 0; }
		// do not matched
		if (!*s1) { return 1; }

		// match any character
		if (*s2 == '?') {
			s1++;
			s2++;
			continue;
		}

		// do not matched
		if (*s1 != *s2) { return 1; }

		s1++;
		s2++;
	}

	return 0;
}

// caller must array_free return array
Array *GetStrokeResult(Table *list, char *stroke)
{
	Array *arr2 = array_new(sizeof(StrokeResult));
	for (int i = 0; i < list->arr->length; i++) {
		StrokeResult *sr = array_get(list->arr, i);
		//fprintf(stderr, "compare: %s, %s\n", sr->stroke, stroke);
		if (CompareStroke(sr->stroke, stroke) == 0) {
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
