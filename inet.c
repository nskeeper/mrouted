/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 */

#include "defs.h"

/*
 * Exported variables.
 */
char s1[MAX_INET_BUF_LEN];		/* buffers to hold the string representations  */
char s2[MAX_INET_BUF_LEN];		/* of IP addresses, to be passed to inet_fmt() */
char s3[MAX_INET_BUF_LEN];		/* or inet_fmts().                             */
char s4[MAX_INET_BUF_LEN];


/*
 * Verify that a given IP address is credible as a host address.
 * (Without a mask, cannot detect addresses of the form {subnet,0} or
 * {subnet,-1}.)
 */
int inet_valid_host(u_int32 naddr)
{
    u_int32 addr;

    addr = ntohl(naddr);

    return (!(IN_MULTICAST(addr) ||
	      IN_BADCLASS (addr) ||
	      (addr & 0xff000000) == 0));
}

/*
 * Verify that a given netmask is plausible;
 * make sure that it is a series of 1's followed by
 * a series of 0's with no discontiguous 1's.
 */
int inet_valid_mask(u_int32 mask)
{
    if (~(((mask & -mask) - 1) | mask) != 0) {
	/* Mask is not contiguous */
	return FALSE;
    }

    return TRUE;
}

/*
 * Verify that a given subnet number and mask pair are credible.
 *
 * With CIDR, almost any subnet and mask are credible.  mrouted still
 * can't handle aggregated class A's, so we still check that, but
 * otherwise the only requirements are that the subnet address is
 * within the [ABC] range and that the host bits of the subnet
 * are all 0.
 */
int inet_valid_subnet(u_int32 nsubnet, u_int32 nmask)
{
    u_int32 subnet, mask;

    subnet = ntohl(nsubnet);
    mask   = ntohl(nmask);

    if ((subnet & mask) != subnet) return FALSE;

    if (subnet == 0)
	return mask == 0;

    if (IN_CLASSA(subnet)) {
	if (mask < 0xff000000 ||
	    (subnet & 0xff000000) == 0x7f000000 ||
	    (subnet & 0xff000000) == 0x00000000) return FALSE;
    }
    else if (IN_CLASSD(subnet) || IN_BADCLASS(subnet)) {
	/* Above Class C address space */
	return FALSE;
    }
    if (subnet & ~mask) {
	/* Host bits are set in the subnet */
	return FALSE;
    }
    if (!inet_valid_mask(mask)) {
	/* Netmask is not contiguous */
	return FALSE;
    }

    return TRUE;
}


/*
 * Convert an IP address in u_long (network) format into a printable string.
 */
char *inet_fmt(u_int32 addr, char *s, size_t len)
{
    u_char *a;

    a = (u_char *)&addr;
    snprintf(s, len, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);

    return s;
}


/*
 * Convert an IP subnet number in u_long (network) format into a printable
 * string including the netmask as a number of bits.
 */
char *inet_fmts(u_int32 addr, u_int32 mask, char *s, size_t len)
{
    u_char *a, *m;
    int bits;

    if ((addr == 0) && (mask == 0)) {
	snprintf(s, len, "default");
	return s;
    }
    a = (u_char *)&addr;
    m = (u_char *)&mask;
    bits = 33 - ffs(ntohl(mask));

    if      (m[3] != 0) snprintf(s, len, "%u.%u.%u.%u/%d", a[0], a[1], a[2], a[3],
						bits);
    else if (m[2] != 0) snprintf(s, len, "%u.%u.%u/%d",    a[0], a[1], a[2], bits);
    else if (m[1] != 0) snprintf(s, len, "%u.%u/%d",       a[0], a[1], bits);
    else                snprintf(s, len, "%u/%d",          a[0], bits);

    return s;
}

/*
 * Convert the printable string representation of an IP address into the
 * u_long (network) format.  Return 0xffffffff on error.  (To detect the
 * legal address with that value, you must explicitly compare the string
 * with "255.255.255.255".)
 */
u_int32 inet_parse(char *s, int n)
{
    u_int32 a = 0;
    u_int a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int i;
    char c;

    i = sscanf(s, "%u.%u.%u.%u%c", &a0, &a1, &a2, &a3, &c);
    if (i < n || i > 4 || a0 > 255 || a1 > 255 || a2 > 255 || a3 > 255)
	return 0xffffffff;

    ((u_char *)&a)[0] = a0;
    ((u_char *)&a)[1] = a1;
    ((u_char *)&a)[2] = a2;
    ((u_char *)&a)[3] = a3;

    return a;
}


/*
 * inet_cksum extracted from:
 *			P I N G . C
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *	Public Domain.  Distribution Unlimited.
 *
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int inet_cksum(u_int16_t *addr, u_int len)
{
	int nleft = (int)len;
	u_int16_t *w = addr;
	u_int16_t answer = 0;
	int32_t sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *) (&answer) = *(u_char *)w ;
		sum += answer;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */

	return answer;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "ellemtel"
 *  c-basic-offset: 4
 * End:
 */
