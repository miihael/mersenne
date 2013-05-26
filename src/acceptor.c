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
#include <assert.h>
#include <err.h>
#include <dlfcn.h>
#include <wordexp.h>

#include <mersenne/acceptor.h>
#include <mersenne/paxos.h>
#include <mersenne/context.h>
#include <mersenne/message.h>
#include <mersenne/fiber_args.h>
#include <mersenne/util.h>
#include <mersenne/me_protocol.strenum.h>
#include <mersenne/sharedmem.h>

static void send_promise(ME_P_ struct acc_instance_record *r, struct me_peer
		*to)
{
	struct me_message msg;
	struct me_paxos_msg_data *data = &msg.me_message_u.paxos_message.data;
	msg.super_type = ME_PAXOS;
	data->type = ME_PAXOS_PROMISE;
	data->me_paxos_msg_data_u.promise.i = r->iid;
	data->me_paxos_msg_data_u.promise.b = r->b;
	data->me_paxos_msg_data_u.promise.v = r->v;
	data->me_paxos_msg_data_u.promise.vb = r->vb;
	msg_send_to(ME_A_ &msg, to->index);
	fbr_log_d(&mctx->fbr, "Sent promise for instance %lu with value size"
			" %u", r->iid, r->v ? r->v->size1 : 0);
}

static void send_learn(ME_P_ struct acc_instance_record *r, struct me_peer *to)
{
	struct me_message msg;
	struct me_paxos_msg_data *data = &msg.me_message_u.paxos_message.data;

	assert(r->v->size1 > 0);

	msg.super_type = ME_PAXOS;
	data->type = ME_PAXOS_LEARN;
	data->me_paxos_msg_data_u.learn.i = r->iid;
	data->me_paxos_msg_data_u.learn.b = r->b;
	data->me_paxos_msg_data_u.learn.v = r->v;
	if(NULL == to)
		msg_send_all(ME_A_ &msg);
	else
		msg_send_to(ME_A_ &msg, to->index);
}

static void send_highest_accepted(ME_P)
{
	struct me_message msg;
	struct me_paxos_msg_data *data = &msg.me_message_u.paxos_message.data;
	uint64_t iid;
	iid = acs_get_highest_accepted(ME_A);
	msg.super_type = ME_PAXOS;
	data->type = ME_PAXOS_LAST_ACCEPTED;
	data->me_paxos_msg_data_u.last_accepted.i = iid;
	msg_send_all(ME_A_ &msg);
}

static void send_reject(ME_P_ struct acc_instance_record *r, struct me_peer *to)
{
	struct me_message msg;
	struct me_paxos_msg_data *data = &msg.me_message_u.paxos_message.data;
	msg.super_type = ME_PAXOS;
	data->type = ME_PAXOS_REJECT;
	data->me_paxos_msg_data_u.reject.i = r->iid;
	data->me_paxos_msg_data_u.reject.b = r->b;
	msg_send_to(ME_A_ &msg, to->index);
	fbr_log_d(&mctx->fbr, "Sent reject for instance %lu at ballot %lu", r->iid, r->b);
}

static void do_prepare(ME_P_ struct me_paxos_message *pmsg, struct me_peer
		*from)
{
	struct acc_instance_record *r = NULL;
	struct me_paxos_prepare_data *data;

	data = &pmsg->data.me_paxos_msg_data_u.prepare;
	if (0 == acs_find_record(ME_A_ &r, data->i, ACS_FM_CREATE)) {
		r->iid = data->i;
		r->b = data->b;
		r->v = NULL;
		r->vb = 0;
		r->is_final = 0;
		acs_store_record(ME_A_ r);
	}
	if (data->b < r->b) {
		send_reject(ME_A_ r, from);
		goto cleanup;
	}
	r->b = data->b;
	acs_store_record(ME_A_ r);
	fbr_log_d(&mctx->fbr, "Promised not to accept ballots lower that %lu for instance %lu", data->b, data->i);
	send_promise(ME_A_ r, from);
cleanup:
	acs_free_record(ME_A_ r);
}

