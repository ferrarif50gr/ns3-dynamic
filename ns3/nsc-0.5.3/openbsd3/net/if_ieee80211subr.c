/*	$OpenBSD: if_ieee80211subr.c,v 1.2 2004/01/26 03:35:36 fgsch Exp $	*/
/*	$NetBSD: if_ieee80211subr.c,v 1.43 2003/07/06 20:54:25 dyoung Exp $	*/
/*	$FreeBSD: src/sys/net/if_ieee80211subr.c,v 1.4 2003/01/21 08:55:59 alfred Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Atsushi Onoe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IEEE 802.11 generic handler
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/endian.h>

#include <dev/rndvar.h>
#include <crypto/arc4.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_llc.h>
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#include <net/if_ieee80211.h>

#include <dev/ic/if_wi_ieee.h>	/* XXX */

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif 

#ifdef IEEE80211_DEBUG
int ieee80211_debug = 0;
#define	DPRINTF(X)	if (ieee80211_debug) printf X
#define	DPRINTF2(X)	if (ieee80211_debug>1) printf X
#else
#define	DPRINTF(X)
#define	DPRINTF2(X)
#endif

static int ieee80211_send_prreq(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_prresp(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_auth(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_deauth(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_asreq(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_asresp(struct ieee80211com *,
    struct ieee80211_node *, int, int);
static int ieee80211_send_disassoc(struct ieee80211com *,
    struct ieee80211_node *, int, int);

static void ieee80211_recv_beacon(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_prreq(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_auth(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_asreq(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_asresp(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_disassoc(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);
static void ieee80211_recv_deauth(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);

static void ieee80211_recv_pspoll(struct ieee80211com *,
    struct mbuf *, int, u_int32_t);

static void ieee80211_crc_init(void);
static u_int32_t ieee80211_crc_update(u_int32_t, u_int8_t *, int);

static const char *ieee80211_mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"reserved#13",	"reserved#14",	"reserved#15"
};

void
ieee80211_ifattach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, rate, mword;
	struct ifmediareq imr;

	bcopy(ic->ic_myaddr, ((struct arpcom *)ifp)->ac_enaddr,
	    ETHER_ADDR_LEN);
	ether_ifattach(ifp);
#if NBPFILTER > 0
	bpfattach(&ic->ic_rawbpf, ifp, DLT_IEEE802_11,
	    sizeof(struct ieee80211_frame_addr4));
#endif
	ieee80211_crc_init();
	memcpy(ic->ic_chan_active, ic->ic_chan_avail,
	    sizeof(ic->ic_chan_active));
	if (isclr(ic->ic_chan_active, ic->ic_ibss_chan)) {
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
			if (isset(ic->ic_chan_active, i)) {
				ic->ic_ibss_chan = i;
				break;
			}
		}
	}
	/* ic->ic_bss.ni_chan must always be a valid channel */
	if (isclr(ic->ic_chan_active, ic->ic_bss.ni_chan))
		ic->ic_bss.ni_chan = ic->ic_ibss_chan;

	ic->ic_des_chan = IEEE80211_CHAN_ANY;
	ic->ic_fixed_rate = -1;
	if (ic->ic_lintval == 0)
		ic->ic_lintval = 100;	/* default sleep */
	TAILQ_INIT(&ic->ic_node);
	rate = 0;
	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		if (ic->ic_sup_rates[i] != 0)
			rate = (ic->ic_sup_rates[i] & IEEE80211_RATE_VAL) / 2;
	}
	if (rate)
		ifp->if_baudrate = IF_Mbps(rate);
	ifp->if_hdrlen = sizeof(struct ieee80211_frame);

	if (ic->ic_max_aid == 0)
		ic->ic_max_aid = IEEE80211_MAX_AID;

	/* initialize management frame handlers */
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_beacon;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_BEACON
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_beacon;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_prreq;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_AUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_auth;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asreq;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asreq;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asresp;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_asresp;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_DEAUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_deauth;
	ic->ic_recv_mgmt[IEEE80211_FC0_SUBTYPE_DISASSOC
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_recv_disassoc;

	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_prreq;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_PROBE_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_prresp;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_AUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_auth;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_DEAUTH
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_deauth;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asreq;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_REQ
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asreq;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_ASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asresp;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_REASSOC_RESP
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_asresp;
	ic->ic_send_mgmt[IEEE80211_FC0_SUBTYPE_DISASSOC
	    >> IEEE80211_FC0_SUBTYPE_SHIFT] = ieee80211_send_disassoc;

#define	ADD(s, o)	ifmedia_add(&ic->ic_media, \
	IFM_MAKEWORD(IFM_IEEE80211, (s), (o), 0), 0, NULL)

	ifmedia_init(&ic->ic_media, 0, ieee80211_media_change,
	    ieee80211_media_status);
	ADD(IFM_AUTO, 0);			/* infrastructure */
	if (ic->ic_flags & IEEE80211_F_HASAHDEMO)
		ADD(IFM_AUTO, IFM_IEEE80211_ADHOC | IFM_FLAG0);
	if (ic->ic_flags & IEEE80211_F_HASHOSTAP)
		ADD(IFM_AUTO, IFM_IEEE80211_HOSTAP);
	if (ic->ic_flags & IEEE80211_F_HASIBSS)
		ADD(IFM_AUTO, IFM_IEEE80211_ADHOC);
	if (ic->ic_flags & IEEE80211_F_HASMONITOR)
		ADD(IFM_AUTO, IFM_IEEE80211_MONITOR);

	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		rate = ic->ic_sup_rates[i];
		mword = ieee80211_rate2media(rate, ic->ic_phytype);
		if (mword == 0)
			continue;
		ADD(mword, 0);			/* infrastructure */
		if (ic->ic_flags & IEEE80211_F_HASAHDEMO)
			ADD(mword, IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (ic->ic_flags & IEEE80211_F_HASHOSTAP)
			ADD(mword, IFM_IEEE80211_HOSTAP);
		if (ic->ic_flags & IEEE80211_F_HASIBSS)
			ADD(mword, IFM_IEEE80211_ADHOC);
		if (ic->ic_flags & IEEE80211_F_HASMONITOR)
			ADD(mword, IFM_IEEE80211_MONITOR);
	}
	(*ic->ic_media.ifm_status)(ifp, &imr);
	ifmedia_set(&ic->ic_media, imr.ifm_active);
#undef	ADD
}

void
ieee80211_ifdetach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int s;

	s = splnet();
	IF_PURGE(&ic->ic_mgtq);
	IF_PURGE(&ic->ic_pwrsaveq);
	if (ic->ic_wep_ctx != NULL) {
		free(ic->ic_wep_ctx, M_DEVBUF);
		ic->ic_wep_ctx = NULL;
	}
	ieee80211_free_allnodes(ic);
	ifmedia_delete_instance(&ic->ic_media, IFM_INST_ANY);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ether_ifdetach(ifp);
	splx(s);
}

void
ieee80211_input(struct ifnet *ifp, struct mbuf *m, int rssi, u_int32_t rstamp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct ether_header *eh;
	void (*rh)(struct ieee80211com *, struct mbuf *, int, u_int);
	struct mbuf *m1;
	int error, len;
	u_int8_t dir, subtype;
	u_int8_t *bssid;
	u_int16_t rxseq;

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: receive packet with wrong version: %x\n",
			    ifp->if_xname, wh->i_fc[0]);
		goto err;
	}

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	if (ic->ic_state != IEEE80211_S_SCAN) {
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			ni = &ic->ic_bss;
			if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid)) {
				DPRINTF2(("ieee80211_input: other bss %s\n",
				    ether_sprintf(wh->i_addr2)));
				/* not interested in */
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
			if (dir == IEEE80211_FC1_DIR_NODS)
				bssid = wh->i_addr3;
			else
				bssid = wh->i_addr1;
			if (!IEEE80211_ADDR_EQ(bssid, ic->ic_bss.ni_bssid) &&
			    !IEEE80211_ADDR_EQ(bssid, etherbroadcastaddr) &&
			    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
			    IEEE80211_FC0_TYPE_CTL) {
				/* not interested in */
				DPRINTF2(("ieee80211_input: other bss %s\n",
				    ether_sprintf(wh->i_addr3)));
				goto out;
			}
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni == NULL) {
				DPRINTF2(("ieee80211_input: unknown src %s\n",
				    ether_sprintf(wh->i_addr2)));
				ni = &ic->ic_bss;	/* XXX allocate? */
			}
			break;
		case IEEE80211_M_MONITOR:
			goto out;
		}
		ni->ni_rssi = rssi;
		ni->ni_rstamp = rstamp;
		rxseq = ni->ni_rxseq;
		ni->ni_rxseq =
		    letoh16(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
		/* TODO: fragment */
		if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
		    rxseq == ni->ni_rxseq) {
			/* duplicate, silently discarded */
			goto out;
		}
		ni->ni_inact = 0;
	}

	if (ic->ic_set_tim != NULL &&
	    (wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) 
	    && ni->ni_pwrsave == 0) {
		/* turn on power save mode */

		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: power save mode on for %s\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));

		ni->ni_pwrsave = IEEE80211_PS_SLEEP;
	}
	if (ic->ic_set_tim != NULL &&
	    (wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) == 0  && 
	    ni->ni_pwrsave != 0) {
		/* turn off power save mode, dequeue stored packets */

		ni->ni_pwrsave = 0;
		if (ic->ic_set_tim) 
			ic->ic_set_tim(ic, ni->ni_associd, 0);

		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: power save mode off for %s\n",
			    ifp->if_xname, ether_sprintf(wh->i_addr2));

		while (!IF_IS_EMPTY(&ni->ni_savedq)) {
			struct mbuf *m;
			IF_DEQUEUE(&ni->ni_savedq, m);
			IF_ENQUEUE(&ic->ic_pwrsaveq, m);
			(*ifp->if_start)(ifp);
		}
	}

	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			if (dir != IEEE80211_FC1_DIR_FROMDS)
				goto out;
			if ((ifp->if_flags & IFF_SIMPLEX) &&
			    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    IEEE80211_ADDR_EQ(wh->i_addr3,
			    ic->ic_myaddr)) {
				/*
				 * In IEEE802.11 network, multicast packet
				 * sent from me is broadcasted from AP.
				 * It should be silently discarded for
				 * SIMPLEX interface.
				 */
				goto out;
			}
			break;
		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
			if (dir != IEEE80211_FC1_DIR_NODS)
				goto out;
			break;
		case IEEE80211_M_HOSTAP:
			if (dir != IEEE80211_FC1_DIR_TODS)
				goto out;
			/* check if source STA is associated */
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni == NULL) {
				DPRINTF(("ieee80211_input: "
				    "data from unknown src %s\n",
				    ether_sprintf(wh->i_addr2)));
				if ((ni = ieee80211_alloc_node(ic, wh->i_addr2,
				    1)) != NULL) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_NOT_AUTHED);
					ieee80211_free_node(ic, ni);
				}
				goto err;
			}
			if (ni->ni_associd == 0) {
				DPRINTF(("ieee80211_input: "
				    "data from unassoc src %s\n",
				    ether_sprintf(wh->i_addr2)));
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_NOT_ASSOCED);
				goto err;
			}
			break;
		case IEEE80211_M_MONITOR:
			/* Should never get here */
			break;
		}
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				m = ieee80211_wep_crypt(ifp, m, 0);
				if (m == NULL)
					goto err;
				wh = mtod(m, struct ieee80211_frame *);
			} else
				goto out;
		}
		/* copy to listener after decrypt */
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		m = ieee80211_decap(ifp, m);
		if (m == NULL)
			goto err;
		ifp->if_ipackets++;

		/* perform as a bridge within the AP */
		m1 = NULL;
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			eh = mtod(m, struct ether_header *);
			if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
				m1 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
				if (m1 == NULL)
					ifp->if_oerrors++;
				else
					m1->m_flags |= M_MCAST;
			} else {
				ni = ieee80211_find_node(ic, eh->ether_dhost);
				if (ni != NULL && ni->ni_associd != 0) {
					m1 = m;
					m = NULL;
				}
			}
			if (m1 != NULL) {
				len = m1->m_pkthdr.len;
				IFQ_ENQUEUE(&ifp->if_snd, m1, NULL, error);
				if (error)
					ifp->if_oerrors++;
				else {
					if (m != NULL)
						ifp->if_omcasts++;
					ifp->if_obytes += len;
				}
			}
		}
		if (m != NULL) {
#if NBPFILTER > 0
			/*
			 * If we forward packet into transmitter of the AP,
			 * we don't need to duplicate for DLT_EN10MB.
			 */
			if (ifp->if_bpf && m1 == NULL)
				bpf_mtap(ifp->if_bpf, m);
#endif
			ether_input_mbuf(ifp, m);
		}
		return;

	case IEEE80211_FC0_TYPE_MGT:
		if (dir != IEEE80211_FC1_DIR_NODS)
			goto err;
		if (ic->ic_opmode == IEEE80211_M_AHDEMO)
			goto out;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* drop frames without interest */
		if (ic->ic_state == IEEE80211_S_SCAN) {
			if (subtype != IEEE80211_FC0_SUBTYPE_BEACON &&
			    subtype != IEEE80211_FC0_SUBTYPE_PROBE_RESP)
				goto out;
		} else {
			if (ic->ic_opmode != IEEE80211_M_IBSS &&
			    subtype == IEEE80211_FC0_SUBTYPE_BEACON)
				goto out;
		}

		if (ifp->if_flags & IFF_DEBUG) {
			/* avoid to print too many frames */
			int doprint = 0;

			switch (subtype) {
			case IEEE80211_FC0_SUBTYPE_BEACON:
				if (ic->ic_state == IEEE80211_S_SCAN)
					doprint = 1;
				break;
			case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
				if (ic->ic_opmode == IEEE80211_M_IBSS)
					doprint = 1;
				break;
			default:
				doprint = 1;
				break;
			}
#ifdef IEEE80211_DEBUG
			doprint += ieee80211_debug;
#endif
			if (doprint)
				printf("%s: received %s from %s rssi %d\n",
				    ifp->if_xname,
				    ieee80211_mgt_subtype_name[subtype
				    >> IEEE80211_FC0_SUBTYPE_SHIFT],
				    ether_sprintf(wh->i_addr2), rssi);
		}
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		rh = ic->ic_recv_mgmt[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT];
		if (rh != NULL)
			(*rh)(ic, m, rssi, rstamp);
		m_freem(m);
		return;

	case IEEE80211_FC0_TYPE_CTL:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
			goto out;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL) {
			/* Dump out a single packet from the host */
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: got power save probe from %s\n",
				       ifp->if_xname, 
				       ether_sprintf(wh->i_addr2));
			ieee80211_recv_pspoll(ic, m, rssi, rstamp);
		}
		goto out;

	default:
		DPRINTF(("ieee80211_input: bad type %x\n", wh->i_fc[0]));
		/* should not come here */
		break;
	}
  err:
	ifp->if_ierrors++;
  out:
	if (m != NULL) {
#if NBPFILTER > 0
		if (ic->ic_rawbpf)
			bpf_mtap(ic->ic_rawbpf, m);
#endif
		m_freem(m);
	}
}

