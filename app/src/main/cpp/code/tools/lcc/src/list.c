/* Third-party component; the applicable licence is recorded in THIRDPARTY.md. */
#include "c.h"


static List freenodes;		

List append(void *x, List list) {
	List new;

	if ((new = freenodes) != NULL)
		freenodes = freenodes->link;
	else
		NEW(new, PERM);
	if (list) {
		new->link = list->link;
		list->link = new;
	} else
		new->link = new;
	new->x = x;
	return new;
}

int length(List list) {
	int n = 0;

	if (list) {
		List lp = list;
		do
			n++;
		while ((lp = lp->link) != list);
	}
	return n;
}

void *ltov(List *list, unsigned arena) {
	int i = 0;
	void **array = newarray(length(*list) + 1, sizeof array[0], arena);

	if (*list) {
		List lp = *list;
		do {
			lp = lp->link;
			array[i++] = lp->x;
		} while (lp != *list);
#ifndef PURIFY
		lp = (*list)->link;
		(*list)->link = freenodes;
		freenodes = lp;
#endif
	}
	*list = NULL;
	array[i] = NULL;
	return array;
}
