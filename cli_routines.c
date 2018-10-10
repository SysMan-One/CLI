#define	__MODULE__	"CLI_ROUTINES"
#define	__IDENT__	"X.00-01"

#ifdef	__GNUC__
	#ident			__IDENT__

	#pragma GCC diagnostic ignored  "-Wparentheses"
	#pragma	GCC diagnostic ignored	"-Wdate-time"
	#pragma	GCC diagnostic ignored	"-Wunused-variable"
	#pragma GCC diagnostic ignored	"-Wmissing-braces"
#endif

#ifdef __cplusplus
    extern "C" {
#define __unknown_params ...
#define __optional_params ...
#else
#define __unknown_params
#define __optional_params ...
#endif

/*
**++
**
**  FACILITY:  Command Language Interface (CLI) Routines
**
**  ABSTRACT: A portable API to implement command line parameters parsing, primary syntax checking,
**	and dispatching of processing. This true story is based on the OpenVMS CDU/CLI facilities.
**
**  DESCRIPTION: Command Language Interface (CLI) Routines - an API is supposed to be used to parse and
**	dispatch processing.
**
**	This set of routines help to implement a follows syntax:
**
**	<verb> [p1 p2 ... p8] [/qualifiers[=<value> ...]
**
**	...
**
**  DESIGN ISSUE:
**	{tbs}
**
**  AUTHORS: Ruslan R. Laishev (RRL)
**
**  CREATION DATE:  22-AUG-2018
**
**  MODIFICATION HISTORY:
**
**--
*/

#include	<string.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<errno.h>

/*
* Defines and includes for enable extend trace and logging
*/
#define		__FAC__	"CLI_RTNS"
#define		__TFAC__ __FAC__ ": "		/* Special prefix for $TRACE			*/
#include	"utility_routines.h"

#define $SHOW_PARM(name, value, format)	$IFTRACE(q_trace, ": " #name " = " format, (value))
#define $SHOW_PTR(var)			$SHOW_PARM(var, var, "%p")
#define $SHOW_STR(var)			$SHOW_PARM(var, (var ? var : "UNDEF(NULL)"), "'%s'")
#define $SHOW_INT(var)			$SHOW_PARM(var, ((int) var), "%d")
#define $SHOW_UINT(var)			$SHOW_PARM(var, ((unsigned) var), "%u")
#define $SHOW_ULL(var)			$SHOW_PARM(var, ((unsigned long long) var), "%llu")
#define $SHOW_UNSIGNED(var)		$SHOW_PARM(var, var, "0x%08x")
#define $SHOW_BOOL(var)			$SHOW_PARM(var, (var ? "ENABLED(TRUE)" : "DISABLED(FALSE)"), "%s");

/*
 * Parameters (P1 - P8)
 */
#define	CLI$K_P1	1	/* Position depended parameters	*/
#define	CLI$K_P2	2
#define	CLI$K_P3	3
#define	CLI$K_P4	4
#define	CLI$K_P5	5
#define	CLI$K_P6	6
#define	CLI$K_P7	7
#define	CLI$K_P8	8

#define	CLI$K_QUAL	4	/* Qualifier*/

				/* Parameter/Value - types	*/
#define	CLI$K_FILE	1
#define	CLI$K_DATE	2
#define	CLI$K_NUM	3	/* Numbe: Octal, Decimal, Hex	*/
#define	CLI$K_IPV4	4	/* 212.129.97.4			*/
#define	CLI$K_IPV6	5
#define	CLI$K_OPT	7	/* Options - no value		*/
#define	CLI$K_QSTRING	8	/* Quoted string		*/
#define	CLI$K_UUID	9	/* Fucken UUID			*/
#define	CLI$K_DEVICE	0xa	/* A device in the /dev/	*/

#define	CLI$K_KWD	0xb	/* A predefined keyword		*/


#define	CLI$M_NEGATABLE	1	/* Qualifier can be negatable	*/
#define	CLI$M_LIST	2
#define	CLI$M_PRESENT	4

#define	CLI$S_MAXVERBL	32	/* Maximum verb's length	*/


typedef	struct __ascic__	{
	unsigned char	len,
			sts[255];
} ASCIC;

#define	$ASCNIL		0,0

typedef	struct __cli_keyword__	{
	ASCIC		name;	/* Keyword's string itself	*/
	unsigned long long val;	/* Associated value		*/
} CLI_KEYWORD;

typedef	struct __cli_param__	{
	ASCIC		name;	/* A short name of the parameter*/
	unsigned	type;	/* FILE, DATE ...		*/
	unsigned	flag;	/* */

	ASCIC		defval;	/* Default value string		*/

	CLI_KEYWORD *	kwd;	/* A list of keywords		*/

	unsigned	pn;	/* P1, P2, ... P8		*/

} CLI_PARAM;

typedef	struct __cli_qual__	{
	ASCIC		name;	/* Qualifier name		*/
	unsigned	type;	/* FILE, DATE ...		*/
	unsigned	flag;	/* */

	ASCIC		defval;	/* Default value string		*/

	CLI_KEYWORD *	kwd;	/* A list of keywords		*/
} CLI_QUAL;

typedef	struct __cli__verb__	{
	ASCIC		name;

	struct __cli__verb__ *next;

	CLI_PARAM	*params;
	CLI_QUAL	*quals;

	int	(*act_rtn) (__unknown_params);
	void	*act_arg;

} CLI_VERB;

typedef	struct	__cli_avp__{
	struct	__cli_avp__ *link;/* Double-linked list stuff	*/

	unsigned	type;	/* 0 - verb, P1 - P8, QUAL	*/

	union	{
		CLI_VERB	*verb;
				/* An address of the CLI's	*/
				/* parameters or qualifiers	*/
				/* definition structure		*/
		CLI_PARAM	*param;
		CLI_QUAL	*qual;
	};

	ASCIC		val;	/* Value string			*/

} CLI_AVP;

/*
 *	A context to keep has been parsed command line:
 *		verb parameters, qualifiers
 */

/* Processing options		*/
#define	CLI$M_OPTRACE	1
#define	CLI$M_OPSIGNAL	2

typedef struct __cli_ctx__
{
	int	opts;

	CLI_AVP		*vlist;	/* A list of verbs' sequence for
				  a command			*/
	CLI_AVP		*avlist;	/* A list of parameters' values
				  and qualifiers		*/
} CLI_CTX;


static	char *cli$val_type	(
			int	valtype
		)
{
	switch (valtype)
		{
		case	CLI$K_FILE:	return	"FILE (./filename.ext, device:\\path\\filename.ext)";
		case	CLI$K_DATE:	return	"DATE (dd-mm-yyyy[-hh:mm:ss])";
		case	CLI$K_NUM:	return	"DIGIT (decimal, octal, hex)";
		case	CLI$K_IPV4:	return	"IPV4 (aa.bb.cc.dd)";
		case	CLI$K_IPV6:	return	"IPV6";
		case	CLI$K_OPT:	return	"OPTION (no value)";
		case	CLI$K_QSTRING:	return	"ASCII string in double quotes";
		case	CLI$K_UUID:	return	"UUID ( ... )";
		case	CLI$K_KWD:	return	"KEYWORD";
		}

	return	"ILLEGAL";
}


static	int	cli$add_item2ctx	(
		CLI_CTX		*clictx,
		int		type,
		void		*item,
		char		*val
			)
{
CLI_AVP	*avp, *avp2;

	/* Allocate memory for new CLI's param/qual value entry */
	if ( !(avp = calloc(1, sizeof(CLI_AVP))) )
		{
		return	(clictx->opts & CLI$M_OPSIGNAL)
			? $LOG(STS$K_ERROR, "Insufficient memory, errno=%d", errno)
			: STS$K_ERROR;
		}

	/* Store a given item: parameter or qualifier into the context */
	avp->type = type;
	__util$str2asc (val, &avp->val);

	if ( !type )
		{
		avp->verb = item;

		/* Run over list down to last item */
		for ( avp2 = clictx->vlist; avp2 && avp2->link; avp2 = avp2->link);

		/* Insert new item into the CLI context list */
		if ( avp2 )
			avp2->link = avp;
		else	clictx->vlist = avp;

		return	STS$K_SUCCESS;
		}

	if ( CLI$K_QUAL == type )
		avp->qual = item;
	else	avp->param =  item;

	/* Run over list down to last item */
	for ( avp2 = clictx->avlist; avp2 && avp2->link; avp2 = avp2->link);

	/* Insert new item into the CLI context list */
	if ( avp2 )
		avp2->link = avp;
	else	clictx->avlist = avp;

	return	STS$K_SUCCESS;
}

void	cli$show_verbs	(
	CLI_VERB	*verbs,
		int	level
		)
{
CLI_VERB *verb, *subverb;
CLI_PARAM *par;
CLI_KEYWORD *kwd;
CLI_QUAL *qual;
char	spaces [64];

	memset(spaces, ' ', sizeof(spaces));

	for ( verb = verbs; $ASCLEN(&verb->name); verb++)
		{
		if ( !level )
			$LOG(STS$K_INFO, "Show verb '%.*s' : ", $ASC(&verb->name));
		else	$LOG(STS$K_INFO, " %.*s%.*s", level * 2, spaces, $ASC(&verb->name));

		/* Run over command's verbs list ... */
		for ( subverb = verb->next; subverb; subverb = subverb->next)
			cli$show_verbs(verb->next, ++level);

		for ( par = verb->params; par && par->pn; par++)
			{
			$LOG(STS$K_INFO, "   P%d - '%.*s' (%s)", par->pn, $ASC(&par->name), cli$val_type (par->type));

			if ( par->kwd )
				{
				for ( kwd = par->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#x ", $ASC(&kwd->name), kwd->val);
				}
			}


		for ( qual = verb->quals; qual && $ASCLEN(&qual->name); qual++)
			{
			$LOG(STS$K_INFO, "   /%.*s (%s)", $ASC(&qual->name), cli$val_type (qual->type) );

			if ( qual->kwd )
				{
				for ( kwd = qual->kwd; $ASCLEN(&kwd->name); kwd++)
					$LOG(STS$K_INFO, "      %.*s=%#x ", $ASC(&kwd->name), kwd->val);
				}
			}

		}
}


static void	cli$show_ctx	(
		CLI_CTX	*	clictx
		)
{
CLI_AVP	*avp;
char	spaces [64];
int	splen = 0;

	memset(spaces, ' ', sizeof(spaces));

	$LOG(STS$K_INFO, "CLI-parser context area");


	/* Run over command's verbs list ... */
	for ( splen = 2, avp = clictx->vlist; avp; avp = avp->link, splen += 2)
		$LOG(STS$K_INFO, "%.*s %.*s  ('%.*s')", splen, spaces, $ASC(&avp->verb->name), $ASC(&avp->val));

	for ( avp = clictx->avlist; avp; avp = avp->link)
		{
		if ( avp->type != CLI$K_QUAL )
			$LOG(STS$K_INFO, "   P%d[0:%d]='%.*s'", avp->param->pn, $ASCLEN(&avp->val), $ASC(&avp->val));
		else	$LOG(STS$K_INFO, "   /%.*s[0:%d]='%.*s'", $ASC(&avp->qual->name), $ASCLEN(&avp->val), $ASC(&avp->val));
		}
}

/*
 *
 *  DESCRIPTION: check a given string against a table of keywords,
 *	detect ambiguous input:
 *		ST	- is conflicted with START & STOP
 *	allow not full length-comparing:
 *		SH	- is matched to  SHOW
 *
 *  INPUT:
 *	sts:	a keyword string to be checked
 *	opts:	processing options, see CLI$M_OP*
 *	klist:	an address of the keyword table, null entry terminated
 *
 *  OUTPUT:
 *	kwd:	an address to accept a pointer to keyword's record
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	cli$check_keyword	(
			char	*sts,
			int	 len,
		int		opts,
		CLI_KEYWORD	*klist,
		CLI_KEYWORD **	kwd
		)
{
CLI_KEYWORD	*krun, *ksel;
int	qlog = opts & CLI$M_OPTRACE;

	*kwd = NULL;

	for ( krun = klist; $ASCLEN(&krun->name); krun++)
		{
		if ( len > $ASCLEN(&krun->name) )
			continue;

		$IFTRACE(qlog, "Match [0:%d]='%.*s' against '%.*s'", len, len, sts, $ASC(&krun->name) );

		if ( !strncasecmp(sts, $ASCPTR(&krun->name), $MIN(len, $ASCLEN(&krun->name))) )
			{
			if ( ksel )
				{
				return	(opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, sts, $ASC(&krun->name), $ASC(&ksel->name))
					: STS$K_FATAL;
				}

			/* Safe has been matched keyword's record */
			ksel = krun;
			}
		}

	if ( ksel )
		{
		*kwd = ksel;
		return	STS$K_SUCCESS;
		}

	return	(opts & CLI$M_OPSIGNAL)
		? $LOG(STS$K_ERROR, "Illegal or unrecognized keyword '%.*s'", len, sts)
		: STS$K_ERROR;
}