int
ieee80211_mgmt_output(struct ifnet *ifp, struct ieee80211_node *ni,
    struct mbuf *m, int type)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_frame *wh;

	if (ni == NULL)
		ni = &ic->ic_bss;
	ni->ni_inact = 0;
	M_PREPEND(m, IEEE80211_HEADER_LEN(ic), M_DONTWAIT);
	if (m == NULL)
		return (ENOMEM);
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT | type;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	*(u_int16_t *)wh->i_dur = 0;
	*(u_int16_t *)wh->i_seq =
	    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
	IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);

	if (ifp->if_flags & IFF_DEBUG) {
		/* avoid to print too many frames */
		if (ic->ic_opmode == IEEE80211_M_IBSS ||
#ifdef IEEE80211_DEBUG
		    ieee80211_debug > 1 ||
#endif
		    (type & IEEE80211_FC0_SUBTYPE_MASK) !=
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			printf("%s: sending %s to %s on channel %u\n",
			    ifp->if_xname,
			    ieee80211_mgt_subtype_name[
			    (type & IEEE80211_FC0_SUBTYPE_MASK)
			    >> IEEE80211_FC0_SUBTYPE_SHIFT],
			    ether_sprintf(ni->ni_macaddr),
			    ni->ni_chan);
	}
	IF_ENQUEUE(&ic->ic_mgtq, m);
	ifp->if_timer = 1;
	(*ifp->if_start)(ifp);
	return (0);
}

struct mbuf *
ieee80211_encap(struct ifnet *ifp, struct mbuf *m)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct llc *llc;
	struct ieee80211_node *ni;

	if (m->m_len < sizeof(struct ether_header)) {
		m = m_pullup(m, sizeof(struct ether_header));
		if (m == NULL)
			return (NULL);
	}
	memcpy(&eh, mtod(m, caddr_t), sizeof(struct ether_header));

	if (!IEEE80211_IS_MULTICAST(eh.ether_dhost) &&
	    (ic->ic_opmode == IEEE80211_M_IBSS ||
	     ic->ic_opmode == IEEE80211_M_HOSTAP)) {
		ni = ieee80211_find_node(ic, eh.ether_dhost);
		if (ni == NULL)
			ni = &ic->ic_bss;	/*XXX*/
	} else
		ni = &ic->ic_bss;
	ni->ni_inact = 0;

	m_adj(m, sizeof(struct ether_header) - sizeof(struct llc));
	llc = mtod(m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh.ether_type;
	M_PREPEND(m, IEEE80211_HEADER_LEN(ic), M_DONTWAIT);
	if (m == NULL)
		return (NULL);
	wh = mtod(m, struct ieee80211_frame *);
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *)wh->i_dur = 0;
	*(u_int16_t *)wh->i_seq =
	    htole16(ni->ni_txseq << IEEE80211_SEQ_SEQ_SHIFT);
	ni->ni_txseq++;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
		IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
		break;
	case IEEE80211_M_HOSTAP:
		wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, ni->ni_bssid);
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
		break;
	case IEEE80211_M_MONITOR:
		/* Should never get here! */
		m_freem(m);
		return (NULL);
	}
	return (m);
}

struct mbuf *
ieee80211_decap(struct ifnet *ifp, struct mbuf *m)
{
	struct ether_header *eh;
	struct ieee80211_frame wh;
	struct llc *llc;

	if (m->m_len < sizeof(wh) + sizeof(*llc)) {
		m = m_pullup(m, sizeof(wh) + sizeof(*llc));
		if (m == NULL)
			return (NULL);
	}
	memcpy(&wh, mtod(m, caddr_t), sizeof(wh));
	llc = (struct llc *)(mtod(m, caddr_t) + sizeof(wh));
	if (llc->llc_dsap == LLC_SNAP_LSAP && llc->llc_ssap == LLC_SNAP_LSAP &&
	    llc->llc_control == LLC_UI && llc->llc_snap.org_code[0] == 0 &&
	    llc->llc_snap.org_code[1] == 0 && llc->llc_snap.org_code[2] == 0) {
		m_adj(m, sizeof(wh) + sizeof(struct llc) - sizeof(*eh));
		llc = NULL;
	} else {
		m_adj(m, sizeof(wh) - sizeof(*eh));
	}
	eh = mtod(m, struct ether_header *);
	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		/* not yet supported */
		DPRINTF(("ieee80211_decap: DS to DS\n"));
		m_freem(m);
		return (NULL);
	}
	if (!ALIGNED_POINTER(mtod(m, caddr_t) + sizeof(*eh), u_int32_t)) {
		struct mbuf *n, *n0, **np;
		caddr_t newdata;
		int off, pktlen;

		n0 = NULL;
		np = &n0;
		off = 0;
		pktlen = m->m_pkthdr.len;
		while (pktlen > off) {
			if (n0 == NULL) {
				MGETHDR(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					return (NULL);
				}
				M_MOVE_PKTHDR(n, m);
				n->m_len = MHLEN;
			} else {
				MGET(n, M_DONTWAIT, MT_DATA);
				if (n == NULL) {
					m_freem(m);
					m_freem(n0);
					return (NULL);
				}
				n->m_len = MLEN;
			}
			if (pktlen - off >= MINCLSIZE) {
				MCLGET(n, M_DONTWAIT);
				if (n->m_flags & M_EXT)
					n->m_len = n->m_ext.ext_size;
			}
			if (n0 == NULL) {
				newdata =
				    (caddr_t)ALIGN(n->m_data + sizeof(*eh)) -
				    sizeof(*eh);
				n->m_len -= newdata - n->m_data;
				n->m_data = newdata;
			}
			if (n->m_len > pktlen - off)
				n->m_len = pktlen - off;
			m_copydata(m, off, n->m_len, mtod(n, caddr_t));
			off += n->m_len;
			*np = n;
			np = &n->m_next;
		}
		m_freem(m);
		m = n0;
	}
	if (llc != NULL) {
		eh = mtod(m, struct ether_header *);
		eh->ether_type = htons(m->m_pkthdr.len - sizeof(*eh));
	}
	return (m);
}

int
ieee80211_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ifreq *ifr = (struct ifreq *)data;
	int i, error = 0;
	struct ieee80211_nwid nwid;
	struct ieee80211_nwkey *nwkey;
	struct ieee80211_power *power;
	struct ieee80211_bssid *bssid;
	struct ieee80211_channel *chan;
	struct ieee80211_wepkey keys[IEEE80211_WEP_NKID];
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCS80211NWID:
		if ((error = copyin(ifr->ifr_data, &nwid, sizeof(nwid))) != 0)
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
		ic->ic_des_esslen = nwid.i_len;
		memcpy(ic->ic_des_essid, nwid.i_nwid, nwid.i_len);
		error = ENETRESET;
		break;
	case SIOCG80211NWID:
		memset(&nwid, 0, sizeof(nwid));
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			nwid.i_len = ic->ic_des_esslen;
			memcpy(nwid.i_nwid, ic->ic_des_essid, nwid.i_len);
			break;
		default:
			nwid.i_len = ic->ic_bss.ni_esslen;
			memcpy(nwid.i_nwid, ic->ic_bss.ni_essid, nwid.i_len);
			break;
		}
		error = copyout(&nwid, ifr->ifr_data, sizeof(nwid));
		break;
	case SIOCS80211NWKEY:
		nwkey = (struct ieee80211_nwkey *)data;
		if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0 &&
		    nwkey->i_wepon != IEEE80211_NWKEY_OPEN) {
			error = EINVAL;
			break;
		}
		/* check and copy keys */
		memset(keys, 0, sizeof(keys));
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keys[i].wk_len = nwkey->i_key[i].i_keylen;
			if ((keys[i].wk_len > 0 &&
			    keys[i].wk_len < IEEE80211_WEP_KEYLEN) ||
			    keys[i].wk_len > sizeof(keys[i].wk_key)) {
				error = EINVAL;
				break;
			}
			if (keys[i].wk_len <= 0)
				continue;
			if ((error = copyin(nwkey->i_key[i].i_keydat,
			    keys[i].wk_key, keys[i].wk_len)) != 0)
				break;
		}
		if (error)
			break;
		i = nwkey->i_defkid - 1;
		if (i < 0 || i >= IEEE80211_WEP_NKID ||
		    keys[i].wk_len == 0 ||
		    (keys[i].wk_len == -1 && ic->ic_nw_keys[i].wk_len == 0)) {
			if (nwkey->i_wepon != IEEE80211_NWKEY_OPEN) {
				error = EINVAL;
				break;
			}
		} else
			ic->ic_wep_txkey = i;
		/* save the key */
		if (nwkey->i_wepon == IEEE80211_NWKEY_OPEN)
			ic->ic_flags &= ~IEEE80211_F_WEPON;
		else
			ic->ic_flags |= IEEE80211_F_WEPON;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (keys[i].wk_len < 0)
				continue;
			ic->ic_nw_keys[i].wk_len = keys[i].wk_len;
			memcpy(ic->ic_nw_keys[i].wk_key, keys[i].wk_key,
			    sizeof(keys[i].wk_key));
		}
		error = ENETRESET;
		break;
	case SIOCG80211NWKEY:
		nwkey = (struct ieee80211_nwkey *)data;
		if (ic->ic_flags & IEEE80211_F_WEPON)
			nwkey->i_wepon = IEEE80211_NWKEY_WEP;
		else
			nwkey->i_wepon = IEEE80211_NWKEY_OPEN;
		nwkey->i_defkid = ic->ic_wep_txkey + 1;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			if (nwkey->i_key[i].i_keydat == NULL)
				continue;
			/* do not show any keys to non-root user */
			if ((error = suser(curproc, 0)) != 0)
				break;
			nwkey->i_key[i].i_keylen = ic->ic_nw_keys[i].wk_len;
			if ((error = copyout(ic->ic_nw_keys[i].wk_key,
			    nwkey->i_key[i].i_keydat,
			    ic->ic_nw_keys[i].wk_len)) != 0)
				break;
		}
		break;
	case SIOCS80211POWER:
		power = (struct ieee80211_power *)data;
		ic->ic_lintval = power->i_maxsleep;
		if (power->i_enabled != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASPMGT) == 0)
				error = EINVAL;
			else if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		}
		break;
	case SIOCG80211POWER:
		power = (struct ieee80211_power *)data;
		power->i_enabled = (ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0;
		power->i_maxsleep = ic->ic_lintval;
		break;
	case SIOCS80211BSSID:
		bssid = (struct ieee80211_bssid *)data;
		if (IEEE80211_ADDR_EQ(bssid->i_bssid, empty_macaddr))
			ic->ic_flags &= ~IEEE80211_F_DESBSSID;
		else {
			ic->ic_flags |= IEEE80211_F_DESBSSID;
			IEEE80211_ADDR_COPY(ic->ic_des_bssid, bssid->i_bssid);
		}
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
			    !IEEE80211_ADDR_EQ(ic->ic_des_bssid,
			    ic->ic_bss.ni_bssid))
				error = ENETRESET;
			break;
		}
		break;
	case SIOCG80211BSSID:
		bssid = (struct ieee80211_bssid *)data;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				IEEE80211_ADDR_COPY(bssid->i_bssid,
				    ic->ic_myaddr);
			else if (ic->ic_flags & IEEE80211_F_DESBSSID)
				IEEE80211_ADDR_COPY(bssid->i_bssid,
				    ic->ic_des_bssid);
			else
				memset(bssid->i_bssid, 0, IEEE80211_ADDR_LEN);
			break;
		default:
			IEEE80211_ADDR_COPY(bssid->i_bssid,
			    ic->ic_bss.ni_bssid);
			break;
		}
		break;
	case SIOCS80211CHANNEL:
		chan = (struct ieee80211_channel *)data;
		if (chan->i_channel == IEEE80211_CHAN_ANY)
			ic->ic_des_chan = IEEE80211_CHAN_ANY;
		else if (chan->i_channel > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, chan->i_channel)) {
			error = EINVAL;
			break;
		} else
			ic->ic_ibss_chan = ic->ic_des_chan = chan->i_channel;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			if (ic->ic_opmode == IEEE80211_M_STA) {
				if (ic->ic_des_chan != IEEE80211_CHAN_ANY &&
				    ic->ic_bss.ni_chan != ic->ic_des_chan)
					error = ENETRESET;
			} else {
				if (ic->ic_bss.ni_chan != ic->ic_ibss_chan)
					error = ENETRESET;
			}
			break;
		}
		break;
	case SIOCG80211CHANNEL:
		chan = (struct ieee80211_channel *)data;
		chan->i_channel = ieee80211_get_channel(ic);
		break;
	case SIOCGWAVELAN:
		error = ieee80211_cfgget(ifp, cmd, data);
		break;
	case SIOCSWAVELAN:
		error = suser(curproc, 0);
		if (error)
			break;
		error = ieee80211_cfgset(ifp, cmd, data);
		break;
	default:
		error = ether_ioctl(ifp, &ic->ic_ac, cmd, data);
		break;
	}
	return (error);
}

