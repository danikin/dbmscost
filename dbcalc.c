/*
*	Copyright (c) Dennis Anikin 2016
*/

#include <stdio.h>

#include "dbcalc_impl.h"

// $2000 per body
// $500 per SSD
// $100 per spinning
// $30 per RAM unit
struct hardware_cost StandardHardwareCost = {2000, 500, 100, 30};

// 500Gb SSD
// 1Tb spinning
// 16Gb RAM
// 16 RAM units max per server
struct hardware_params StandardParams = {500 * 1024, 1000 * 1024, 16 * 1024, 16};

// 20 units per rack
// $1000 per rack per month
// 1 units per server
// 3% interest per year
// 36 months amortization period
struct other_costs StandardOtherCost = {20, 1000, 1, 3, 36};

struct requirements ReadWriteHeavyBigDataset = {100000, 50000, 1024 * 1024, 2, 2};
struct requirements ReadHeavyBigDataset = {100000, 100, 1024 * 1024, 2, 2};
struct requirements JustBigDataset = {100, 20, 1024 * 1024, 2, 2};
struct requirements ReadWriteHeavySmallDataset = {100000, 50000, 128 * 1024, 2, 2};
struct requirements ReadHeavySmallDataset = {100000, 100, 128 * 1024, 2, 2};
struct requirements SuperBigSuperHeavy = {3000000, 1000000, 1024 * 1024, 2, 2};

// 100K read QPS, 100K write QPS, 10% data overhead, 100% in RAM and on spinning disk, 1-256Gb RAM
struct database_system Tarantool = {100000, 100000, 110, 100, 0, 100, 1 * 1024, 256 * 1024, 0, 0};
struct database_system TarantoolWithSupport = {100000, 100000, 110, 100, 0, 100, 1 * 1024, 256 * 1024, 1000, 0};

// 10K read QPS, 1K write QPS, 30% data overhead, 10% in RAM and 100% on SSD, 32-1024Gb RAM
struct database_system MySQL = {10000, 1000, 130, 0, 100, 10, 32 * 1024, 1024 * 1024, 0, 0};

// 80K read QPS, 80K write QPS, 25% data overhead, 100% in RAM and on spinning disk, 1-256Gb RAM
struct database_system Redis = {80000, 80000, 125, 100, 0, 100, 1 * 1024, 256 * 1024, 0, 0};
struct database_system RedisWithSupport = {80000, 80000, 125, 100, 0, 100, 1 * 1024, 256 * 1024, 3000, 0};

void print_db_params(struct database_system *dbs)
{
        printf("%d read QPS, %d write QPS, %d%% overhead, %d%% spinning, %d%% SSD, %d%% RAM, %dGb-%dGb RAM per server\n",
                dbs->i_max_read_qps_per_server,
                dbs->i_max_write_qps_per_server,
                dbs->i_overhead_for_dataset_storing - 100,
                dbs->i_data_spinning_ratio,
                dbs->i_data_SSD_ratio,
                dbs->i_data_RAM_ratio,
                dbs->i_min_RAM_amount_per_server / 1024,
                dbs->i_max_RAM_amount_per_server / 1024);
}

void print_result(struct calc_result *res, struct database_system *dbs)
{
        printf("number_of_servers=%d\n\
server is %dxSSD, %dxSATA, %dxRAMxGb, for $%d\n\
total_upfront_cost = $%d\n\
monthly_cost = $%d\n\
grand_total_monthly = $%d\n\
read workload = %d%%, write workload = %d%%, RAM workload = %d%%\n\
Bottleneck: %s\n",
                res->number_of_servers,
                res->number_of_SSD_per_server, res->number_of_spinning_per_server, res->amount_of_RAM_per_server / 1024, res->cost_of_server,
                res->total_upfront_cost,
                res->monthly_cost,
                res->grand_total_monthly,
		res->read_qps_per_server*100/dbs->i_max_read_qps_per_server,
		res->write_qps_per_server*100/dbs->i_max_write_qps_per_server,
		res->amount_of_RAM_per_server*100/dbs->i_max_RAM_amount_per_server,
		res->bottle_neck);
}

struct comparison
{
	const char *name;
	struct database_system *dbs;
};

void compare(struct requirements *reqs, struct comparison *c, int n)
{
        printf("\
------------------------------------------------------------\n\
Requirements: %d read QPS, %d write QPS, %dGb dataset\n\
------------------------------------------------------------\n\n\
", reqs->i_read_qps, reqs->i_write_qps, reqs->i_size_of_dataset / 1024);

	struct calc_result res;

	// Calculate all the database systems in the comparison
	for (int i = 0; i < n; ++i)
	{
		do_calc(&StandardHardwareCost, &StandardParams, &StandardOtherCost, reqs, c[i].dbs, &res);

		printf("%s:\n\n", c[i].name);
		print_result(&res, c[i].dbs);
		printf("\n");
	}
}

int main()
{
	// Fell the structure with rivals
	struct comparison comparisons[] = {
		{"Tarantool", &Tarantool},
		{"Tarantool with support", &TarantoolWithSupport},
		{"Redis", &Redis},
		{"Redis with support", &RedisWithSupport},
		{"MySQL", &MySQL}
	};

	int n_comp = sizeof(comparisons)/sizeof(struct comparison);

	// Print the name of all the database systems
	for (int i = 0; i < n_comp; ++i)
	{
		printf("%s: ", comparisons[i].name);
		print_db_params(&Tarantool);
	}
	printf("\n");

	// Compare the database systems with all the workloads
        compare(&ReadWriteHeavyBigDataset, comparisons, n_comp);
        compare(&ReadHeavyBigDataset, comparisons, n_comp);
        compare(&JustBigDataset, comparisons, n_comp);
        compare(&ReadWriteHeavySmallDataset, comparisons, n_comp);
        compare(&ReadHeavySmallDataset, comparisons, n_comp);
        compare(&SuperBigSuperHeavy, comparisons, n_comp);

        return 0;
}

