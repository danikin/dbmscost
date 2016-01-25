/*
*	Copyright (c) Dennis Anikin 2016
*/

#ifndef _dbcalc_impl_h_included_
#define _dbcalc_impl_h_included_

struct hardware_cost
{
        // Hardware costs in $

	// The cost of a server except RAM and disks
        int i_cost_server_body;

	// The costs of RAM & disks per unit
        int i_SSD_price;
        int i_spinning_price;
        int i_RAM_unit_price;
};

struct hardware_params
{
	// Disks & RAM sizes in Mb
        int i_SSD_size;
        int i_spinning_size;
        int i_RAM_unit_size;

        // We can't put too many units of RAM to a server because it doesn't have enough slots
        int i_max_RAM_units_per_server; // In units
};

struct other_costs
{
        int i_units_per_rack;
        int i_rack_monthly_price; // $
        int i_units_per_server;
        int i_cost_of_money; // annual bank interest, %
        int i_amortization_period; // a period in months after the price of a server is zero

	// Todo:
	// Additional units incurred by disks (the more disks the more units per server)
};

struct requirements
{
        // Requirements

        int i_read_qps;
        int i_write_qps;
        int i_size_of_dataset; // Mb
        int i_number_of_replicas;
        int i_disks_per_RAID;
};

struct database_system
{
        int i_max_read_qps_per_server;
        int i_max_write_qps_per_server;
        int i_overhead_for_dataset_storing; // %, >=100
        int i_data_spinning_ratio; // %
        int i_data_SSD_ratio; // %
        int i_data_RAM_ratio; // %

        // We can't have to little RAM per server because otherwise it won't work at all (i.e. a database system has its fixed RAM cost)
        int i_min_RAM_amount_per_server; // In Mb

        // We can't have to much RAM per server because for some databases (in-memory) is would mean a long start
        // Note: it differs from i_max_RAM_units_per_server because i_max_RAM_units_per_server is the server physycal limit, but
        // i_max_RAM_amount_per_server is a database system limit
        int i_max_RAM_amount_per_server; // In Mb

	// Monthly support cost per server
	int i_monthly_support_per_server;

	// Monthly license fee per server
	int i_monthly_license_fee_per_server;
};

struct calc_result
{
        int number_of_servers;
        int number_of_SSD_per_server;
        int number_of_spinning_per_server;
        int amount_of_RAM_per_server;

        int cost_of_server;
        int total_upfront_cost;
        int monthly_cost;
        int grand_total_monthly;

	int read_qps_per_server;
	int write_qps_per_server;

	const char *bottle_neck;
};

void do_calc(const struct hardware_cost *hc,
                const struct hardware_params *hp,
                const struct other_costs *oc,
                const struct requirements *reqs,
                const struct database_system *dbs,
                struct calc_result *res);

#endif