/*
 *
 *  DESCRIPTION: extract a params list from the command line corresponding to definition
 *		of the param list from the 'verb'
 *
 *  INPUT:
 *	verbs:	verb's definition structure
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  IMPLICIT OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	cli$parse_quals(
	CLI_CTX	*	clictx,
	CLI_VERB	*verb,
		int	argc,
		char ** argv
			)
{
CLI_QUAL	*qrun, *qsel = NULL;
int		status, len, i, qlog = clictx->opts & CLI$M_OPTRACE;
char		*aptr, *vptr;

	/*
	 *  Run over arguments from command line
	 */
	for ( qsel = NULL, aptr = argv[i = 0]; argc; argc--, i++,  aptr = argv[i] )
		{
		if ( (*aptr == '-') || (*aptr == '/') )
			aptr++;
		else	continue;

		/* Is there '=' and vlue ? */
		if ( vptr = strchr(aptr, '=') )
			len = vptr - aptr;
		else	len = strnlen(aptr, ASC$K_SZ);

		for ( qsel = NULL, qrun = verb->quals; $ASCLEN(&qrun->name); qrun++  )
			{
			if ( len > $ASCLEN(&qrun->name) )
				continue;

//			$IFTRACE(qlog, "Match [0:%d]='%.*s' against '%.*s'", len, len, aptr, $ASC(&qrun->name) );

			if ( !strncasecmp(aptr, $ASCPTR(&qrun->name), len) )
				{
				if ( qsel )
					{
					return	(clictx->opts & CLI$M_OPSIGNAL)
						? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, aptr, $ASC(&qrun->name), $ASC(&qsel->name))
						: STS$K_FATAL;
					}

				vptr	+= (vptr != NULL);
				$IFTRACE(qlog, "%.*s='%s'", $ASC(&qrun->name), vptr);

				status = cli$add_item2ctx(clictx, CLI$K_QUAL, qrun, vptr);

				qsel = qrun;
				}
			}
		}

	return	STS$K_SUCCESS;
}

