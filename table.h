#ifndef TABLE_H
#define TABLE_H

#include "array.h"

#define STROKE_MAX_CHAR 16
#define RESULT_MAX_CHAR 16
#define STROKECHAR_MAX 256

typedef struct {
	char stroke[STROKE_MAX_CHAR];
	char result[RESULT_MAX_CHAR];
} StrokeResult;

typedef struct {
	Array *arr;		// array of StrokeResult
	char strokeChar[STROKECHAR_MAX]; /* character used for stroke */
} Table;

int CountStrokeResult(Table *l);
int IsCharStroke(Table *l, const char ch);
void AddStrokeChar(Table *l, const char *s);
int CompareStroke(const char *s1, const char *s2);
Array *GetStrokeResult(Table *list, char *stroke);
void LoadTable(Table *srlist, const char filename[]);
void UnloadTable(Table *t);

#endif
