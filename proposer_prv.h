/********************************************************************

  Copyright 2012 Konstantin Olkhovskiy <lupus@oxnull.net>

  This file is part of Mersenne.

  Mersenne is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  Mersenne is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Mersenne.  If not, see <http://www.gnu.org/licenses/>.

 ********************************************************************/

#ifndef _PROPOSER_PRV_H_
#define _PROPOSER_PRV_H_

#include <ev.h>
#include <bitmask.h>

#include <context.h>
#include <me_protocol.h>

#define INSTANCE_WINDOW 10
#define MAX_PROC 100
#define TO1 0.1
#define TO2 0.1

enum instance_state {
    IS_EMPTY = 0,
    IS_P1_PENDING,
    IS_P1_READY_NO_VALUE,
    IS_P1_READY_WITH_VALUE,
    IS_P2_PENDING,
    IS_CLOSED,
    IS_DELIVERED,
    IS_MAX
};

struct pro_instance {
	uint64_t iid;
	uint64_t b;
	enum instance_state state;
	struct {
		char v[ME_MAX_XDR_MESSAGE_LEN];
		uint32_t v_size;
		uint64_t vb;
		struct bitmask *acks;
	} p1;
	struct {
		char v[ME_MAX_XDR_MESSAGE_LEN];
		uint32_t v_size;
	} p2;
	int client_value;
	ev_timer timer;

	struct me_context *mctx;
};

enum instance_event_type {
	IE_I = 0,
	IE_S,
	IE_TO,
	IE_P,
	IE_R0,
	IE_R1,
	IE_NV,
	IE_A,
	IE_E,
	IE_C,
	IE_D,
};

struct ie_base {
	enum instance_event_type type;
};

struct ie_p {
	struct me_peer *from;
	struct me_paxos_promise_data *data;
	struct ie_base b;
};

typedef void is_func_t(ME_P_ struct pro_instance *instance, struct ie_base *base);

void do_is_empty(ME_P_ struct pro_instance *instance, struct ie_base *base);
void do_is_p1_pending(ME_P_ struct pro_instance *instance, struct ie_base *base);
void do_is_p1_ready_no_value(ME_P_ struct pro_instance *instance, struct ie_base *base);
void do_is_p1_ready_with_value(ME_P_ struct pro_instance *instance, struct ie_base *base);
void do_is_p2_pending(ME_P_ struct pro_instance *instance, struct ie_base *base);
void do_is_closed(ME_P_ struct pro_instance *instance, struct ie_base *base);
void do_is_delivered(ME_P_ struct pro_instance *instance, struct ie_base *base);

#endif