static void do_accept(ME_P_ struct me_paxos_message *pmsg, struct me_peer
		*from)
{
	struct acc_instance_record *r = NULL;
	struct me_paxos_accept_data *data;
	char buf[ME_MAX_XDR_MESSAGE_LEN];

	data = &pmsg->data.me_paxos_msg_data_u.accept;
	assert(data->v->size1 > 0);
	if(0 == acs_find_record(ME_A_ &r, data->i, ACS_FM_JUST_FIND)) {
		//TODO: Add automatic accept of ballot 0
		return;
	}
	if(data->b < r->b) {
		//TODO: Add REJECT message here for speedup
		goto cleanup;
	}
	r->b = data->b;
	r->vb = data->b;
	if(NULL == r->v) {
		r->v = buf_sm_steal(data->v);
		assert(r->v->size1 > 0);
	} else
		//FIXME: Find the cause of this as it's a violation of the
		//FIXME: crutial safety property
		if(0 != buf_cmp(r->v, data->v)) {
			fbr_log_e(&mctx->fbr, "Conflict while accepting a value for instance %lu ballot %lu", r->iid, r->b);
			snprintf(buf, data->v->size1 + 1, "%s", data->v->ptr);
			fbr_log_e(&mctx->fbr, "Suggested value sized %d is: ``%s''", data->v->size1, buf);
			snprintf(buf, r->v->size1 + 1, "%s", r->v->ptr);
			fbr_log_e(&mctx->fbr, "Current value sized %d is: ``%s''", r->v->size1, buf);
			abort();
		}
	if(r->iid > acs_get_highest_accepted(ME_A))
		acs_set_highest_accepted(ME_A_ r->iid);
	acs_store_record(ME_A_ r);
	send_learn(ME_A_ r, NULL);
cleanup:
	acs_free_record(ME_A_ r);
}

static void do_retransmit(ME_P_ struct me_paxos_message *pmsg, struct me_peer
		*from)
{
	uint64_t iid;
	struct acc_instance_record *r = NULL;
	struct me_paxos_retransmit_data *data;

	data = &pmsg->data.me_paxos_msg_data_u.retransmit;
	for(iid = data->from; iid <= data->to; iid++) {
		if(0 == acs_find_record(ME_A_ &r, iid, ACS_FM_JUST_FIND))
			continue;
		if(NULL == r->v) {
			//FIXME: Not absolutely sure about this...
			acs_free_record(ME_A_ r);
			continue;
		}
		send_learn(ME_A_ r, from);
		acs_free_record(ME_A_ r);
	}
}

static void repeater_fiber(struct fbr_context *fiber_context, void *_arg)
{
	struct me_context *mctx;
	mctx = container_of(fiber_context, struct me_context, fbr);

	for(;;) {
		send_highest_accepted(ME_A);
		fbr_sleep(&mctx->fbr, mctx->args_info.acceptor_repeat_interval_arg);
	}
}

static void do_delivered_value(ME_P_ uint64_t iid, struct buffer *buffer)
{
	struct acc_instance_record *r = NULL;

	if (0 == acs_find_record(ME_A_ &r, iid, ACS_FM_CREATE)) {
		r->iid = iid;
		r->b = 0ULL;
		r->v = buffer;
		r->vb = 0ULL;
		r->is_final = 0;
	}
	if (0 == r->is_final) {
		r->is_final = 1;
		acs_store_record(ME_A_ r);
	}
	acs_set_highest_finalized(ME_A_ iid);
	acs_free_record(ME_A_ r);
}

static void process_lea_fb(ME_P_ struct fbr_buffer *lea_fb)
{
	struct lea_instance_info *instance_info;
	const size_t lii_size = sizeof(struct lea_instance_info);

	while (fbr_buffer_can_read(&mctx->fbr, lea_fb, lii_size)) {
		acs_batch_start(ME_A);
		while (fbr_buffer_can_read(&mctx->fbr, lea_fb, lii_size)) {
			instance_info = fbr_buffer_read_address(&mctx->fbr,
					lea_fb, lii_size);
			do_delivered_value(ME_A_ instance_info->iid,
					instance_info->buffer);
			fbr_buffer_read_advance(&mctx->fbr, lea_fb);
		}
		acs_batch_finish(ME_A);
	}
}