/*
 *
 *  DESCRIPTION: extract a params list from the command line corresponding to definition
 *		of the param list from the 'verb, add param/value pair into the 'CLI-context' area.
 *
 *  INPUT:
 *	clictx:	A CLI-context to be created
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	cli$parse_params(
	CLI_CTX	*	clictx,
	CLI_VERB	*verb,
		int	argc,
		char ** argv
			)
{
CLI_PARAM	*param;
int		status, pi, qlog = clictx->opts & CLI$M_OPTRACE;

	/*
	 * Run firstly over parameters list and extract values
	 */
	for ( pi = 0, param = verb->params; param && (pi < argc) && param->pn; param++, pi++ )
		{
		$IFTRACE(qlog, "P%d(%.*s)='%s'", param->pn, $ASC(&param->name), argv[pi] );

		/* Put parameter's value into the CLI context */
		if ( !(1 & (status = cli$add_item2ctx (clictx, param->pn, param, argv[pi]))) )
			return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Error inserting P%d - %.*s into CLI context area", param->pn, $ASC(&param->name)) : STS$K_FATAL;
		}

	/* Did we get all parameters ? */
	if ( param && param->pn )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Missing P%d - %.*s !", param->pn, $ASC(&param->name)) : STS$K_FATAL;

	/* Now we can extract qualifiers ... */
	status = cli$parse_quals(clictx, verb, argc - pi, argv + pi);

	return	status;
}



