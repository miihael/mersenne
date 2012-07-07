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

#include <uthash.h>
#include <err.h>

#include <peers.h>
#include <context.h>
#include <vars.h>

void add_peer(ME_P_ struct me_peer *p)
{
	HASH_ADD(hh, mctx->peers, addr.sin_addr, sizeof(struct in_addr), p);
}

struct me_peer *find_peer(ME_P_ struct sockaddr_in *addr)
{
	struct me_peer *p;
	HASH_FIND(hh, mctx->peers, &addr->sin_addr, sizeof(struct in_addr), p);
	return p;
}

struct me_peer *find_peer_by_index(ME_P_ int index)
{
	struct me_peer *p;

	for(p=mctx->peers; p != NULL; p=p->hh.next) {
		if(p->index == index)
			return p;
	}
	return NULL;
}

void delete_peer(ME_P_ struct me_peer *peer)
{
	HASH_DEL(mctx->peers, peer);
}

void load_peer_list(ME_P_ int my_index) {
	int i;
	char line_buf[255];
	FILE* peers_file;
	struct me_peer *p;

	peers_file = fopen("peers", "r");
	if(!peers_file)
		err(1, "fopen");
	i = 0;
	while(1) {
		fgets(line_buf, 255, peers_file);
		if(feof(peers_file))
			break;
		p = malloc(sizeof(struct me_peer));
		memset(p, 0, sizeof(struct me_peer));
		p->index = i;
		p->addr.sin_family = AF_INET;
		if(0 == inet_aton(line_buf, &p->addr.sin_addr))
			errx(EXIT_FAILURE, "invalid address: %s", line_buf);
		p->addr.sin_port = htons(MERSENNE_PORT);
		if(i == my_index) {
			mctx->me = p;
			printf("My ip is %s\n", line_buf);
		}
		add_peer(ME_A_ p);
		i++;
	}
	fclose(peers_file);
}