void
ieee80211_print_essid(u_int8_t *essid, int len)
{
	int i;
	u_int8_t *p; 

	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;
	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	} else {
		printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%02x", *p);
	}
}

void
ieee80211_dump_pkt(u_int8_t *buf, int len, int rate, int rssi)
{
	struct ieee80211_frame *wh;
	int i;

	wh = (struct ieee80211_frame *)buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		printf("NODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		printf("TODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s)", ether_sprintf(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		printf("FRDS %s", ether_sprintf(wh->i_addr3));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		printf("DSDS %s", ether_sprintf((u_int8_t *)&wh[1]));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s", ether_sprintf(wh->i_addr2));
		printf("->%s)", ether_sprintf(wh->i_addr1));
		break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		printf(" data");
		break;
	case IEEE80211_FC0_TYPE_MGT:
		printf(" %s", ieee80211_mgt_subtype_name[
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
		    >> IEEE80211_FC0_SUBTYPE_SHIFT]);
		break;
	default:
		printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
		break;
	}
	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
		printf(" WEP");
	if (rate >= 0)
		printf(" %dM", rate / 2);
	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");
	if (len > 0) {
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				printf(" ");
			printf("%02x", buf[i]);
		}
		printf("\n");
	}
}

void
ieee80211_watchdog(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	if (ic->ic_mgt_timer) {
		if (--ic->ic_mgt_timer == 0)
			ieee80211_new_state(ifp, IEEE80211_S_SCAN, -1);
	}
	if (ic->ic_inact_timer) {
		if (--ic->ic_inact_timer == 0) {
			struct ieee80211_node *ni, *nextbs;
			for (ni = TAILQ_FIRST(&ic->ic_node); ni != NULL; ) {
				if (++ni->ni_inact <= IEEE80211_INACT_MAX) {
					ni = TAILQ_NEXT(ni, ni_list);
					continue;
				}
				if (ifp->if_flags & IFF_DEBUG)
					printf("%s: station %s deauthenticate"
					    " (reason %d)\n",
					    ifp->if_xname,
					    ether_sprintf(ni->ni_macaddr),
					    IEEE80211_REASON_AUTH_EXPIRE);
				nextbs = TAILQ_NEXT(ni, ni_list);
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_EXPIRE);
				ieee80211_free_node(ic, ni);
				ni = nextbs;
			}
			if (!TAILQ_EMPTY(&ic->ic_node))
				ic->ic_inact_timer = IEEE80211_INACT_WAIT;
		}
	}
	if (ic->ic_mgt_timer != 0 || ic->ic_inact_timer != 0)
		ifp->if_timer = 1;
}

static void
ieee80211_begin_scan(struct ifnet *ifp, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = (void *)ifp;
	int i;

	memcpy(ic->ic_chan_scan, ic->ic_chan_active,
	    sizeof(ic->ic_chan_scan));

	DPRINTF(("ieee80211_begin_scan: scanning"));
	for (i = 0; i < sizeof(ic->ic_chan_scan) * NBBY; i++) {
	        if (!isset(ic->ic_chan_scan, i))
	                continue;
	        DPRINTF((" %d", i));
		ni->ni_chan = i;
	}
	DPRINTF(("\n"));
	clrbit(ic->ic_chan_scan, ni->ni_chan);
	ic->ic_flags |= IEEE80211_F_ASCAN;
	IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
}

void
ieee80211_next_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	int chan, s;

	s = splnet();

	chan = ic->ic_bss.ni_chan;
	for (;;) {
		chan = (chan + 1) % (IEEE80211_CHAN_MAX + 1);
		if (isset(ic->ic_chan_scan, chan))
			break;
		if (chan == ic->ic_bss.ni_chan) {
			DPRINTF(("ieee80211_next_scan: no chan available\n"));
			ieee80211_end_scan(ifp);
			splx(s);
			return;
		}
	}
	clrbit(ic->ic_chan_scan, chan);
	DPRINTF(("ieee80211_next_scan: chan %d->%d\n",
	    ic->ic_bss.ni_chan, chan));
	ic->ic_bss.ni_chan = chan;
	ieee80211_new_state(ifp, IEEE80211_S_SCAN, -1);
	splx(s);
}

void
ieee80211_create_ibss(struct ieee80211com *ic)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	int i;

	ni = &ic->ic_bss;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: creating ibss\n", ifp->if_xname);
	ic->ic_flags |= IEEE80211_F_SIBSS;
	ni->ni_nrate = 0;
	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		if (ic->ic_sup_rates[i])
			ni->ni_rates[ni->ni_nrate++] =
			    ic->ic_sup_rates[i];
	}
	IEEE80211_ADDR_COPY(ni->ni_macaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_myaddr);
	ni->ni_bssid[0] |= 0x02;	/* local bit for IBSS */
	ni->ni_esslen = ic->ic_des_esslen;
	memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
	ni->ni_rssi = 0;
	ni->ni_rstamp = 0;
	memset(ni->ni_tstamp, 0, sizeof(ni->ni_tstamp));
	ni->ni_intval = ic->ic_lintval;
	ni->ni_capinfo = IEEE80211_CAPINFO_IBSS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	ni->ni_chan = ic->ic_ibss_chan;
	if (ic->ic_phytype == IEEE80211_T_FH) {
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
}

int
ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ifnet *ifp = &ic->ic_if;
	u_int8_t rate;
	int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ni->ni_chan))
		fail |= 0x01;
	if (ic->ic_des_chan != IEEE80211_CHAN_ANY &&
	    ni->ni_chan != ic->ic_des_chan)
		fail |= 0x01;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (ic->ic_flags & IEEE80211_F_WEPON) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
	rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DONEGO);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if (ic->ic_des_esslen != 0 &&
	    (ni->ni_esslen != ic->ic_des_esslen ||
	     memcmp(ni->ni_essid, ic->ic_des_essid,
	     ic->ic_des_esslen) != 0))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;
	if (ifp->if_flags & IFF_DEBUG) {
		printf(" %c %s", fail ? '-' : '+',
		    ether_sprintf(ni->ni_macaddr));
		printf(" %s%c", ether_sprintf(ni->ni_bssid),
		    fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ni->ni_chan,
		    fail & 0x01 ? '!' : ' ');
		printf(" %+4d", ni->ni_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		    fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
		    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
		    "????",
		    fail & 0x02 ? '!' : ' ');
		printf(" %3s%c ",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
		    "wep" : "no",
		    fail & 0x04 ? '!' : ' ');
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}
	return (fail);
}

void
ieee80211_end_scan(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni, *nextbs, *selbs;
	void *p;

	ni = TAILQ_FIRST(&ic->ic_node);
	if (ni == NULL) {
		DPRINTF(("ieee80211_end_scan: no scan candidate\n"));
  notfound:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    (ic->ic_flags & IEEE80211_F_IBSSON) &&
		    ic->ic_des_esslen != 0) {
			ieee80211_create_ibss(ic);
			return;
		}
		if (ic->ic_flags & IEEE80211_F_ASCAN) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: entering passive scan mode\n",
				    ifp->if_xname);
			ic->ic_flags &= ~IEEE80211_F_ASCAN;
		}
		memcpy(ic->ic_chan_scan, ic->ic_chan_active,
		    sizeof(ic->ic_chan_active));
		ieee80211_next_scan(ifp);
		return;
	}
	selbs = NULL;
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s:\tmacaddr          bssid         chan  rssi rate flag  wep  essid\n",
		    ifp->if_xname);
	for (; ni != NULL; ni = nextbs) {
		nextbs = TAILQ_NEXT(ni, ni_list);
		if (ni->ni_fails) {
			/*
			 * The configuration of the access points may change
			 * during my scan.  So delete the entry for the AP
			 * and retry to associate if there is another beacon.
			 */
			if (ni->ni_fails++ > 2)
				ieee80211_free_node(ic, ni);
			continue;
		}
		if (ieee80211_match_bss(ic, ni) == 0) {
			if (selbs == NULL || ni->ni_rssi > selbs->ni_rssi)
				selbs = ni;
		}
	}
	if (selbs == NULL)
		goto notfound;
	p = ic->ic_bss.ni_private;
	ic->ic_bss = *selbs;
	ic->ic_bss.ni_private = p;
	if (p != NULL && ic->ic_node_privlen)
		memcpy(p, selbs->ni_private, ic->ic_node_privlen);
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		ieee80211_fix_rate(ic, &ic->ic_bss, IEEE80211_F_DOFRATE |
		    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
		if (ic->ic_bss.ni_nrate == 0) {
			selbs->ni_fails++;
			goto notfound;
		}
		ieee80211_new_state(ifp, IEEE80211_S_RUN, -1);
	} else
		ieee80211_new_state(ifp, IEEE80211_S_AUTH, -1);
}

int
ieee80211_get_rate(struct ieee80211com *ic)
{
	int rate;
	if (ic->ic_fixed_rate == -1) {
		if (ic->ic_state == IEEE80211_S_RUN)
			rate = ic->ic_bss.ni_rates[ic->ic_bss.ni_txrate];
		else
			rate = 0;
	} else
		rate = ic->ic_sup_rates[ic->ic_fixed_rate];
	return (rate & IEEE80211_RATE_VAL);
}

int
ieee80211_get_channel(struct ieee80211com *ic)
{
	switch (ic->ic_state) {
	case IEEE80211_S_INIT:
		if (ic->ic_opmode == IEEE80211_M_STA)
			return (ic->ic_des_chan);
		else
			return (ic->ic_ibss_chan);
	case IEEE80211_S_SCAN:	/* XXX could be confusing if ic_des_chan is
				 * set
				 */
	default:
		return (ic->ic_bss.ni_chan);
	}
}


struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211com *ic, u_int8_t *macaddr, int copy)
{
	struct ieee80211_node *ni;
	int hash;
	int s;

	ni = malloc(sizeof(struct ieee80211_node) + ic->ic_node_privlen,
	    M_DEVBUF, M_NOWAIT);
	if (ni == NULL)
		return (NULL);
	if (copy)
		memcpy(ni, &ic->ic_bss, sizeof(struct ieee80211_node));
	else
		memset(ni, 0, sizeof(struct ieee80211_node));
	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	if (ic->ic_node_privlen) {
		ni->ni_private = &ni[1];
		memset(ni->ni_private, 0, ic->ic_node_privlen);
	} else
		ni->ni_private = NULL;

	hash = IEEE80211_NODE_HASH(macaddr);
	s = splnet();
	TAILQ_INSERT_TAIL(&ic->ic_node, ni, ni_list);
	LIST_INSERT_HEAD(&ic->ic_hash[hash], ni, ni_hash);
	splx(s);
	ic->ic_inact_timer = IEEE80211_INACT_WAIT;
	return (ni);
}