/*
 *
 *  DESCRIPTION: parsing input list of arguments by using a command's verbs definition is provided by 'verbs'
 *		syntax definition. Internaly performs command verb matching and calling other CLI-routines to parse 'parameters'
 *		and 'qualifiers'.
 *		Create a CLI-context area is supposed to be used by cli$get_value() routine to extratct a parameter value.

 *  INPUT:
 *	verbs:	commands' verbs definition structure, null entry terminated
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
static	int	cli$parse_verb	(
	CLI_CTX		*clictx,
	CLI_VERB *	verbs,
		int	argc,
		char ** argv
			)
{
CLI_VERB	*vrun, *vsel;
int		status, len, qlog = clictx->opts & CLI$M_OPTRACE;
char		*pverb;

	$IFTRACE(qlog, "argc=%d", argc);

	/*
	 * Sanity check for input arguments ...
	 */
	if ( argc < 1 )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Too many arguments") : STS$K_FATAL;


	pverb = argv[0];

	/*
	 * First at all we need to match  argv[1] in the verbs list
	 */
	len = strnlen(pverb, CLI$S_MAXVERBL);

	$IFTRACE(qlog, "argv[1]='%s'->[0:%d]='%.*s'", pverb, len, len, pverb);

	for (vrun = verbs, vsel = NULL; $ASCLEN(&vrun->name); vrun++)
		{
		if ( len > $ASCLEN(&vrun->name) )
			continue;

		$IFTRACE(qlog, "Matching '%.*s' vs '%.*s' ... ", len, pverb, $ASC(&vrun->name));

		/*
		 * Match verb from command line against given verbs table,
		 * we are comparing at minimal length, but we must checks for ambiguous verb's definitions like:
		 *
		 * SET will match verbs: SET & SETUP
		 * DEL will match: DELETE & DELIVERY
		 *
		 */
		if ( !strncasecmp(pverb, $ASCPTR(&vrun->name), $MIN(len, $ASCLEN(&vrun->name))) )
			{
			$IFTRACE(qlog, "Matched on length=%d '%.*s' := '%.*s' !", $MIN(len, $ASCLEN(&vrun->name)), len, pverb, $ASC(&vrun->name));

			/* Check that there is not previous matches */
			if ( vsel )
				{
				return	(clictx->opts & CLI$M_OPSIGNAL)
					? $LOG(STS$K_FATAL, "Ambiguous input '%.*s' (matched to : '%.*s', '%.*s')", len, pverb, $ASC(&vrun->name), $ASC(&vsel->name))
					: STS$K_FATAL;
				}

			/* Safe matched verb for future checks */
			vsel = vrun;
			}
		}

	/* Found something ?*/
	if ( !vsel )
		return	(clictx->opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Unrecognized command verb '%s.*'", len, pverb) : STS$K_FATAL;

	/*
	 * Ok, we has got in 'vsel' a legal verb, so
	 * so we will now matching arguments from 'argv' against
	 * the verb's parameters and qualifiers list
	 */
	/* Insert new item into the CLI context list */
	cli$add_item2ctx (clictx, 0, vsel, pverb);

	/* Is there a next subverb ? */
	if ( vsel->next )
		status = cli$parse_verb	(clictx, vsel, argc - 1, argv++);
	else	{
		if ( ( !vsel->params) && (!vsel->quals) )
			return	STS$K_SUCCESS;

		status = cli$parse_params(clictx, vsel, argc - 1, argv + 1);
		}

	return	status;
}


