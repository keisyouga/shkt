#ifndef ARRAY_H
#define ARRAY_H

typedef struct {
/* public */
	void *data;
	int length;
/* private */
	int allocated;
	int element_size;
} Array;

Array *array_new(int element_size);
void array_add(Array *a, void *v);
// suppress warning
//#define array_add(a,v) array_add(a,(char*)v)
void *array_get(Array *a, int num);
void array_free(Array *a);
void array_remove(Array *a, int index);

#endif