struct ieee80211_node *
ieee80211_find_node(struct ieee80211com *ic, u_int8_t *macaddr)
{
	struct ieee80211_node *ni;
	int hash;
	int s;

	hash = IEEE80211_NODE_HASH(macaddr);
	s = splnet();
	LIST_FOREACH(ni, &ic->ic_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr))
			break;
	}
	splx(s);
	return (ni);
}

void
ieee80211_free_node(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	int s;

	s = splnet();
	if (ic->ic_node_free != NULL)
		(*ic->ic_node_free)(ic, ni);
	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	TAILQ_REMOVE(&ic->ic_node, ni, ni_list);
	LIST_REMOVE(ni, ni_hash);
	if (!IF_IS_EMPTY(&ni->ni_savedq)) {
		IF_PURGE(&ni->ni_savedq);
		if (ic->ic_set_tim)
			ic->ic_set_tim(ic, ni->ni_associd, 0);
	}
	splx(s);
	free(ni, M_DEVBUF);
	if (TAILQ_EMPTY(&ic->ic_node))
		ic->ic_inact_timer = 0;
}

void
ieee80211_free_allnodes(struct ieee80211com *ic)
{
	struct ieee80211_node *ni;

	while ((ni = TAILQ_FIRST(&ic->ic_node)) != NULL)
		ieee80211_free_node(ic, ni);  
}

int
ieee80211_fix_rate(struct ieee80211com *ic, struct ieee80211_node *ni, int flags)
{
	int i, j, ignore, error;
	int okrate, badrate;
	u_int8_t r;

	error = 0;
	okrate = badrate = 0;
	for (i = 0; i < ni->ni_nrate; ) {
		ignore = 0;
		if (flags & IEEE80211_F_DOSORT) {
			for (j = i + 1; j < ni->ni_nrate; j++) {
				if ((ni->ni_rates[i] & IEEE80211_RATE_VAL) >
				    (ni->ni_rates[j] & IEEE80211_RATE_VAL)) {
					r = ni->ni_rates[i];
					ni->ni_rates[i] = ni->ni_rates[j];
					ni->ni_rates[j] = r;
				}
			}
		}
		r = ni->ni_rates[i] & IEEE80211_RATE_VAL;
		badrate = r;
		if (flags & IEEE80211_F_DOFRATE) {
			if (ic->ic_fixed_rate >= 0 &&
			    r != (ic->ic_sup_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL))
				ignore++;
		}
		if (flags & IEEE80211_F_DONEGO) {
			for (j = 0; j < IEEE80211_RATE_SIZE; j++) {
				if (r ==
				    (ic->ic_sup_rates[j] & IEEE80211_RATE_VAL))
					break;
			}
			if (j == IEEE80211_RATE_SIZE) {
				if (ni->ni_rates[i] & IEEE80211_RATE_BASIC)
					error++;
				ignore++;
			}
		}
		if (flags & IEEE80211_F_DODEL) {
			if (ignore) {
				ni->ni_nrate--;
				for (j = i; j < ni->ni_nrate; j++)
					ni->ni_rates[j] = ni->ni_rates[j + 1];
				ni->ni_rates[j] = 0;
				continue;
			}
		}
		if (!ignore)
			okrate = ni->ni_rates[i];
		i++;
	}
	if (okrate == 0 || error != 0)
		return (badrate | IEEE80211_RATE_BASIC);
	return (okrate & IEEE80211_RATE_VAL);
}

static u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const u_int8_t rates[IEEE80211_RATE_SIZE])
{
	int i, j;

	*frm++ = IEEE80211_ELEMID_RATES;
	j = 0;
	for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
		if (rates[i] != 0) {
			frm[1 + j] = rates[i];
			j++;
		}
	}
	*frm++ = j;
	return (frm + j);
}

static u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = len;
	memcpy(frm, ssid, len);
	return (frm + len);
}

void
ieee80211_pwrsave(struct ieee80211com *ic, struct ieee80211_node *ni, 
		  struct mbuf *m)
{
	/* Store the new packet on our queue, changing the TIM if necessary */

	if (IF_IS_EMPTY(&ni->ni_savedq)) {
		ic->ic_set_tim(ic, ni->ni_associd, 1);
	}
	if (ni->ni_savedq.ifq_len >= IEEE80211_PS_MAX_QUEUE) {
		IF_DROP(&ni->ni_savedq);
		m_freem(m);
		if (ic->ic_if.if_flags & IFF_DEBUG)
			printf("%s: station %s power save queue overflow"
			       " of size %d drops %d\n",
			       ic->ic_if.if_xname, 
			       ether_sprintf(ni->ni_macaddr), 
			       IEEE80211_PS_MAX_QUEUE,
			       ni->ni_savedq.ifq_drops);
	} else {
		IF_ENQUEUE(&ni->ni_savedq, m);
	}
}

static int
ieee80211_send_prreq(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int dummy)
{
	int ret;
	struct mbuf *m;
	u_int8_t *frm;

	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	m->m_data += IEEE80211_HEADER_LEN(ic);
	frm = mtod(m, u_int8_t *);
	frm = ieee80211_add_ssid(frm, ic->ic_des_essid, ic->ic_des_esslen);
	frm = ieee80211_add_rates(frm, ic->ic_sup_rates);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	ret = ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
	ic->ic_mgt_timer = IEEE80211_TRANS_WAIT;
	return (ret);
}

static int
ieee80211_send_prresp(struct ieee80211com *ic, struct ieee80211_node *bs0,
    int type, int dummy)
{
	struct mbuf *m;
	u_int8_t *frm;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int16_t capinfo;

	/*
	 * probe response frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] parameter set (IBSS)
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	m->m_data += IEEE80211_HEADER_LEN(ic);
	frm = mtod(m, u_int8_t *);

	memset(frm, 0, 8);	/* timestamp should be filled later */
	frm += 8;
	*(u_int16_t *)frm = htole16(ni->ni_intval);
	frm += 2;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;

	frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
	frm = ieee80211_add_rates(frm, ni->ni_rates);

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		*frm++ = IEEE80211_ELEMID_IBSSPARMS;
		*frm++ = 2;
		*frm++ = 0; *frm++ = 0;		/* TODO: ATIM window */
	} else {	/* IEEE80211_M_HOSTAP */
		/* TODO: TIM */
		*frm++ = IEEE80211_ELEMID_TIM;
		*frm++ = 4;	/* length */
		*frm++ = 0;	/* DTIM count */
		*frm++ = 1;	/* DTIM period */
		*frm++ = 0;	/* bitmap control */
		*frm++ = 0;	/* Partial Virtual Bitmap (variable length) */
	}
	/* TODO: check MHLEN limit */
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return (ieee80211_mgmt_output(&ic->ic_if, bs0, m, type));
}

static int
ieee80211_send_auth(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int seq)
{
	struct mbuf *m;
	u_int16_t *frm;
	int ret;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	MH_ALIGN(m, 2 * 3);
	m->m_pkthdr.len = m->m_len = 6;
	frm = mtod(m, u_int16_t *);
	/* TODO: shared key auth */
	frm[0] = htole16(IEEE80211_AUTH_ALG_OPEN);
	frm[1] = htole16(seq);
	frm[2] = 0;			/* status */
	ret = ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
	if (ic->ic_opmode == IEEE80211_M_STA)
		ic->ic_mgt_timer = IEEE80211_TRANS_WAIT;
	return (ret);
}

static int
ieee80211_send_deauth(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int reason)
{
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: station %s deauthenticate (reason %d)\n",
		    ifp->if_xname, ether_sprintf(ni->ni_macaddr), reason);
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	MH_ALIGN(m, 2);
	m->m_pkthdr.len = m->m_len = 2;
	*mtod(m, u_int16_t *) = htole16(reason);
	return (ieee80211_mgmt_output(&ic->ic_if, ni, m, type));
}

static int
ieee80211_send_asreq(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int dummy)
{
	struct mbuf *m;
	u_int8_t *frm;
	u_int16_t capinfo;
	int ret;

	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] listen interval
	 *	[6*] current AP address (reassoc only)
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	m->m_data += IEEE80211_HEADER_LEN(ic);
	frm = mtod(m, u_int8_t *);

	capinfo = 0;
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo |= IEEE80211_CAPINFO_IBSS;
	else		/* IEEE80211_M_STA */
		capinfo |= IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;

	*(u_int16_t *)frm = htole16(ic->ic_lintval);
	frm += 2;

	if (type == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		IEEE80211_ADDR_COPY(frm, ic->ic_bss.ni_bssid);
		frm += IEEE80211_ADDR_LEN;
	}

	frm = ieee80211_add_ssid(frm, ni->ni_essid, ni->ni_esslen);
	frm = ieee80211_add_rates(frm, ic->ic_sup_rates);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	ret = ieee80211_mgmt_output(&ic->ic_if, ni, m, type);
	ic->ic_mgt_timer = IEEE80211_TRANS_WAIT;
	return (ret);
}

static int
ieee80211_send_asresp(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int status)
{
	struct mbuf *m;
	u_int8_t *frm;
	u_int16_t capinfo;

	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] status
	 *	[2] association ID
	 *	[tlv] supported rates
	 */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	m->m_data += IEEE80211_HEADER_LEN(ic);
	frm = mtod(m, u_int8_t *);

	capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_WEPON)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	*(u_int16_t *)frm = htole16(capinfo);
	frm += 2;

	*(u_int16_t *)frm = htole16(status);
	frm += 2;

	if (status == IEEE80211_STATUS_SUCCESS && ni != NULL)
		*(u_int16_t *)frm = htole16(ni->ni_associd);
	else
		*(u_int16_t *)frm = htole16(0);
	frm += 2;

	if (ni != NULL)
		frm = ieee80211_add_rates(frm, ni->ni_rates);
	else
		frm = ieee80211_add_rates(frm, ic->ic_bss.ni_rates);
	m->m_pkthdr.len = m->m_len = frm - mtod(m, u_int8_t *);

	return (ieee80211_mgmt_output(&ic->ic_if, ni, m, type));
}

static int
ieee80211_send_disassoc(struct ieee80211com *ic, struct ieee80211_node *ni,
    int type, int reason)
{
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: station %s disassociate (reason %d)\n",
		    ifp->if_xname, ether_sprintf(ni->ni_macaddr), reason);
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOMEM);
	MH_ALIGN(m, 2);
	m->m_pkthdr.len = m->m_len = 2;
	*mtod(m, u_int16_t *) = htole16(reason);
	return (ieee80211_mgmt_output(&ic->ic_if, ni, m,
	    IEEE80211_FC0_SUBTYPE_DISASSOC));
}

/* Verify the existence and length of __elem or get out. */
#define IEEE80211_VERIFY_ELEMENT(__subr_name, __wh, __elem, __maxlen) \
	do { \
		if (__elem == NULL) { \
			DPRINTF((#__subr_name ": no " #__elem "\n")); \
			return; \
		} \
		if (__elem[1] > __maxlen) { \
			DPRINTF((#__subr_name ": bad " #__elem \
			    " len %d from %s\n", \
			    __elem[1], ether_sprintf(__wh->i_addr2))); \
			return; \
		} \
	} while (0)

static void
ieee80211_recv_beacon(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm, *tstamp, *bintval, *capinfo, *ssid, *rates;
	u_int8_t chan, fhindex;
	u_int16_t fhdwell;

	if (ic->ic_opmode != IEEE80211_M_IBSS &&
	    ic->ic_state != IEEE80211_S_SCAN) {
		/* XXX: may be useful for background scan */
		return;
	}

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * beacon frame format
	 *	[8] time stamp
	 *	[2] beacon interval
	 *	[2] cabability information
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 *	[tlv] parameter set (FH/DS)
	 */
	tstamp  = frm;	frm += 8;
	bintval = frm;	frm += 2;
	capinfo = frm;	frm += 2;
	ssid = rates = NULL;
	chan = ic->ic_bss.ni_chan;
	fhdwell = 0;
	fhindex = 0;
	while (frm < efrm) {
		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		case IEEE80211_ELEMID_FHPARMS:
			if (ic->ic_phytype == IEEE80211_T_FH) {
				fhdwell = (frm[3] << 8) | frm[2];
				chan = IEEE80211_FH_CHAN(frm[4], frm[5]);
				fhindex = frm[6];
			}
			break;
		case IEEE80211_ELEMID_DSPARMS:
			if (ic->ic_phytype == IEEE80211_T_DS)
				chan = frm[2];
			break;
		}
		frm += frm[1] + 2;
	}
	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_beacon, wh, rates,
	    IEEE80211_RATE_SIZE);
	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_beacon, wh, ssid,
	    IEEE80211_NWID_LEN);
	ni = ieee80211_find_node(ic, wh->i_addr2);
