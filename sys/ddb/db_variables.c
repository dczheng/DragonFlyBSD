/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: src/sys/ddb/db_variables.c,v 1.18 1999/08/28 00:41:11 peter Exp $
 * $DragonFly: src/sys/ddb/db_variables.c,v 1.4 2005/12/23 21:35:44 swildner Exp $
 */

/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <ddb/ddb.h>
#include <ddb/db_lex.h>
#include <ddb/db_variables.h>

static int	db_find_variable (struct db_variable **varp);
static void	db_write_variable (struct db_variable *, db_expr_t *);

#ifdef notused
static int	db_set_variable (db_expr_t value);
#endif

static struct db_variable db_vars[] = {
	{ "radix",	&db_radix, NULL },
	{ "maxoff",	&db_maxoff, NULL },
	{ "maxwidth",	&db_max_width, NULL },
	{ "tabstops",	&db_tab_stop_width, NULL },
};
static struct db_variable *db_evars = 
		db_vars + sizeof(db_vars)/sizeof(db_vars[0]);

static int
db_find_variable(struct db_variable **varp)
{
	int	t;
	struct db_variable *vp;

	t = db_read_token();
	if (t == tIDENT) {
	    for (vp = db_vars; vp < db_evars; vp++) {
		if (!strcmp(db_tok_string, vp->name)) {
		    *varp = vp;
		    return (1);
		}
	    }
	    for (vp = db_regs; vp < db_eregs; vp++) {
		if (!strcmp(db_tok_string, vp->name)) {
		    *varp = vp;
		    return (1);
		}
	    }
	}
	db_error("Unknown variable\n");
	return (0);
}

int
db_get_variable(db_expr_t *valuep)
{
	struct db_variable *vp;

	if (!db_find_variable(&vp))
	    return (0);

	db_read_variable(vp, valuep);

	return (1);
}

#ifdef notused
static int
db_set_variable(db_expr_t value)
{
	struct db_variable *vp;

	if (!db_find_variable(&vp))
	    return (0);

	db_write_variable(vp, &value);

	return (1);
}
#endif

void
db_read_variable(struct db_variable *vp, db_expr_t *valuep)
{
	db_varfcn_t	*func = vp->fcn;

	if (func == NULL)
	    *valuep = *(vp->valuep);
	else
	    (*func)(vp, valuep, DB_VAR_GET);
}

static void
db_write_variable(struct db_variable *vp, db_expr_t *valuep)
{
	db_varfcn_t	*func = vp->fcn;

	if (func == NULL)
	    *(vp->valuep) = *valuep;
	else
	    (*func)(vp, valuep, DB_VAR_SET);
}

void
db_set_cmd(db_expr_t dummy1, boolean_t dummy2, db_expr_t dummy3, char *dummy4)
{
	db_expr_t	value;
	struct db_variable *vp;
	int	t;

	t = db_read_token();
	if (t != tDOLLAR) {
	    db_error("Unknown variable\n");
	    return;
	}
	if (!db_find_variable(&vp)) {
	    db_error("Unknown variable\n");
	    return;
	}

	t = db_read_token();
	if (t != tEQ)
	    db_unread_token(t);

	if (!db_expression(&value)) {
	    db_error("No value\n");
	    return;
	}
	if (db_read_token() != tEOL) {
	    db_error("?\n");
	}

	db_write_variable(vp, &value);
}
