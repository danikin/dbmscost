/*
*	dbcalc_cgi.c Copyright (c) Dennis Anikin 2016
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dbcalc_impl.h"

// Processes cgi input and calls a handler on each name-value pair
void process_cgi_input(void (*handler)(const char *name, const char *value, void *context), void *context)
{
	enum state
	{
		STATE_NAME,
		STATE_VALUE,
		STATE_VALUE_FINISHED
	} state = STATE_NAME;

	char name[256];
	int name_len = 0;

	char value[256];
	int value_len = 0;

	while (1)
	{
		int c = fgetc(stdin);

		// If a name-value pair is finished then handle it
		if (state == STATE_VALUE_FINISHED || (state == STATE_VALUE && c == -1))
		{
			handler(name, value, context);

			// Reset name & value
			name_len = value_len = 0;

			state = STATE_NAME;
		}

		if (c == -1)
			break;

		switch (state)
		{
			case STATE_NAME:
				if (c == '=')
				{
					// Name is finished
					name[name_len] = 0;

					state = STATE_VALUE;
				}
				else
				if (name_len < sizeof(name)-1)
					name[name_len++] = c;
				break;

			case STATE_VALUE:
				if (c == '&')
				{
					// Value is finished
					value[value_len] = 0;

					state = STATE_VALUE_FINISHED;
				}
				else
				if (value_len < sizeof(value)-1)
					value[value_len++] = c;
				break;
			default:
				assert("Unknown state");
				break;
		} // switch (state)
	} // while (1)
}

struct context
{
	struct hardware_cost hc;
	struct hardware_params hp;
	struct other_costs oc;
	struct requirements reqs;
	struct database_system dbs;

	int num_vars;
};

void cgi_handler(const char *name, const char *value, void *c)
{
//	printf("VAR: '%s' -> '%s'\n", name, value);

	struct context *ctx = (struct context *)c;

#define _DBCALC_PROCESS_VAR(v, s)\
	if (!strcmp(name, #v)) { /*printf("MATCH\n"); */ctx->s.v = atoi(value); ++ctx->num_vars; } else

	_DBCALC_PROCESS_VAR(i_cost_server_body, hc)
	_DBCALC_PROCESS_VAR(i_SSD_price, hc)
	_DBCALC_PROCESS_VAR(i_spinning_price, hc)
	_DBCALC_PROCESS_VAR(i_RAM_unit_price, hc)

	_DBCALC_PROCESS_VAR(i_SSD_size, hp)
	_DBCALC_PROCESS_VAR(i_spinning_size, hp)
	_DBCALC_PROCESS_VAR(i_RAM_unit_size, hp)
	_DBCALC_PROCESS_VAR(i_max_RAM_units_per_server, hp)


	_DBCALC_PROCESS_VAR(i_units_per_rack, oc)
	_DBCALC_PROCESS_VAR(i_rack_monthly_price, oc)
	_DBCALC_PROCESS_VAR(i_units_per_server, oc)
	_DBCALC_PROCESS_VAR(i_cost_of_money, oc)
	_DBCALC_PROCESS_VAR(i_amortization_period, oc)

	_DBCALC_PROCESS_VAR(i_read_qps, reqs)
	_DBCALC_PROCESS_VAR(i_write_qps, reqs)
	_DBCALC_PROCESS_VAR(i_size_of_dataset, reqs)
	_DBCALC_PROCESS_VAR(i_number_of_replicas, reqs)
	_DBCALC_PROCESS_VAR(i_disks_per_RAID, reqs)

	_DBCALC_PROCESS_VAR(i_max_read_qps_per_server, dbs)
	_DBCALC_PROCESS_VAR(i_max_write_qps_per_server, dbs)
	_DBCALC_PROCESS_VAR(i_overhead_for_dataset_storing, dbs)
	_DBCALC_PROCESS_VAR(i_data_spinning_ratio, dbs)
	_DBCALC_PROCESS_VAR(i_data_SSD_ratio, dbs)
	_DBCALC_PROCESS_VAR(i_data_RAM_ratio, dbs)

	_DBCALC_PROCESS_VAR(i_min_RAM_amount_per_server, dbs)
	_DBCALC_PROCESS_VAR(i_max_RAM_amount_per_server, dbs)
	_DBCALC_PROCESS_VAR(i_monthly_support_per_server, dbs)
	_DBCALC_PROCESS_VAR(i_monthly_license_fee_per_server, dbs)

	{}

#undef _DBCALC_PROCESS_VAR

}

void print_result(struct calc_result *res,
			struct database_system *dbs)
{

	printf("\n{\n");

#define _DBCALC_PRINT_VAR(v) printf(#v": \"%d\",\n", res->v);

	_DBCALC_PRINT_VAR(number_of_servers)
	_DBCALC_PRINT_VAR(number_of_SSD_per_server)
	_DBCALC_PRINT_VAR(number_of_spinning_per_server)
	_DBCALC_PRINT_VAR(amount_of_RAM_per_server)
	_DBCALC_PRINT_VAR(cost_of_server)
	_DBCALC_PRINT_VAR(total_upfront_cost)
	_DBCALC_PRINT_VAR(monthly_cost)
	_DBCALC_PRINT_VAR(grand_total_monthly)

	printf("read_workload: \"%d\",\n", res->read_qps_per_server*100/dbs->i_max_read_qps_per_server);
	printf("write_workload: \"%d\",\n", res->write_qps_per_server*100/dbs->i_max_write_qps_per_server);
	printf("ram_workload: \"%d\",\n", res->amount_of_RAM_per_server*100/dbs->i_max_RAM_amount_per_server);
	printf("bottle_neck: \"%s\",\n", res->bottle_neck);

#undef _DBCALC_PRINT_VAR

	printf("}\n");
}

int main()
{
	struct calc_result res;
	struct context ctx;
	ctx.num_vars = 0;

	process_cgi_input(cgi_handler, &ctx);

	if (ctx.num_vars < 28)
		fprintf(stderr, "[500] Too little vars: %d. Expected 28\n", ctx.num_vars);
	else
	{
		do_calc(&ctx.hc, &ctx.hp, &ctx.oc, &ctx.reqs, &ctx.dbs, &res);

		print_result(&res, &ctx.dbs);
	}

	return 0;
}

