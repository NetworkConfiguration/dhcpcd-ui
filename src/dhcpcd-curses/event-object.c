/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "event-object.h"

/* Track event objects by parameter */
EVENT_OBJECT *
event_object_find(EVENT_OBJECTS *events, void *object)
{
	EVENT_OBJECT *eo;

	assert(events);
	assert(object);
	TAILQ_FOREACH(eo, events, next) {
		if (eo->object == object)
			return eo;
	}
	errno = ESRCH;
	return NULL;
}

EVENT_OBJECT *
event_object_add(EVENT_OBJECTS *events, struct event *event,
    struct timeval *tv, void *object)
{
	EVENT_OBJECT *eo;

	assert(events);
	assert(event);
	assert(object);
	eo = malloc(sizeof(*eo));
	if (eo == NULL || event_add(event, tv) == -1)
		return NULL;
	eo->event = event;
	eo->object = object;
	TAILQ_INSERT_TAIL(events, eo, next);
	return eo;
}

void
event_object_delete(EVENT_OBJECTS *events, EVENT_OBJECT *eo)
{

	assert(events);
	assert(eo);
	TAILQ_REMOVE(events, eo, next);
	event_del(eo->event);
	event_free(eo->event);
	free(eo);
}

void
event_object_find_delete(EVENT_OBJECTS *events, void *object)
{
	EVENT_OBJECT *eo;

	if ((eo = event_object_find(events, object)) != NULL)
		event_object_delete(events, eo);
}

EVENT_OBJECTS *
event_object_new(void)
{
	EVENT_OBJECTS *events;

	if ((events = malloc(sizeof(*events))) == NULL)
		return NULL;

	TAILQ_INIT(events);
	return events;
}

void
event_object_free(EVENT_OBJECTS *events)
{
	EVENT_OBJECT *eo;

	if (events) {
		while ((eo = TAILQ_FIRST(events)) != NULL) {
			event_object_delete(events, eo);
		}
		free(events);
	}
}
