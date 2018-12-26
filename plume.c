#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ibase.h>

#define PLUME_VERSION "0.1"

#define ERREXIT(status, rc)	{isc_print_status(status); return rc;}

#define INDEX_LEN	32
#define INDEX_MAX	20000

#define SQL_VARCHAR(len) struct {short vary_length; char vary_string[(len)+1];}

char *isc_database;
char *isc_user;
char *isc_password;

char idx_list[INDEX_MAX][INDEX_LEN];

short goodbye = 0;

void help(char *name)
{
    printf("Usage: %s [options]\n"
           "Avaliable options:\n"
           "\t-h help\n"
           "\t-v version\n"
           "\t-d database connections string\n"
           "\t-u username\n"
           "\t-p password\n"
           , name);
}

void version()
{
    printf("Plume version " PLUME_VERSION " \n");
}

int parse(int argc, char *argv[])
{
    char *opts = "hvd:u:p:";
    int opt;
    while((opt = getopt(argc, argv, opts)) != -1)
    {
        switch(opt)
        {
        case 'h':
            help(argv[0]);
            goodbye = 1;
            break;
        case 'v':
            version();
            goodbye = 1;
            break;
        case 'd':
            isc_database = optarg;
            break;
        case 'u':
            isc_user = optarg;
            break;
        case 'p':
            isc_password = optarg;
            break;
        }
    }
    return 0;
}

int get_index_list()
{
	//Database var
	char			    dpb_buffer[256], *dpb;
	short		    	dpb_length = 0L;
	isc_db_handle		db = 0L;
	isc_tr_handle		trans = 0L;
	isc_stmt_handle		stmt = 0L;
	ISC_STATUS_ARRAY	status;
	XSQLDA			    *sqlda;
	long			    fetch_stat;
	char			    *sel_str =
		"select RDB$INDEX_NAME from RDB$INDICES where RDB$SYSTEM_FLAG<>1 and RDB$INDEX_INACTIVE=1 order by RDB$FOREIGN_KEY;";
	SQL_VARCHAR(INDEX_LEN)	idx_name;
	short		        flag0 = 0;

	int idx_num = 0;

    //Some magic...
	dpb = dpb_buffer;
	*dpb++ = isc_dpb_version1;
	*dpb++ = isc_dpb_num_buffers;
	*dpb++ = 1;
	*dpb++ = 90;
	dpb_length = dpb - dpb_buffer;

	dpb = dpb_buffer;
	isc_expand_dpb(&dpb, (short *) &dpb_length,
		isc_dpb_user_name, isc_user,
		isc_dpb_password, isc_password,
		NULL);

	//Connect to database
	if (isc_attach_database(status, 0, isc_database, &db, dpb_length, dpb))
	{
		ERREXIT(status, 1);
	}

	//start transaction
	if (isc_start_transaction(status, &trans, 1, &db, 0, NULL))
	{
		isc_print_status(status);
	}

	/* Allocate an output SQLDA. */
	sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH(1));
	sqlda->sqln = 1;
	sqlda->sqld = 1;
	sqlda->version = 1;

	//Create statement
	if (isc_dsql_allocate_statement(status, &db, &stmt))
	{
		ERREXIT(status, 1);
	}

	//Prepare statement
	if (isc_dsql_prepare(status, &trans, &stmt, 0, sel_str, 1, sqlda))
	{
		ERREXIT(status, 1);
	}

	sqlda->sqlvar[0].sqldata = (char *)&idx_name;
	sqlda->sqlvar[0].sqltype = SQL_VARYING + 1;
	sqlda->sqlvar[0].sqlind  = &flag0;

	//Execute statement
	if (isc_dsql_execute(status, &trans, &stmt, 1, NULL))
	{
		ERREXIT(status, 1);
	}

	//Примечание: если использовать как строку, тогда терминировать '\0'
	while ((fetch_stat = isc_dsql_fetch(status, &stmt, 1, sqlda)) == 0)
	{
		memcpy(idx_list[idx_num], idx_name.vary_string, idx_name.vary_length);
		idx_list[idx_num][INDEX_LEN-1] = '\0';
		idx_num++;
		if (idx_num == INDEX_MAX) {
            break;
		}
	}

	if (fetch_stat != 100L)
	{
		ERREXIT(status, 1);
	}

	//Free statement
	if (isc_dsql_free_statement(status, &stmt, DSQL_close))
	{
		ERREXIT(status, 1)
	}

	//Commit transaction
	if (isc_commit_transaction(status, &trans))
	{
		ERREXIT(status, 1);
	}

	//Close
	if (isc_detach_database(status, &db))
	{
		ERREXIT(status, 1);
	}

	free(sqlda);

    return 0;
}

int activate_index()
{
	//Database var
	char			    dpb_buffer[256], *dpb;
	short		    	dpb_length = 0L;
	isc_db_handle		db = 0L;
	isc_tr_handle		trans = 0L;
	ISC_STATUS_ARRAY	status;
	char			    upd_str[256] = "";

	int idx_num = 0;

    //Some magic... i can't understand this magic yet
	dpb = dpb_buffer;
	*dpb++ = isc_dpb_version1;
	*dpb++ = isc_dpb_num_buffers;
	*dpb++ = 1;
	*dpb++ = 90;
	dpb_length = dpb - dpb_buffer;

	dpb = dpb_buffer;
	isc_expand_dpb(&dpb, (short *) &dpb_length,
		isc_dpb_user_name, isc_user,
		isc_dpb_password, isc_password,
		NULL);

	//Connect to database
	if (isc_attach_database(status, 0, isc_database, &db, dpb_length, dpb))
	{
		ERREXIT(status, 1);
	}

    while((*idx_list[idx_num]) && (idx_num < INDEX_MAX))
    {
        printf("Activate index %s\n", idx_list[idx_num]);

        //start transaction
        trans = 0L;
        if (isc_start_transaction(status, &trans, 1, &db, 0, NULL))
        {
            isc_print_status(status);
        }

        //did not master the parameterized query
        strcpy(upd_str, "alter index ");
        strcat(upd_str, idx_list[idx_num]);
        strcat(upd_str, " active;");

        isc_dsql_exec_immed2(status, &db, &trans, 0, upd_str, SQL_DIALECT_CURRENT, NULL, NULL);
        isc_print_status(status);

        //Commit transaction
        if (isc_commit_transaction(status, &trans))
        {
            ERREXIT(status, 1);
        }
        idx_num++;
    }

	//Close datatabase attac
	if (isc_detach_database(status, &db))
	{
		ERREXIT(status, 1);
	}

    return 0;
}

int main(int argc, char *argv[])
{
    int err;

    //Parse arguments
	if (argc == 1)
	{
		help(argv[0]);
		return 0;
	}

	parse(argc, argv);

	if (goodbye == 1)
	{
		return 0;
	}

	//Validate arg

	//Get index list
    err = get_index_list();

    //Activate index
    err = activate_index();

    return err;
}
