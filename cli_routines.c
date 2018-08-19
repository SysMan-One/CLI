/*
 * Argument's types
 */
#define	CLI$K_P1	1	/* Position depended parameters	*/
#define	CLI$K_P2	2
#define	CLI$K_P3	3

#define	CLI$K_P8	8

#define	CLI$K_QUAL	4	/* Qualifier*/

#define	CLI$K_FILE	1
#define	CLI$K_DATE	2
#define	CLI$K_NUM	3
#define	CLI$K_IPV4	4
#define	CLI$K_IPV6	5
#define	CLI$K_OPT	7
#define	CLI$K_QSTRING	8
#define	CLI$K_UUID	9

#define	CLI$K_KWD	0xA

typedef	struct __ascic__	{
	unsigned char	len,
			sts[255];
} ASCIC;

typedef	struct __cli_param__	{
	unsigned	pn;	/* P1, P2, ... P8		*/
	unsigned	type;	/* FILE, DATE ...		*/
	ASCIC		alias;	/* Alias of the parameter	*/
} CLI_PARAM;

typedef	struct __cli_qual__	{
	ASCIC		name;	/* Qualifier name		*/
	unsigned	type;	/* FILE, DATE ...		*/
} CLI_QUAL;

typedef	struct __cli_opt__	{
	ASCIC		name;	/* Qualifier's options name	*/
	unsigned	type;	/* FILE, DATE ...		*/
} CLI_OPT;


typedef	struct __cli__verb__	{
	ASCIC	name;
	CLI_PARAM	params;
	CLI_QUAL	quals;
	int	action_routine	(...);

} CLI_VERB;

/*
 CRYPTOR SHOW	VOLUME	<p1-volume_name>/UUID=<uuid>
		USER	<p1-user-name>	/GROUP=(ADMINS, NETWORKS)
		VM	<p1-VM-name>	/UUID=<uuid> /GROUP=()

		DIFF	<p1-file-name> <p2-file-name> /BLOCK=(START=<lbn>, END=<lbn>, COUNT=<lbn>)
			/IGNORE=(ERROR) /LOGGING=(FULL, TRACE, ERROR)
*/

CLI_QUAL	diff_quals [] = {
	{ {13, "BLOCK"}, CLI$K_KWD },
	{ {13, "IGNORE"}, CLI$K_KWD },
	{ {13, "LOGGING"}, CLI$K_KWD },
	{0}};

CLI_PARAM	diff_params [] = {
	{CLI$K_P1, CLI$K_FILE, {13, "Input file 1"} },
	{CLI$K_P2, CLI$K_FILE, {13, "Input file 2"} },

	{0}};

CLI_VERB	diff_verb = { {13, "diff"}, diff_params, diff_quals };


int	cli_parse(...);
int	cli_dispatch(...);
int	cli_get_value ();
int	cli_kwd2val();


#include <stdio.h>

ASCIC	prompt = {13, "CRYPTORCP>"};


int main()
{
	printf("Hello World!\n");
	return 0;
}
