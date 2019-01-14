#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"

// for performance, realloc every BLOCK time
#define BLOCK 100

Array *array_new(int element_size)
{
	Array *arr;

	arr = calloc(1, sizeof(Array));
	arr->element_size = element_size;

	return arr;
}

void array_add(Array *a, void *v)
{
	int size = (a->length + 1) * a->element_size;
	if (a->allocated < size) {
		a->data = realloc(a->data, size * BLOCK);
		a->allocated = size * BLOCK;
	}
	memcpy(a->data + a->length * a->element_size, v, a->element_size);
	a->length++;
}

void *array_get(Array *a, int num)
{
	return a->data + (num *a->element_size);
}

void array_free(Array *a)
{
	free(a->data);
	free(a);
}

// remove index (start from 0)
// do not resize allocated memory
void array_remove(Array *a, int index)
{
	memmove(a->data + index * a->element_size,
		a->data + (index + 1) * a->element_size,
		(a->length - index - 1) * a->element_size);

	a->length--;
}
