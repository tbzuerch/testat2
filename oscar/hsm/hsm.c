/*	Oscar, a hardware abstraction framework for the LeanXcam and IndXcam.
	Copyright (C) 2008 Supercomputing Systems AG
	
	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*! @file
 * @brief HSM implementation for host and target.
 * 
	************************************************************************/

#include <string.h>
#include <stdio.h>

#include "oscar.h"

struct OscModule OscModule_hsm = {
	.name = "hsm",
	.dependencies = {
		&OscModule_log,
		NULL // To end the flexible array.
	}
};

static Msg const startMsg = { START_EVT };
static Msg const entryMsg = { ENTRY_EVT };
static Msg const exitMsg  = { EXIT_EVT };
#define MAX_STATE_NESTING 8

/* .........................................................................*/
void StateCtor(State *me, char const *name, State *super, EvtHndlr hndlr) {
	me->name  = name;
	me->super = super;
	me->hndlr = hndlr;
}

/* .........................................................................*/
void HsmCtor(Hsm *me, char const *name, EvtHndlr topHndlr) {
	StateCtor(&me->top, "top", 0, topHndlr);
	me->name = name;
}

/* .........................................................................*/
void HsmOnStart(Hsm *me) {
	me->curr = &me->top;
	me->next = 0;
	StateOnEvent(me->curr, me, &entryMsg);
	while (StateOnEvent(me->curr, me, &startMsg), me->next) {
		State *entryPath[MAX_STATE_NESTING];
		register State **trace = entryPath;
		register State *s;
		*trace = 0;
		for (s = me->next; s != me->curr; s = s->super) {
			*(++trace) = s;                         /* trace path to target */
		}
		/*while (s = *trace--) {        */
		while (0 != (s = *trace--)) {          /* retrace entry from source */
			StateOnEvent(s, me, &entryMsg);
		}
		me->curr = me->next;
		me->next = 0;
	}
}

/* .........................................................................*/
void HsmOnEvent(Hsm *me, Msg const *msg) {
	State *entryPath[MAX_STATE_NESTING];
	register State **trace;
	register State *s;
	for (s = me->curr; s; s = s->super) {
		me->source = s;                 /* level of outermost event handler */
		msg = StateOnEvent(s, me, msg);
		if (msg == 0) {
			if (me->next) {                      /* state transition taken? */
				trace = entryPath;
				*trace = 0;
				for (s = me->next; s != me->curr; s = s->super) {
					*(++trace) = s;                 /* trace path to target */
				}
				/*while (s = *trace--) {      */
				while (0 != (s = *trace--)) {     /* retrace entry from LCA */
					StateOnEvent(s, me, &entryMsg);
				}
				me->curr = me->next;
				me->next = 0;
				while (StateOnEvent(me->curr, me, &startMsg), me->next) {
					trace = entryPath;
					*trace = 0;
					for (s = me->next; s != me->curr; s = s->super) {
						*(++trace) = s;            /* record path to target */
					}
					/*while (s = *trace--) {   */
					while (0!= (s = *trace--)) {        /* retrace the entry */
						StateOnEvent(s, me, &entryMsg);
					}
					me->curr = me->next;
					me->next = 0;
				}
			}
			break;                                       /* event processed */
		}
	}
}

/* .........................................................................*/
void HsmExit_(Hsm *me, unsigned char toLca) {
	register State *s = me->curr;
	while (s != me->source) {
		StateOnEvent(s, me, &exitMsg);
		s = s->super;
	}
	while (toLca--) {
		StateOnEvent(s, me, &exitMsg);
		s = s->super;
	}
	me->curr = s;
}

/* .........................................................................*/
unsigned char HsmToLCA_(Hsm *me, State *target) {
	State *s, *t;
	unsigned char toLca = 0;
	if (me->source == target) {
		return 1;
	}
	for (s = me->source; s; ++toLca, s = s->super) {
		for (t = target; t; t = t->super) {
			if (s == t) {
				return toLca;
			}
		}
	}
	return 0;
}