/*
 *
 *  DESCRIPTION: a top level routine -as main entry for the CLI parsing.
 *		Create a CLI-context area is supposed to be used by cli$get_value() routine to extratct a parameter value.

 *  INPUT:
 *	verbs:	commands' verbs definition structure, null entry terminated
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
int	cli$parse	(
	CLI_VERB *	verbs,
	int		opts,
		int	argc,
		char ** argv,
		void **	clictx
			)
{
int	status, qlog = opts & CLI$M_OPTRACE;
CLI_CTX	*ctx;

	$IFTRACE(qlog, "argc=%d, opts=%#x", argc, opts);

	/*
	 * Sanity check for input arguments ...
	 */
	if ( argc < 1 )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Too many arguments") : STS$K_FATAL;

	/* Create CLI-context area */
	if ( !(*clictx = calloc(1, sizeof(CLI_CTX))) )
		return	(opts & CLI$M_OPSIGNAL) ? $LOG(STS$K_FATAL, "Cannot allocate memory, errno=%d", errno) : STS$K_FATAL;
	ctx = *clictx;
	ctx->opts = opts;


	status = cli$parse_verb(*clictx, verbs, argc, argv);

	return	STS$K_SUCCESS;
}

/*
 *
 *  DESCRIPTION: parsing input list of arguments by using a command's verbs definition is provided by 'verbs'
 *		syntax definition. Internaly performs command verb matching and calling other CLI-routines to parse 'parameters'
 *		and 'qualifiers'.
 *		Create a CLI-context area is supposed to be used by cli$get_value() routine to extratct a parameter value.

 *  INPUT:
 *	verbs:	commands' verbs definition structure, null entry terminated
 *	opts:	processing options, see CLI$M_OP*
 *	argc:	arguments count
 *	argv:	arguments array
 *
 *  OUTPUT:
 *	ctx:	A CLI-context to be created
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
int	cli$get_value	(
		CLI_CTX	*clictx,
		int	opts,
		ASCIC	*val
			)
{
	/* Sanity check */
}