#ifdef IEEE80211_DEBUG
	if (ieee80211_debug &&
	    (ieee80211_debug > 1 || ni == NULL ||
	    ic->ic_state == IEEE80211_S_SCAN)) {
		int is_prresp = ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		                 IEEE80211_FC0_SUBTYPE_PROBE_RESP);
		printf("ieee80211_recv_beacon: %s%s on chan %u (bss chan %u) ",
		    (ni == NULL ? "new " : ""),
		    is_prresp ? "probe response" : "beacon",
		    chan, ic->ic_bss.ni_chan);
		ieee80211_print_essid(ssid + 2, ssid[1]);
		printf(" from %s\n", ether_sprintf(wh->i_addr2));
	}
#endif
	if (ni == NULL) {
		if ((ni = ieee80211_alloc_node(ic, wh->i_addr2, 0)) == NULL)
			return;
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, ssid + 2, ssid[1]);
	} else if (ssid[1] != 0) {
		ni->ni_esslen = ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, ssid + 2, ssid[1]);
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	memcpy(ni->ni_tstamp, tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = letoh16(*(u_int16_t *)bintval);
	ni->ni_capinfo = letoh16(*(u_int16_t *)capinfo);
	ni->ni_chan = chan;
	ni->ni_fhdwell = fhdwell;
	ni->ni_fhindex = fhindex;
	if (ic->ic_state == IEEE80211_S_SCAN &&
	    (ic->ic_flags & IEEE80211_F_ASCAN) == 0)
		ieee80211_end_scan(&ic->ic_if);
}

static void
ieee80211_recv_prreq(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm, *ssid, *rates;
	u_int8_t rate;
	int allocbs;

	if (ic->ic_opmode == IEEE80211_M_STA)
		return;
	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * prreq frame format
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	ssid = rates = NULL;
	while (frm < efrm) {
		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		}
		frm += frm[1] + 2;
	}
	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_prreq, wh, rates,
	    IEEE80211_RATE_SIZE);
	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_prreq, wh, ssid,
	    IEEE80211_NWID_LEN);
	if (ssid[1] != 0 &&
	    (ssid[1] != ic->ic_bss.ni_esslen ||
	    memcmp(ssid + 2, ic->ic_bss.ni_essid, ic->ic_bss.ni_esslen) != 0)) {
#ifdef IEEE80211_DEBUG
		if (ieee80211_debug) {
			printf("ieee80211_recv_prreq: ssid unmatch ");
			ieee80211_print_essid(ssid + 2, ssid[1]);
			printf(" from %s\n", ether_sprintf(wh->i_addr2));
		}
#endif
		return;
	}

	ni = ieee80211_find_node(ic, wh->i_addr2);
	if (ni == NULL) {
		if ((ni = ieee80211_alloc_node(ic, wh->i_addr2, 1)) == NULL)
			return;
		DPRINTF(("ieee80211_recv_prreq: new req from %s\n",
		    ether_sprintf(wh->i_addr2)));
		allocbs = 1;
	} else
		allocbs = 0;
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	rate = ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT |
	    IEEE80211_F_DOFRATE | IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (rate & IEEE80211_RATE_BASIC) {
		DPRINTF(("ieee80211_recv_prreq: rate negotiation failed: %s\n",
		    ether_sprintf(wh->i_addr2)));
	} else {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP,
		    0);
	}
	if (allocbs && (ic->ic_opmode == IEEE80211_M_HOSTAP))
		ieee80211_free_node(ic, ni);
}

static void
ieee80211_recv_auth(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm;
	u_int16_t algo, seq, status;
	int allocbs;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * auth frame format
	 *	[2] algorithm
	 *	[2] sequence
	 *	[2] status
	 *	[tlv*] challenge
	 */
	if (frm + 6 > efrm) {
		DPRINTF(("ieee80211_recv_auth: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	algo   = letoh16(*(u_int16_t *)frm);
	seq    = letoh16(*(u_int16_t *)(frm + 2));
	status = letoh16(*(u_int16_t *)(frm + 4));
	if (algo != IEEE80211_AUTH_ALG_OPEN) {
		/* TODO: shared key auth */
		DPRINTF(("ieee80211_recv_auth: unsupported auth %d from %s\n",
		    algo, ether_sprintf(wh->i_addr2)));
		return;
	}
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:
		if (ic->ic_state != IEEE80211_S_RUN || seq != 1)
			return;
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;

	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_MONITOR:
		/* should not come here */
		break;

	case IEEE80211_M_HOSTAP:
		if (ic->ic_state != IEEE80211_S_RUN || seq != 1)
			return;
		allocbs = 0;
		ni = ieee80211_find_node(ic, wh->i_addr2);
		if (ni == NULL) {
			ni = ieee80211_alloc_node(ic, wh->i_addr2, 0);
			if (ni == NULL)
				return;
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss.ni_bssid);
			allocbs = 1;
		}
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, 2);
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s %s authenticated\n",
			    ifp->if_xname,
			    (allocbs ? "newly" : "already"),
			    ether_sprintf(ni->ni_macaddr));
		break;

	case IEEE80211_M_STA:
		if (ic->ic_state != IEEE80211_S_AUTH || seq != 2)
			return;
		if (status != 0) {
			printf("%s: authentication failed (reason %d) for %s\n",
			    ic->ic_if.if_xname, status,
			    ether_sprintf(wh->i_addr3));
			ni = ieee80211_find_node(ic, wh->i_addr2);
			if (ni != NULL)
				ni->ni_fails++;
			return;
		}
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	}
}

static void
ieee80211_recv_asreq(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int8_t *frm, *efrm, *ssid, *rates;
	u_int16_t capinfo, bintval;
	int reassoc, resp, newassoc;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP ||
	    (ic->ic_state != IEEE80211_S_RUN))
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
	    IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		reassoc = 1;
		resp = IEEE80211_FC0_SUBTYPE_REASSOC_RESP;
	} else {
		reassoc = 0;
		resp = IEEE80211_FC0_SUBTYPE_ASSOC_RESP;
	}
	/*
	 * asreq frame format
	 *	[2] capability information
	 *	[2] listen interval
	 *	[6*] current AP address (reassoc only)
	 *	[tlv] ssid
	 *	[tlv] supported rates
	 */
	if (frm + (reassoc ? 10 : 4) > efrm) {
		DPRINTF(("ieee80211_recv_asreq: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}

	if (!IEEE80211_ADDR_EQ(wh->i_addr3, ic->ic_bss.ni_bssid)) {
		DPRINTF(("ieee80211_recv_asreq: ignore other bss from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	capinfo = letoh16(*(u_int16_t *)frm);	frm += 2;
	bintval = letoh16(*(u_int16_t *)frm);	frm += 2;
	if (reassoc)
		frm += 6;	/* ignore current AP info */
	ssid = rates = NULL;
	while (frm < efrm) {
		switch (*frm) {
		case IEEE80211_ELEMID_SSID:
			ssid = frm;
			break;
		case IEEE80211_ELEMID_RATES:
			rates = frm;
			break;
		}
		frm += frm[1] + 2;
	}
	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_asreq, wh, rates,
	    IEEE80211_RATE_SIZE);
	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_asreq, wh, ssid,
	    IEEE80211_NWID_LEN);
	if (ssid[1] != ic->ic_bss.ni_esslen ||
	    memcmp(ssid + 2, ic->ic_bss.ni_essid, ssid[1]) != 0) {
#ifdef IEEE80211_DEBUG
		if (ieee80211_debug) {
			printf("ieee80211_recv_asreq: ssid unmatch ");
			ieee80211_print_essid(ssid + 2, ssid[1]);
			printf(" from %s\n", ether_sprintf(wh->i_addr2));
		}
#endif
		return;
	}
	ni = ieee80211_find_node(ic, wh->i_addr2);
	if (ni == NULL) {
		DPRINTF(("ieee80211_recv_asreq: not authenticated for %s\n",
		    ether_sprintf(wh->i_addr2)));
		if ((ni = ieee80211_alloc_node(ic, wh->i_addr2, 1)) == NULL)
			return;
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		    IEEE80211_REASON_ASSOC_NOT_AUTHED);
		ieee80211_free_node(ic, ni);
		return;
	}
	if ((capinfo & IEEE80211_CAPINFO_ESS) == 0 ||
	    (capinfo & IEEE80211_CAPINFO_PRIVACY) !=
	    ((ic->ic_flags & IEEE80211_F_WEPON) ?
	     IEEE80211_CAPINFO_PRIVACY : 0)) {
		DPRINTF(("ieee80211_recv_asreq: capability unmatch %x for %s\n",
		    capinfo, ether_sprintf(wh->i_addr2)));
		IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
		ni->ni_associd = 0;
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_CAPINFO);
		return;
	}
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_nrate == 0) {
		DPRINTF(("ieee80211_recv_asreq: rate unmatch for %s\n",
		    ether_sprintf(wh->i_addr2)));
		IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
		ni->ni_associd = 0;
		IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_BASIC_RATE);
		return;
	}
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	ni->ni_intval = bintval;
	ni->ni_capinfo = capinfo;
	ni->ni_chan = ic->ic_bss.ni_chan;
	ni->ni_fhdwell = ic->ic_bss.ni_fhdwell;
	ni->ni_fhindex = ic->ic_bss.ni_fhindex;
	if (ni->ni_associd == 0) {
		u_int16_t aid;

		/* 
		 * It would be clever to search the bitmap more efficiently,
		 * but this will do for now.
		 */
		for (aid = 1; aid < ic->ic_max_aid; aid++) {
			if (!IEEE80211_AID_ISSET(aid, ic->ic_aid_bitmap))
				break;
		}
		
		if (ic->ic_bss.ni_associd >= ic->ic_max_aid) {
			IEEE80211_SEND_MGMT(ic, ni, resp,
					    IEEE80211_REASON_ASSOC_TOOMANY);
			return;
		} else {
			ni->ni_associd = aid | 0xc000;
			IEEE80211_AID_SET(ni->ni_associd, ic->ic_aid_bitmap);
			newassoc = 1;
		}
	} else
		newassoc = 0;
	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_SUCCESS);
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: station %s %s associated at aid %d\n",
		       ifp->if_xname, (newassoc ? "newly" : "already"),
		       ether_sprintf(ni->ni_macaddr), 
		       (ni->ni_associd & ~0xc000));
}

static void
ieee80211_recv_asresp(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int8_t *frm, *efrm, *rates;
	int status;

	if (ic->ic_opmode != IEEE80211_M_STA ||
	    ic->ic_state != IEEE80211_S_ASSOC)
		return;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * asresp frame format
	 *	[2] capability information
	 *	[2] status
	 *	[2] association ID
	 *	[tlv] supported rates
	 */
	if (frm + 6 > efrm) {
		DPRINTF(("ieee80211_recv_asresp: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}

	ni->ni_capinfo = letoh16(*(u_int16_t *)frm);
	frm += 2;

	status = letoh16(*(u_int16_t *)frm);
	frm += 2;
	if (status != 0) {
		printf("%s: association failed (reason %d) for %s\n",
		    ifp->if_xname, status, ether_sprintf(wh->i_addr3));
		ni = ieee80211_find_node(ic, wh->i_addr2);
		if (ni != NULL)
			ni->ni_fails++;
		return;
	}
	ni->ni_associd = letoh16(*(u_int16_t *)frm);
	frm += 2;
	rates = frm;

	IEEE80211_VERIFY_ELEMENT(ieee80211_recv_asresp, wh, rates,
	    IEEE80211_RATE_SIZE);
	memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
	ni->ni_nrate = rates[1];
	memcpy(ni->ni_rates, rates + 2, ni->ni_nrate);
	ieee80211_fix_rate(ic, ni, IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
	    IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
	if (ni->ni_nrate == 0)
		return;
	ieee80211_new_state(ifp, IEEE80211_S_RUN,
	    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
}

static void
ieee80211_recv_disassoc(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * disassoc frame format
	 *	[2] reason
	 */
	if (frm + 2 > efrm) {
		DPRINTF(("ieee80211_recv_disassoc: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	reason = letoh16(*(u_int16_t *)frm);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_ASSOC,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	case IEEE80211_M_HOSTAP:
		if ((ni = ieee80211_find_node(ic, wh->i_addr2)) != NULL) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: station %s disassociated"
				    " by peer (reason %d)\n", ifp->if_xname,
				    ether_sprintf(ni->ni_macaddr), reason);
			IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
			ni->ni_associd = 0;
		}
		break;
	default:
		break;
	}
}

static void
ieee80211_recv_deauth(struct ieee80211com *ic, struct mbuf *m0, int rssi,
    u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	u_int8_t *frm, *efrm;
	u_int16_t reason;

	wh = mtod(m0, struct ieee80211_frame *);
	frm = (u_int8_t *)&wh[1];
	efrm = mtod(m0, u_int8_t *) + m0->m_len;
	/*
	 * dauth frame format
	 *	[2] reason
	 */
	if (frm + 2 > efrm) {
		DPRINTF(("ieee80211_recv_deauth: too short from %s\n",
		    ether_sprintf(wh->i_addr2)));
		return;
	}
	reason = letoh16(*(u_int16_t *)frm);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ieee80211_new_state(&ic->ic_if, IEEE80211_S_AUTH,
		    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
		break;
	case IEEE80211_M_HOSTAP:
		if ((ni = ieee80211_find_node(ic, wh->i_addr2)) != NULL) {
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: station %s deauthenticated"
				    " by peer (reason %d)\n", ifp->if_xname,
				    ether_sprintf(ni->ni_macaddr), reason);
			ieee80211_free_node(ic, ni);
		}
		break;
	default:
		break;
	}
}


static void
ieee80211_recv_pspoll(struct ieee80211com *ic, struct mbuf *m0, int rssi,
		      u_int32_t rstamp)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m;
	u_int16_t aid;

	if (ic->ic_set_tim == NULL)  /* No powersaving functionality */
		return;

	wh = mtod(m0, struct ieee80211_frame *);

	if ((ni = ieee80211_find_node(ic, wh->i_addr2)) == NULL) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent bogus power save poll\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2));
		return;
	}

	memcpy(&aid, wh->i_dur, sizeof(wh->i_dur));
	if ((aid & 0xc000) != 0xc000) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent bogus aid %x\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2), aid);
		return;
	}

	if (aid != ni->ni_associd) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s aid %x doesn't match pspoll "
			       "aid %x\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2),
			       ni->ni_associd, aid);
		return;
	}

	/* Okay, take the first queued packet and put it out... */

	IF_DEQUEUE(&ni->ni_savedq, m);
	if (m == NULL) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: station %s sent pspoll, "
			       "but no packets are saved\n",
			       ifp->if_xname, ether_sprintf(wh->i_addr2));
		return;
	}
	wh = mtod(m, struct ieee80211_frame *);

	/* 
	 * If this is the last packet, turn off the TIM fields.
	 * If there are more packets, set the more packets bit.
	 */

	if (IF_IS_EMPTY(&ni->ni_savedq)) {
		if (ic->ic_set_tim) 
			ic->ic_set_tim(ic, ni->ni_associd, 0);
	} else {
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
	}

	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: enqueued power saving packet for station %s\n",
		       ifp->if_xname, ether_sprintf(ni->ni_macaddr));

	IF_ENQUEUE(&ic->ic_pwrsaveq, m);
	(*ifp->if_start)(ifp);
}


