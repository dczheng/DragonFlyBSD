# $FreeBSD: src/usr.sbin/yp_mkdb/Makefile,v 1.5.2.1 2001/04/25 12:11:13 ru Exp $

PROG=	yp_mkdb
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c
WARNS?=	3
NO_WCAST_FUNCTION_TYPE=

MAN=	yp_mkdb.8

.PATH:	${.CURDIR}/../../libexec/ypxfr ${.CURDIR}/../ypserv

CFLAGS+= -Dyp_error=warnx -I${.CURDIR}/../../libexec/ypxfr
CFLAGS+= -I${.CURDIR}/../ypserv

.include <bsd.prog.mk>