/*
 *
 *  DESCRIPTION: release resources has been allocated by cli$parse() routine.
 *
 *  INPUT:
 *	ctx:	A CLI-context has been created by cli$parse()
 *
 *  RETURN:
 *	SS$_NORMAL, condition status
 *
 */
int	cli$cleanup	(
		CLI_CTX	*clictx
		)
{

CLI_AVP	*avp, *avp2;

	for (avp = clictx->vlist; avp; )
		{
		avp2 = avp;
		avp = avp->link;
		free(avp2);
		}

	for (avp = clictx->avlist; avp; )
		{
		avp2 = avp;
		avp = avp->link;
		free(avp2);
		}

	free(clictx);

	return	STS$K_SUCCESS;
}


#ifdef	__CLI_DEBUG__

#include	<stdio.h>
#include	<errno.h>


/*
	SHOW	VOLUME	<p1-volume_name>/UUID=<uuid> /FULL
		USER	<p1-user-name>  /FULL /GROUP=<user_group_name>
		VM	<p1-VM-name>	/FULL /GROUP=<vm_group_name>

	DIFF	<p1-file-name> <p2-file-name> /BLOCK=(START=<lbn>, END=<lbn>, COUNT=<lbn>)
			/IGNORE=(ERROR) /LOGGING=(FULL, TRACE, ERROR)
*/

enum	{
	SHOW$K_VOLUME = 1,
	SHOW$K_USER,
	SHOW$K_VM
};


enum	{
	DIFF$K_FULL = 1,
	DIFF$K_TRACE = 1,
	DIFF$K_ERROR = 1
};

CLI_KEYWORD	diff_log_opts[] = {
			{ .name = {$ASCINI("FULL")},	.val = DIFF$K_FULL},
			{ .name = {$ASCINI("TRACE")},	.val = DIFF$K_TRACE},
			{ .name = {$ASCINI("ERROR")},	.val = DIFF$K_ERROR},
			{0}},

		show_what_opts [] = {
			{ .name = {$ASCINI("VOLUME")},	.val = SHOW$K_VOLUME},
			{ .name = {$ASCINI("USER")},	.val = SHOW$K_USER},
			{ .name = {$ASCINI("VM")},	.val = SHOW$K_VM},
			{0}};