static void do_acceptor_msg(ME_P_ struct msg_info *info)
{
	struct me_paxos_message *pmsg;
	const char *s;
	pmsg = &info->msg->me_message_u.paxos_message;
	switch(pmsg->data.type) {
	case ME_PAXOS_PREPARE:
		do_prepare(ME_A_ pmsg, info->from);
		break;
	case ME_PAXOS_ACCEPT:
		do_accept(ME_A_ pmsg, info->from);
		break;
	case ME_PAXOS_RETRANSMIT:
		do_retransmit(ME_A_ pmsg, info->from);
		break;
	default:
		s = strval_me_paxos_message_type(pmsg->data.type);
		errx(EXIT_FAILURE, "wrong message type for acceptor:"
				" %s", s);
	}
	sm_free(info->msg);
}

static void process_fb(ME_P_ struct fbr_buffer *fb)
{
	struct msg_info *info;
	const size_t msg_size = sizeof(struct msg_info);

	while (fbr_buffer_can_read(&mctx->fbr, fb, msg_size)) {
		acs_batch_start(ME_A);
		while (fbr_buffer_can_read(&mctx->fbr, fb, msg_size)) {
			info = fbr_buffer_read_address(&mctx->fbr, fb,
					msg_size);
			do_acceptor_msg(ME_A_ info);
			fbr_buffer_read_advance(&mctx->fbr, fb);
		}
		acs_batch_finish(ME_A);
	}
}

void acc_fiber(struct fbr_context *fiber_context, void *_arg)
{
	struct me_context *mctx;
	fbr_id_t repeater;
	fbr_id_t learner;
	struct lea_fiber_arg lea_arg;
	struct fbr_ev_cond_var ev_fb;
	struct fbr_ev_cond_var ev_lea_fb;
	struct fbr_mutex fb_mutex;
	struct fbr_mutex lea_fb_mutex;
	struct fbr_buffer fb, lea_fb;
	struct fbr_ev_base *fb_events[3];
	int n_events;

	mctx = container_of(fiber_context, struct me_context, fbr);

	fbr_buffer_init(&mctx->fbr, &fb, 0);
	fbr_set_user_data(&mctx->fbr, fbr_self(&mctx->fbr), &fb);
	fbr_mutex_init(&mctx->fbr, &fb_mutex);
	fbr_buffer_init(&mctx->fbr, &lea_fb, 0);
	fbr_mutex_init(&mctx->fbr, &lea_fb_mutex);

	repeater = fbr_create(&mctx->fbr, "acceptor/repeater", repeater_fiber,
			NULL, 0);
	fbr_transfer(&mctx->fbr, repeater);

	fbr_ev_cond_var_init(&mctx->fbr, &ev_fb,
			fbr_buffer_cond_read(&mctx->fbr, &fb),
			&fb_mutex);
	fbr_ev_cond_var_init(&mctx->fbr, &ev_lea_fb,
			fbr_buffer_cond_read(&mctx->fbr, &lea_fb),
			&lea_fb_mutex);

	lea_arg.buffer = &lea_fb;
	lea_arg.starting_iid = acs_get_highest_finalized(ME_A);
	learner = fbr_create(&mctx->fbr, "acceptor/learner", lea_fiber,
			&lea_arg, 0);
	fbr_transfer(&mctx->fbr, learner);

	fb_events[0] = &ev_fb.ev_base;
	fb_events[1] = &ev_lea_fb.ev_base;
	fb_events[2] = NULL;

	fbr_set_noreclaim(&mctx->fbr, fbr_self(&mctx->fbr));
	for (;;) {
		fbr_mutex_lock(&mctx->fbr, &fb_mutex);
		fbr_mutex_lock(&mctx->fbr, &lea_fb_mutex);
		n_events = fbr_ev_wait_to(&mctx->fbr, fb_events, 0.25);
		assert(-1 != n_events);
		if (ev_fb.ev_base.arrived) {
			process_fb(ME_A_ &fb);
			fbr_mutex_unlock(&mctx->fbr, &fb_mutex);
		}
		if (ev_lea_fb.ev_base.arrived) {
			process_lea_fb(ME_A_ &lea_fb);
			fbr_mutex_unlock(&mctx->fbr, &lea_fb_mutex);
		}
		if (fbr_want_reclaim(&mctx->fbr, fbr_self(&mctx->fbr)))
			break;
	}
	acs_destroy(ME_A);
	fbr_reclaim(&mctx->fbr, repeater);
	fbr_set_reclaim(&mctx->fbr, fbr_self(&mctx->fbr));
}
