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

#ifndef _PAXOS_H_
#define _PAXOS_H_

#include <stdlib.h>
#include <uthash.h>

#include <me_protocol.h>
#include <context_fwd.h>
#include <acceptor.h>
#include <proposer.h>
#include <learner.h>

#define HASH_FIND_IID(head,findiid,out) \
	HASH_FIND(hh,head,findiid,sizeof(uint64_t),out)
#define HASH_ADD_IID(head,iidfield,add) \
	HASH_ADD(hh,head,iidfield,sizeof(uint64_t),add)

struct me_peer;

struct pxs_peer_info {
	int is_acceptor;
};

struct pxs_context {
	struct acc_context acc;
	struct pro_context pro;
	struct lea_context lea;
};

#define PXS_CONTEXT_INITIALIZER { \
	.acc = ACC_CONTEXT_INITIALIZER, \
	.pro = ACC_CONTEXT_INITIALIZER, \
	.lea = LEA_CONTEXT_INITIALIZER, \
}

void pxs_do_message(ME_P_ struct me_message *msg, struct me_peer *from);
void pxs_fiber_init(ME_P);
void pxs_send_acceptors(ME_P_ struct me_message *msg);
int pxs_acceptors_count(ME_P);

#endif