int
ieee80211_new_state(struct ifnet *ifp, enum ieee80211_state nstate, int mgt)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni = &ic->ic_bss;
	u_int8_t old_bssid[IEEE80211_ADDR_LEN];
	int i, error, ostate;
#ifdef IEEE80211_DEBUG
	static const char *stname[] = 
	    { "INIT", "SCAN", "AUTH", "ASSOC", "RUN" };
#endif

	ostate = ic->ic_state;
	DPRINTF(("ieee80211_new_state: %s -> %s\n",
	    stname[ostate], stname[nstate]));
	if (ic->ic_newstate) {
		error = (*ic->ic_newstate)(ic->ic_softc, nstate);
		if (error == EINPROGRESS)
			return (0);
		if (error != 0)
			return (error);
	}

	/* state transition */
	ic->ic_state = nstate;
	switch (nstate) {
	case IEEE80211_S_INIT:
		switch (ostate) {
		case IEEE80211_S_INIT:
			break;
		case IEEE80211_S_RUN:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_ASSOC_LEAVE);
				break;
			case IEEE80211_M_HOSTAP:
				TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
					if (ni->ni_associd == 0)
						continue;
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DISASSOC,
					    IEEE80211_REASON_ASSOC_LEAVE);
				}
				break;
			default:
				break;
			}
			/* FALLTHRU */
		case IEEE80211_S_ASSOC:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_LEAVE);
				break;
			case IEEE80211_M_HOSTAP:
				TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_AUTH_LEAVE);
				}
				break;
			default:
				break;
			}
			/* FALLTHRU */
		case IEEE80211_S_AUTH:
		case IEEE80211_S_SCAN:
			ic->ic_mgt_timer = 0;
			IF_PURGE(&ic->ic_mgtq);
			IF_PURGE(&ic->ic_pwrsaveq);
			if (ic->ic_wep_ctx != NULL) {
				free(ic->ic_wep_ctx, M_DEVBUF);
				ic->ic_wep_ctx = NULL;
			}
			ieee80211_free_allnodes(ic);
			break;
		}
		break;
	case IEEE80211_S_SCAN:
		ic->ic_flags &= ~IEEE80211_F_SIBSS;
		ni = &ic->ic_bss;
		/* initialize bss for probe request */
		IEEE80211_ADDR_COPY(ni->ni_macaddr, etherbroadcastaddr);
		IEEE80211_ADDR_COPY(old_bssid, ic->ic_bss.ni_bssid);
		IEEE80211_ADDR_COPY(ni->ni_bssid, etherbroadcastaddr);
		ni->ni_nrate = 0;
		memset(ni->ni_rates, 0, IEEE80211_RATE_SIZE);
		for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
			if (ic->ic_sup_rates[i] != 0)
				ni->ni_rates[ni->ni_nrate++] =
				    ic->ic_sup_rates[i];
		}
		ni->ni_associd = 0;
		ni->ni_rstamp = 0;
		switch (ostate) {
		case IEEE80211_S_INIT:
			/* use lowest rate */
			ni->ni_txrate = 0;
			ieee80211_begin_scan(ifp, ni);
			break;
		case IEEE80211_S_SCAN:
			/* scan next */
			if (ic->ic_flags & IEEE80211_F_ASCAN) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
			}
			break;
		case IEEE80211_S_RUN:
			/* beacon miss */
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: no recent beacons from %s;"
				    " rescanning\n",
				    ifp->if_xname, ether_sprintf(old_bssid));
			ieee80211_free_allnodes(ic);
			/* FALLTHRU */
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			/* timeout restart scan */
			ni = ieee80211_find_node(ic, ic->ic_bss.ni_macaddr);
			if (ni != NULL)
				ni->ni_fails++;
			ieee80211_begin_scan(ifp, &ic->ic_bss);
			break;
		}
		break;
	case IEEE80211_S_AUTH:
		switch (ostate) {
		case IEEE80211_S_INIT:
			DPRINTF(("ieee80211_new_state: invalid transition\n"));
			break;
		case IEEE80211_S_SCAN:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_AUTH, 1);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			switch (mgt) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				/* ??? */
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				/* ignore and retry scan on timeout */
				break;
			}
			break;
		case IEEE80211_S_RUN:
			switch (mgt) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				ic->ic_state = ostate;	/* stay RUN */
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				/* try to reauth */
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 1);
				break;
			}
			break;
		}
		break;
	case IEEE80211_S_ASSOC:
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
		case IEEE80211_S_ASSOC:
			DPRINTF(("ieee80211_new_state: invalid transition\n"));
			break;
		case IEEE80211_S_AUTH:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			break;
		case IEEE80211_S_RUN:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 1);
			break;
		}
		break;
	case IEEE80211_S_RUN:
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_AUTH:
		case IEEE80211_S_RUN:
			DPRINTF(("ieee80211_new_state: invalid transition\n"));
			break;
		case IEEE80211_S_SCAN:		/* adhoc mode */
		case IEEE80211_S_ASSOC:		/* infra mode */
			if (ifp->if_flags & IFF_DEBUG) {
				printf("%s: ", ifp->if_xname);
				if (ic->ic_opmode == IEEE80211_M_STA)
					printf("associated ");
				else
					printf("synchronized ");
				printf("with %s ssid ",
				    ether_sprintf(ic->ic_bss.ni_bssid));
				ieee80211_print_essid(ic->ic_bss.ni_essid,
				    ic->ic_bss.ni_esslen);
				printf(" channel %d\n", ic->ic_bss.ni_chan);
			}
			/* start with highest negotiated rate */
			ic->ic_bss.ni_txrate = ic->ic_bss.ni_nrate - 1;
			ic->ic_mgt_timer = 0;
			(*ifp->if_start)(ifp);
			break;
		}
		break;
	}
	return (0);
}

struct mbuf *
ieee80211_wep_crypt(struct ifnet *ifp, struct mbuf *m0, int txflag)
{
	struct ieee80211com *ic = (void *)ifp;
	struct mbuf *m, *n, *n0;
	struct ieee80211_frame *wh;
	int i, left, len, moff, noff, kid;
	u_int32_t iv, crc;
	u_int8_t *ivp;
	void *ctx;
	u_int8_t keybuf[IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE];
	u_int8_t crcbuf[IEEE80211_WEP_CRCLEN];

	n0 = NULL;
	if ((ctx = ic->ic_wep_ctx) == NULL) {
		ctx = malloc(RC4STATE, M_DEVBUF, M_NOWAIT);
		if (ctx == NULL)
			goto fail;
		ic->ic_wep_ctx = ctx;
	}
	m = m0;
	left = m->m_pkthdr.len;
	MGET(n, M_DONTWAIT, m->m_type);
	n0 = n;
	if (n == NULL)
		goto fail;
	M_MOVE_PKTHDR(n, m);
	len = IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN;
	if (txflag) {
		n->m_pkthdr.len += len;
	} else {
		n->m_pkthdr.len -= len;
		left -= len;
	}
	n->m_len = MHLEN;
	if (n->m_pkthdr.len >= MINCLSIZE) {
		MCLGET(n, M_DONTWAIT);
		if (n->m_flags & M_EXT)
			n->m_len = n->m_ext.ext_size;
	}
	len = sizeof(struct ieee80211_frame);
	memcpy(mtod(n, caddr_t), mtod(m, caddr_t), len);
	wh = mtod(n, struct ieee80211_frame *);
	left -= len;
	moff = len;
	noff = len;
	if (txflag) {
		kid = ic->ic_wep_txkey;
		wh->i_fc[1] |= IEEE80211_FC1_WEP;
		if (!ic->ic_iv_flag) {
			ic->ic_iv = arc4random();
			ic->ic_iv_flag++;
		} else
	                iv = ic->ic_iv + 1;
		/*
		 * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
		 * (B, 255, N) with 3 <= B < 8
		 */
		if (iv >= 0x03ff00 &&
		    (iv & 0xf8ff00) == 0x00ff00)
			iv += 0x000100;
		ic->ic_iv = iv + 1;
		/* put iv in little endian to prepare 802.11i */
		ivp = mtod(n, u_int8_t *) + noff;
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++) {
			ivp[i] = iv & 0xff;
			iv >>= 8;
		}
		ivp[IEEE80211_WEP_IVLEN] = kid << 6;	/* pad and keyid */
		noff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	} else {
		wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
		ivp = mtod(m, u_int8_t *) + moff;
		kid = ivp[IEEE80211_WEP_IVLEN] >> 6;
		moff += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN;
	}
	memcpy(keybuf, ivp, IEEE80211_WEP_IVLEN);
	memcpy(keybuf + IEEE80211_WEP_IVLEN, ic->ic_nw_keys[kid].wk_key,
	    ic->ic_nw_keys[kid].wk_len);
	rc4_keysetup(ctx, keybuf,
	    IEEE80211_WEP_IVLEN + ic->ic_nw_keys[kid].wk_len);

	/* encrypt with calculating CRC */
	crc = ~0;
	while (left > 0) {
		len = m->m_len - moff;
		if (len == 0) {
			m = m->m_next;
			moff = 0;
			continue;
		}
		if (len > n->m_len - noff) {
			len = n->m_len - noff;
			if (len == 0) {
				MGET(n->m_next, M_DONTWAIT, n->m_type);
				if (n->m_next == NULL)
					goto fail;
				n = n->m_next;
				n->m_len = MLEN;
				if (left >= MINCLSIZE) {
					MCLGET(n, M_DONTWAIT);
					if (n->m_flags & M_EXT)
						n->m_len = n->m_ext.ext_size;
				}
				noff = 0;
				continue;
			}
		}
		if (len > left)
			len = left;
		rc4_crypt(ctx, mtod(n, caddr_t) + noff,
		    mtod(m, caddr_t) + moff, len);
		if (txflag)
			crc = ieee80211_crc_update(crc,
			    mtod(m, u_int8_t *) + moff, len);
		else
			crc = ieee80211_crc_update(crc,
			    mtod(n, u_int8_t *) + noff, len);
		left -= len;
		moff += len;
		noff += len;
	}
	crc = ~crc;
	if (txflag) {
		*(u_int32_t *)crcbuf = htole32(crc);
		if (n->m_len >= noff + sizeof(crcbuf))
			n->m_len = noff + sizeof(crcbuf);
		else {
			n->m_len = noff;
			MGET(n->m_next, M_DONTWAIT, n->m_type);
			if (n->m_next == NULL)
				goto fail;
			n = n->m_next;
			n->m_len = sizeof(crcbuf);
			noff = 0;
		}
		rc4_crypt(ctx, mtod(n, caddr_t) + noff, crcbuf,
		    sizeof(crcbuf));
	} else {
		n->m_len = noff;
		for (noff = 0; noff < sizeof(crcbuf); noff += len) {
			len = sizeof(crcbuf) - noff;
			if (len > m->m_len - moff)
				len = m->m_len - moff;
			if (len > 0)
				rc4_crypt(ctx, crcbuf + noff,
				    mtod(m, caddr_t) + moff, len);
			m = m->m_next;
			moff = 0;
		}
		if (crc != letoh32(*(u_int32_t *)crcbuf)) {
#ifdef IEEE80211_DEBUG
			if (ieee80211_debug) {
				printf("%s: decrypt CRC error\n",
				    ifp->if_xname);
				if (ieee80211_debug > 1)
					ieee80211_dump_pkt(n0->m_data,
					    n0->m_len, -1, -1);
			}
#endif
			goto fail;
		}
	}
	m_freem(m0);
	return (n0);

  fail:
	m_freem(m0);
	m_freem(n0);
	return (NULL);
}