CLI_QUAL	diff_quals [] = {
			{ .name = {$ASCINI("START")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("END")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("COUNT")},	.type = CLI$K_NUM},
			{ .name = {$ASCINI("IGNORE")},	.type = CLI$K_OPT},
			{ .name = {$ASCINI("LOGGING")}, .type = CLI$K_KWD, .kwd = diff_log_opts},
			{0}},

		show_volume_quals [] = {
			{ .name = {$ASCINI("UUID")},	CLI$K_UUID},
			{ .name = {$ASCINI("FULL")},	CLI$K_OPT},
			{0}},
		show_user_quals [] = {
			{ .name = {$ASCINI("GROUP")},	CLI$K_QSTRING},
			{ .name = {$ASCINI("FULL")},	CLI$K_OPT},
			{0}},
		show_vm_quals [] = {
			{ .name = {$ASCINI("GROUP")},	CLI$K_QSTRING},
			{ .name = {$ASCINI("FULL")},	CLI$K_OPT},
			{0}};

CLI_PARAM	diff_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_FILE, .name = {$ASCINI("Input file 1")} },
			{.pn = CLI$K_P2, .type = CLI$K_FILE, .name = {$ASCINI("Input file 2")} },
			{0}},

		show_volume_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_DEVICE, .name = {$ASCINI("Volume name (eg: sdb, sdb1, sda5: )")} },
			{0}},

		show_user_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_QSTRING,  .name = {$ASCINI("User spec")}},
			{0}},

		show_vm_params [] = {
			{.pn = CLI$K_P1, .type = CLI$K_QSTRING,  .name = {$ASCINI("VM Id")}},
			{0}};


int	diff_action	( void *clictx, void *arg);
int	show_action	( void *clictx, void *arg);

CLI_VERB	show_what []  = {
	{ {$ASCINI("volume")},	.params = show_volume_params, .quals = show_volume_quals , .act_rtn = show_action, .act_arg = SHOW$K_VOLUME },
	{ {$ASCINI("vm")},	.params = show_user_params, .quals = show_vm_quals,  .act_rtn = show_action, .act_arg = SHOW$K_VM  },
	{ {$ASCINI("user")},	.params = show_vm_params, .quals = show_user_quals , .act_rtn = show_action, .act_arg = SHOW$K_USER  },
	{0}};


CLI_VERB	top_commands []  = {
	{ .name = {$ASCINI("diff")}, .params = diff_params, .quals = diff_quals , .act_rtn = diff_action  },
	{ .name = {$ASCINI("show")}, .next = show_what},
	{0}};

ASCIC	prompt = {$ASCINI("CRYPTORCP>")};


int	diff_action	( void *clictx, void *arg)
{

	return	STS$K_SUCCESS;
}

int	show_action	( void *clictx, void *arg)
{
int	what = (int) arg, status;
ASCIC	val;


	switch	( what )
		{
		case	SHOW$K_VOLUME:
			break;

		case	SHOW$K_USER:
			break;

		case	SHOW$K_VM:
			break;

		default:
			$LOG(STS$K_FATAL, "Error processing SHOW command");
		}

	return	STS$K_SUCCESS;
}

int main	(int	argc, char **argv)
{
int	status;
void	*clictx = NULL;

	/* Dump to screen verbs definitions */
	cli$show_verbs (top_commands, 0);

	/* Process command line arguments */
	if ( !(1 & (status = cli$parse (top_commands, CLI$M_OPTRACE | CLI$M_OPSIGNAL, argc - 1, argv + 1, &clictx))) )
		return	-EINVAL;

	/* Show  a result of the parsing */
	cli$show_ctx (clictx);

	/* Process command line arguments */
//	if ( !(1 & (status = cli$dispatch (clictx, CLI$M_OPTRACE | CLI$M_OPSIGNAL))) )
//		return	-EINVAL;

	/* Release hass been allocated resources */
	cli$cleanup(clictx);

	return 0;
}


#ifdef __cplusplus
    }
#endif
#endif	/* __CLI_DEBUG__ */
