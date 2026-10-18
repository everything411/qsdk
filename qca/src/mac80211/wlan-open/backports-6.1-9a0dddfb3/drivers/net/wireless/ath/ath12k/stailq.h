/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef STAILQ_H
#define STAILQ_H

#define STAILQ_HEAD(name, type) \
struct name { \
	struct type *stqh_first; \
	struct type **stqh_last; \
}

#define STAILQ_ENTRY(type) \
struct { \
	struct type *stqe_next; \
}

#define STAILQ_INIT(head) do { \
	(head)->stqh_first = NULL; \
	(head)->stqh_last = &(head)->stqh_first; \
} while (0)

#define STAILQ_INSERT_TAIL(head, elm, field) do { \
	(elm)->field.stqe_next = NULL; \
	*(head)->stqh_last = (elm); \
	(head)->stqh_last = &(elm)->field.stqe_next; \
} while (0)

#define STAILQ_REMOVE_HEAD(head, field) do { \
	(head)->stqh_first = (head)->stqh_first->field.stqe_next; \
	if ((head)->stqh_first == NULL) \
		(head)->stqh_last = &(head)->stqh_first; \
} while (0)

#define STAILQ_FIRST(head) ((head)->stqh_first)
#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define STAILQ_FOREACH(var, head, field) \
	for ((var) = STAILQ_FIRST((head)); (var); (var) = STAILQ_NEXT((var), field))

#define STAILQ_FOREACH_SAFE(var, head, field, tvar)            \
	for ((var) = STAILQ_FIRST((head));                \
			(var) && ((tvar) = STAILQ_NEXT((var), field), 1);        \
			(var) = (tvar))

#define STAILQ_REMOVE(head, elm, type, field) do {                      \
	if (STAILQ_FIRST((head)) == (elm)) {                            \
		STAILQ_REMOVE_HEAD(head, field);                        \
	} else {                                                        \
		struct type *curelm = STAILQ_FIRST((head));             \
		while (curelm && (STAILQ_NEXT(curelm, field) != (elm))) \
			curelm = STAILQ_NEXT(curelm, field);            \
		if (curelm) {                                           \
			struct type *nxtelm;                           \
			nxtelm = STAILQ_NEXT(STAILQ_NEXT(curelm, field), field); \
			STAILQ_NEXT(curelm, field) = nxtelm;           \
			if (nxtelm == NULL)                            \
				(head)->stqh_last = &STAILQ_NEXT(curelm, field); \
		}                                                       \
	}                                                               \
} while (0)
#endif