/*
 * CRC 32 -- routine from RFC 2083
 */

/* Table of CRCs of all 8-bit messages */
static u_int32_t ieee80211_crc_table[256];

/* Make the table for a fast CRC. */
static void
ieee80211_crc_init(void)
{
	u_int32_t c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (u_int32_t)n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320UL ^ (c >> 1);
			else
				c = c >> 1;
		}
		ieee80211_crc_table[n] = c;
	}
}

/*
 * Update a running CRC with the bytes buf[0..len-1]--the CRC
 * should be initialized to all 1's, and the transmitted value
 * is the 1's complement of the final running CRC
 */

static u_int32_t
ieee80211_crc_update(u_int32_t crc, u_int8_t *buf, int len)
{
	u_int8_t *endbuf;

	for (endbuf = buf + len; buf < endbuf; buf++)
		crc = ieee80211_crc_table[(crc ^ *buf) & 0xff] ^ (crc >> 8);
	return (crc);
}

int
ieee80211_media_change(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ifmedia_entry *ime;
	enum ieee80211_opmode newmode;
	int i, rate, error = 0;

	ime = ic->ic_media.ifm_cur;
	if (IFM_SUBTYPE(ime->ifm_media) == IFM_AUTO) {
		i = -1;
	} else {
		rate = ieee80211_media2rate(ime->ifm_media, ic->ic_phytype);
		if (rate == 0)
			return (EINVAL);
		for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
			if ((ic->ic_sup_rates[i] & IEEE80211_RATE_VAL) == rate)
				break;
		}
		if (i == IEEE80211_RATE_SIZE)
			return (EINVAL);
	}
	if (ic->ic_fixed_rate != i) {
		ic->ic_fixed_rate = i;
		error = ENETRESET;
	}

	if ((ime->ifm_media & IFM_IEEE80211_ADHOC) &&
	    (ime->ifm_media & IFM_FLAG0))
		newmode = IEEE80211_M_AHDEMO;
	else if (ime->ifm_media & IFM_IEEE80211_ADHOC)
		newmode = IEEE80211_M_IBSS;
	else if (ime->ifm_media & IFM_IEEE80211_HOSTAP)
		newmode = IEEE80211_M_HOSTAP;
	else if (ime->ifm_media & IFM_IEEE80211_MONITOR)
		newmode = IEEE80211_M_MONITOR;
	else
		newmode = IEEE80211_M_STA;
	if (ic->ic_opmode != newmode) {
		ic->ic_opmode = newmode;
		error = ENETRESET;
	}
	if (error == ENETRESET) {
		if ((ifp->if_flags & IFF_UP) != 0)
			error = (*ifp->if_init)(ifp);
		else
			error = 0;
	}
	if (error == 0)
		ifp->if_baudrate = ifmedia_baudrate(ime->ifm_media);

	return (error);
}

void
ieee80211_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ieee80211com *ic = (void *)ifp;
	int rate;

	imr->ifm_status = IFM_AVALID;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		rate = ic->ic_bss.ni_rates[ic->ic_bss.ni_txrate] &
		    IEEE80211_RATE_VAL;
	else {
		if (ic->ic_fixed_rate == -1)
			rate = 0;
		else
			rate = ic->ic_sup_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL;
	}
	imr->ifm_active |= ieee80211_rate2media(rate, ic->ic_phytype);
	switch (ic->ic_opmode) {
	case IEEE80211_M_AHDEMO:
		imr->ifm_active |= IFM_IEEE80211_ADHOC | IFM_FLAG0;
		break;
	case IEEE80211_M_HOSTAP:
		imr->ifm_active |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;
	case IEEE80211_M_STA:
		break;
	}
}

/*
 * convert IEEE80211 rate value to ifmedia subtype.
 * ieee80211 rate is in unit of 0.5Mbps.
 */

int
ieee80211_rate2media(int rate, enum ieee80211_phytype phytype)
{
	int mword;

	mword = 0;
	switch (phytype) {
	case IEEE80211_T_FH:
		switch (rate & IEEE80211_RATE_VAL) {
		case 0:
			mword = IFM_AUTO;
			break;
		case 2:
			mword = IFM_IEEE80211_FH1;
			break;
		case 4:
			mword = IFM_IEEE80211_FH2;
			break;
		default:
			mword = IFM_NONE;
			break;
		}
		break;

	case IEEE80211_T_DS:
		switch (rate & IEEE80211_RATE_VAL) {
		case 0:
			mword = IFM_AUTO;
			break;
		case 2:
			mword = IFM_IEEE80211_DS1;
			break;
		case 4:
			mword = IFM_IEEE80211_DS2;
			break;
		case 11:
			mword = IFM_IEEE80211_DS5;
			break;
		case 22:
			mword = IFM_IEEE80211_DS11;
			break;
		default:
			mword = IFM_NONE;
			break;
		}
		break;

	default:
		mword = IFM_MANUAL;
		break;
	}
	return (mword);
}

int
ieee80211_media2rate(int mword, enum ieee80211_phytype phytype)
{
	int rate;

	rate = 0;
	switch (phytype) {
	case IEEE80211_T_FH:
		switch (IFM_SUBTYPE(mword)) {
		case IFM_IEEE80211_FH1:
			rate = 2;
			break;
		case IFM_IEEE80211_FH2:
			rate = 4;
			break;
		}
		break;

	case IEEE80211_T_DS:
		switch (IFM_SUBTYPE(mword)) {
		case IFM_IEEE80211_DS1:
			rate = 2;
			break;
		case IFM_IEEE80211_DS2:
			rate = 4;
			break;
		case IFM_IEEE80211_DS5:
			rate = 11;
			break;
		case IFM_IEEE80211_DS11:
			rate = 22;
			break;
		}
		break;

	default:
		break;
	}
	return (rate);
}

/*
 * XXX
 * Wireless LAN specific configuration interface, which is compatible
 * with wiconfig(8).
 */

int
ieee80211_cfgget(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, j, error;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_req wreq;
	struct wi_ltv_keys *keys;
	struct wi_apinfo *ap;
	struct ieee80211_node *ni;
#ifdef WICACHE
	struct wi_sigcache wsc;
#endif /* WICACHE */

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return (error);
	wreq.wi_len = 0;
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
		/* nothing appropriate */
		break;
	case WI_RID_NODENAME:
		strlcpy((char *)&wreq.wi_val[1], hostname,
		    sizeof(wreq.wi_val) - 1);
		wreq.wi_val[0] = htole16(strlen(hostname));
		wreq.wi_len = (1 + strlen(hostname) + 1) / 2;
		break;
	case WI_RID_CURRENT_SSID:
		if (ic->ic_state != IEEE80211_S_RUN) {
			wreq.wi_val[0] = 0;
			wreq.wi_len = 1;
			break;
		}
		wreq.wi_val[0] = htole16(ic->ic_bss.ni_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_bss.ni_essid,
		    ic->ic_bss.ni_esslen);
		wreq.wi_len = (1 + ic->ic_bss.ni_esslen + 1) / 2;
		break;
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		wreq.wi_val[0] = htole16(ic->ic_des_esslen);
		memcpy(&wreq.wi_val[1], ic->ic_des_essid, ic->ic_des_esslen);
		wreq.wi_len = (1 + ic->ic_des_esslen + 1) / 2;
		break;
	case WI_RID_CURRENT_BSSID:
		if (ic->ic_state == IEEE80211_S_RUN)
			IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_bss.ni_bssid);
		else
			memset(wreq.wi_val, 0, IEEE80211_ADDR_LEN);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_CHANNEL_LIST:
		memset(wreq.wi_val, 0, sizeof(wreq.wi_val));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
			if (isset(ic->ic_chan_active, i)) {
				setbit((u_int8_t *)wreq.wi_val, j);
				wreq.wi_len = j / 16 + 1;
			}
		}
		break;
	case WI_RID_OWN_CHNL:
		wreq.wi_val[0] = htole16(ic->ic_ibss_chan);
		wreq.wi_len = 1;
		break;
	case WI_RID_CURRENT_CHAN:
		wreq.wi_val[0] = htole16(ic->ic_bss.ni_chan);
		wreq.wi_len = 1;
		break;
	case WI_RID_COMMS_QUALITY:
		wreq.wi_val[0] = 0;				/* quality */
		wreq.wi_val[1] = htole16(ic->ic_bss.ni_rssi);	/* signal */
		wreq.wi_val[2] = 0;				/* noise */
		wreq.wi_len = 3;
		break;
	case WI_RID_PROMISC:
		wreq.wi_val[0] = htole16((ifp->if_flags & IFF_PROMISC) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_PORTTYPE:
		wreq.wi_val[0] = htole16(ic->ic_opmode);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAC_NODE:
		IEEE80211_ADDR_COPY(wreq.wi_val, ic->ic_myaddr);
		wreq.wi_len = IEEE80211_ADDR_LEN / 2;
		break;
	case WI_RID_TX_RATE:
		if (ic->ic_fixed_rate == -1)
			wreq.wi_val[0] = 0;	/* auto */
		else
			wreq.wi_val[0] = htole16(
			    (ic->ic_sup_rates[ic->ic_fixed_rate] &
			    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_TX_RATE:
		wreq.wi_val[0] = htole16(
		    (ic->ic_bss.ni_rates[ic->ic_bss.ni_txrate] &
		    IEEE80211_RATE_VAL) / 2);
		wreq.wi_len = 1;
		break;
	case WI_RID_RTS_THRESH:
		wreq.wi_val[0] = htole16(IEEE80211_MAX_LEN);	/* TODO: RTS */
		wreq.wi_len = 1;
		break;
	case WI_RID_CREATE_IBSS:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_IBSSON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MICROWAVE_OVEN:
		wreq.wi_val[0] = 0;	/* no ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_ROAMING_MODE:
		wreq.wi_val[0] = htole16(1);	/* enabled ... not supported */
		wreq.wi_len = 1;
		break;
	case WI_RID_SYSTEM_SCALE:
		wreq.wi_val[0] = htole16(1);	/* low density ... not supp */
		wreq.wi_len = 1;
		break;
	case WI_RID_PM_ENABLED:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_MAX_SLEEP:
		wreq.wi_val[0] = htole16(ic->ic_lintval);
		wreq.wi_len = 1;
		break;
	case WI_RID_CUR_BEACON_INT:
		wreq.wi_val[0] = htole16(ic->ic_bss.ni_intval);
		wreq.wi_len = 1;
		break;
	case WI_RID_WEP_AVAIL:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_HASWEP) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_CNFAUTHMODE:
		wreq.wi_val[0] = htole16(1);	/* TODO: open system only */
		wreq.wi_len = 1;
		break;
	case WI_RID_ENCRYPTION:
		wreq.wi_val[0] =
		    htole16((ic->ic_flags & IEEE80211_F_WEPON) ? 1 : 0);
		wreq.wi_len = 1;
		break;
	case WI_RID_TX_CRYPT_KEY:
		wreq.wi_val[0] = htole16(ic->ic_wep_txkey);
		wreq.wi_len = 1;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		keys = (struct wi_ltv_keys *)&wreq;
		/* do not show keys to non-root user */
		error = suser(curproc, 0);
		if (error) {
			memset(keys, 0, sizeof(*keys));
			error = 0;
			break;
		}
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			keys->wi_keys[i].wi_keylen =
			    htole16(ic->ic_nw_keys[i].wk_len);
			memcpy(keys->wi_keys[i].wi_keydat,
			    ic->ic_nw_keys[i].wk_key, ic->ic_nw_keys[i].wk_len);
		}
		wreq.wi_len = sizeof(*keys) / 2;
		break;
	case WI_RID_MAX_DATALEN:
		wreq.wi_val[0] = htole16(IEEE80211_MAX_LEN);	/* TODO: frag */
		wreq.wi_len = 1;
		break;
	case WI_RID_IFACE_STATS:
		/* XXX: should be implemented in lower drivers */
		break;
	case WI_RID_READ_APS:
		if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
			for (i = 0; i < IEEE80211_PSCAN_WAIT; i++) {
				tsleep((caddr_t)ic, PWAIT | PCATCH, "i80211",
				    hz);
				if (ic->ic_state != IEEE80211_S_SCAN ||
				    (ic->ic_flags & IEEE80211_F_SCANAP) == 0 ||
				    (ic->ic_flags & IEEE80211_F_ASCAN) == 0)
					break;
			}
			ic->ic_flags &= ~IEEE80211_F_SCANAP;
			memcpy(ic->ic_chan_active, ic->ic_chan_avail,
			    sizeof(ic->ic_chan_active));
		}
		i = 0;
		ap = (void *)((char *)wreq.wi_val + sizeof(i));
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if ((caddr_t)(ap + 1) > (caddr_t)(&wreq + 1))
				break;
			memset(ap, 0, sizeof(*ap));
			if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
				IEEE80211_ADDR_COPY(ap->bssid, ni->ni_macaddr);
				ap->namelen = ic->ic_des_esslen;
				if (ic->ic_des_esslen)
					memcpy(ap->name, ic->ic_des_essid,
					    ic->ic_des_esslen);
			} else {
				IEEE80211_ADDR_COPY(ap->bssid, ni->ni_bssid);
				ap->namelen = ni->ni_esslen;
				if (ni->ni_esslen)
					memcpy(ap->name, ni->ni_essid,
					    ni->ni_esslen);
			}
			ap->channel = ni->ni_chan;
			ap->signal = ni->ni_rssi;
			ap->capinfo = ni->ni_capinfo;
			ap->interval = ni->ni_intval;
			for (j = 0; j < ni->ni_nrate; j++) {
				if (ni->ni_rates[j] & IEEE80211_RATE_BASIC) {
					ap->rate = (ni->ni_rates[j] &
					    IEEE80211_RATE_VAL) * 5; /* XXX */
				}
			}
			i++;
			ap++;
		}
		memcpy(wreq.wi_val, &i, sizeof(i));
		wreq.wi_len = (sizeof(int) + sizeof(*ap) * i) / 2;
		break;
#ifdef WICACHE
	case WI_RID_READ_CACHE:
		i = 0;
		TAILQ_FOREACH(ni, &ic->ic_node, ni_list) {
			if (i == MAXWICACHE)
				break;
			IEEE80211_ADDR_COPY(wsc.macsrc, ni->ni_macaddr);
			memset(&wsc.ipsrc, 0, sizeof(wsc.ipsrc));
			wsc.signal = ni->ni_rssi;
			wsc.noise = 0;
			wsc.quality = 0;
			memcpy((caddr_t)wreq.wi_val + sizeof(wsc) * i,
			    &wsc, sizeof(wsc));
			i++;
		}
		wreq.wi_len = sizeof(wsc) * i / 2;
		break;
#endif /* WICACHE */
	case WI_RID_SCAN_APS:
		error = EINVAL;
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == 0) {
		wreq.wi_len++;
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
	}
	return (error);
}

int
ieee80211_cfgset(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	int i, j, len, error;
	struct ifreq *ifr = (struct ifreq *)data;
	struct wi_ltv_keys *keys;
	struct wi_req wreq;
	u_char chanlist[roundup(IEEE80211_CHAN_MAX, NBBY)];

	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return (error);
	if (wreq.wi_len-- < 1)
		return (EINVAL);
	switch (wreq.wi_type) {
	case WI_RID_SERIALNO:
	case WI_RID_NODENAME:
		return (EPERM);
	case WI_RID_CURRENT_SSID:
		return (EPERM);
	case WI_RID_OWN_SSID:
	case WI_RID_DESIRED_SSID:
		len = letoh16(wreq.wi_val[0]);
		if (wreq.wi_len < (1 + len + 1) / 2)
			return (EINVAL);
		if (len > IEEE80211_NWID_LEN)
			return (EINVAL);
		ic->ic_des_esslen = len;
		memset(ic->ic_des_essid, 0, sizeof(ic->ic_des_essid));
		memcpy(ic->ic_des_essid, &wreq.wi_val[1], len);
		error = ENETRESET;
		break;
	case WI_RID_CURRENT_BSSID:
		return (EPERM);
	case WI_RID_OWN_CHNL:
		if (wreq.wi_len != 1)
			return (EINVAL);
		i = letoh16(wreq.wi_val[0]);
		if (i < 0 ||
		    i > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, i))
			return (EINVAL);
		ic->ic_ibss_chan = i;
		if (ic->ic_flags & IEEE80211_F_SIBSS)
			error = ENETRESET;
		break;
	case WI_RID_CURRENT_CHAN:
		return (EPERM);
	case WI_RID_COMMS_QUALITY:
		return (EPERM);
	case WI_RID_PROMISC:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (ifp->if_flags & IFF_PROMISC) {
			if (wreq.wi_val[0] == 0) {
				ifp->if_flags &= ~IFF_PROMISC;
				error = ENETRESET;
			}
		} else {
			if (wreq.wi_val[0] != 0) {
				ifp->if_flags |= IFF_PROMISC;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_PORTTYPE:
		if (wreq.wi_len != 1)
			return (EINVAL);
		switch (letoh16(wreq.wi_val[0])) {
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			if (!(ic->ic_flags & IEEE80211_F_HASIBSS))
				return (EINVAL);
			break;
		case IEEE80211_M_AHDEMO:
			if (ic->ic_phytype != IEEE80211_T_DS ||
			    !(ic->ic_flags & IEEE80211_F_HASAHDEMO))
				return (EINVAL);
			break;
		case IEEE80211_M_HOSTAP:
			if (!(ic->ic_flags & IEEE80211_F_HASHOSTAP))
				return (EINVAL);
			break;
		default:
			return (EINVAL);
		}
		if (letoh16(wreq.wi_val[0]) != ic->ic_opmode) {
			ic->ic_opmode = letoh16(wreq.wi_val[0]);
			error = ENETRESET;
		}
		break;
	case WI_RID_MAC_NODE:
		if (wreq.wi_len != IEEE80211_ADDR_LEN / 2)
			return (EINVAL);
		IEEE80211_ADDR_COPY(LLADDR(ifp->if_sadl), wreq.wi_val);
		/* if_init will copy lladdr into ic_myaddr */
		error = ENETRESET;
		break;
	case WI_RID_TX_RATE:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (wreq.wi_val[0] == 0) {
			/* auto */
			ic->ic_fixed_rate = -1;
			break;
		}
		for (i = 0; i < IEEE80211_RATE_SIZE; i++) {
			if (letoh16(wreq.wi_val[0]) ==
			    (ic->ic_sup_rates[i] & IEEE80211_RATE_VAL) / 2)
				break;
		}
		if (i == IEEE80211_RATE_SIZE)
			return (EINVAL);
		ic->ic_fixed_rate = i;
		error = ENETRESET;
		break;
	case WI_RID_CUR_TX_RATE:
		return (EPERM);
	case WI_RID_RTS_THRESH:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (letoh16(wreq.wi_val[0]) != IEEE80211_MAX_LEN)
			return (EINVAL);	/* TODO: RTS */
		break;
	case WI_RID_CREATE_IBSS:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASIBSS) == 0)
				return (EINVAL);
			if ((ic->ic_flags & IEEE80211_F_IBSSON) == 0) {
				ic->ic_flags |= IEEE80211_F_IBSSON;
				if (ic->ic_opmode == IEEE80211_M_IBSS &&
				    ic->ic_state == IEEE80211_S_SCAN)
					error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_IBSSON) {
				ic->ic_flags &= ~IEEE80211_F_IBSSON;
				if (ic->ic_flags & IEEE80211_F_SIBSS) {
					ic->ic_flags &= ~IEEE80211_F_SIBSS;
					error = ENETRESET;
				}
			}
		}
		break;
	case WI_RID_MICROWAVE_OVEN:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (wreq.wi_val[0] != 0)
			return (EINVAL);	/* not supported */
		break;
	case WI_RID_ROAMING_MODE:
		if (wreq.wi_len != 1)
			return EINVAL;
		if (letoh16(wreq.wi_val[0]) != 1)
			return (EINVAL);	/* not supported */
		break;
	case WI_RID_SYSTEM_SCALE:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (letoh16(wreq.wi_val[0]) != 1)
			return (EINVAL);	/* not supported */
		break;
	case WI_RID_PM_ENABLED:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASPMGT) == 0)
				return (EINVAL);
			if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_MAX_SLEEP:
		if (wreq.wi_len != 1)
			return (EINVAL);
		ic->ic_lintval = letoh16(wreq.wi_val[0]);
		if (ic->ic_flags & IEEE80211_F_PMGTON)
			error = ENETRESET;
		break;
	case WI_RID_CUR_BEACON_INT:
		return (EPERM);
	case WI_RID_WEP_AVAIL:
		return (EPERM);
	case WI_RID_CNFAUTHMODE:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (letoh16(wreq.wi_val[0]) != 1)
			return (EINVAL);	/* TODO: shared key auth */
		break;
	case WI_RID_ENCRYPTION:
		if (wreq.wi_len != 1)
			return (EINVAL);
		if (wreq.wi_val[0] != 0) {
			if ((ic->ic_flags & IEEE80211_F_HASWEP) == 0)
				return (EINVAL);
			if ((ic->ic_flags & IEEE80211_F_WEPON) == 0) {
				ic->ic_flags |= IEEE80211_F_WEPON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_WEPON) {
				ic->ic_flags &= ~IEEE80211_F_WEPON;
				error = ENETRESET;
			}
		}
		break;
	case WI_RID_TX_CRYPT_KEY:
		if (wreq.wi_len != 1)
			return (EINVAL);
		i = letoh16(wreq.wi_val[0]);
		if (i >= IEEE80211_WEP_NKID)
			return (EINVAL);
		ic->ic_wep_txkey = i;
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		if (wreq.wi_len != sizeof(struct wi_ltv_keys) / 2)
			return (EINVAL);
		keys = (struct wi_ltv_keys *)&wreq;
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = letoh16(keys->wi_keys[i].wi_keylen);
			if (len != 0 && len < IEEE80211_WEP_KEYLEN)
				return (EINVAL);
			if (len > sizeof(ic->ic_nw_keys[i].wk_key))
				return (EINVAL);
		}
		memset(ic->ic_nw_keys, 0, sizeof(ic->ic_nw_keys));
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {
			len = letoh16(keys->wi_keys[i].wi_keylen);
			ic->ic_nw_keys[i].wk_len = len;
			memcpy(ic->ic_nw_keys[i].wk_key,
			    keys->wi_keys[i].wi_keydat, len);
		}
		error = ENETRESET;
		break;
	case WI_RID_MAX_DATALEN:
		if (wreq.wi_len != 1)
			return (EINVAL);
		len = letoh16(wreq.wi_val[0]);
		if (len < 350 /* ? */ || len > IEEE80211_MAX_LEN)
			return (EINVAL);
		if (len != IEEE80211_MAX_LEN)
			return (EINVAL);	/* TODO: fragment */
		break;
	case WI_RID_IFACE_STATS:
		error = EPERM;
		break;
	case WI_RID_SCAN_APS:
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
		wreq.wi_len -= 2;	/* XXX: tx rate? */
		/* FALLTHRU */
	case WI_RID_CHANNEL_LIST:
		memset(chanlist, 0, sizeof(chanlist));
		/*
		 * Since channel 0 is not available for DS, channel 1
		 * is assigned to LSB on WaveLAN.
		 */
		if (ic->ic_phytype == IEEE80211_T_DS)
			i = 1;
		else
			i = 0;
		for (j = 0; i <= IEEE80211_CHAN_MAX; i++, j++) {
			if (j / 16 >= wreq.wi_len)
				break;
			if (isclr((u_int8_t *)wreq.wi_val, j))
				continue;
			if (isclr(ic->ic_chan_avail, i)) {
				if (wreq.wi_type == WI_RID_CHANNEL_LIST)
					error = EPERM;
				else
					continue;
			}
			setbit(chanlist, i);
		}
		if (error == EPERM && ic->ic_chancheck != NULL)
			error = (*ic->ic_chancheck)(ic->ic_softc, chanlist);
		if (error)
			return (error);
		memcpy(ic->ic_chan_active, chanlist,
		    sizeof(ic->ic_chan_active));
		if (isclr(chanlist, ic->ic_ibss_chan)) {
			for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
				if (isset(chanlist, i)) {
					ic->ic_ibss_chan = i;
					break;
				}
		}
		if (isclr(chanlist, ic->ic_bss.ni_chan))
			ic->ic_bss.ni_chan = ic->ic_ibss_chan;
		if (wreq.wi_type == WI_RID_CHANNEL_LIST)
			error = ENETRESET;
		else {
			ic->ic_flags |= IEEE80211_F_SCANAP;
			error = ieee80211_new_state(ifp, IEEE80211_S_SCAN, -1);
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
