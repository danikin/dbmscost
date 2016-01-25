/*
*	Copyright (c) Dennis Anikin 2016
*/

#include <stdio.h>

#include "dbcalc_impl.h"

int ceil_div(int a, int b)
{
        return (a+b-1)/b;
}

int ceil_percent(int a, int b)
{
        return ceil_div(a*b, 100);
}

int max2(int a, int b)
{
        return a > b ? a : b;
}

int max3(int a, int b, int c)
{
        return (a > b ? max2(a, c) : max2(b, c));
}

void do_calc(const struct hardware_cost *hc,
                const struct hardware_params *hp,
                const struct other_costs *oc,
                const struct requirements *reqs,
                const struct database_system *dbs,
                struct calc_result *res)
{
	// To handle all the reads we must have at least as many servers as our read workload requires with respect to maximum server read capacity
	int number_of_servers_to_handle_reads =
		ceil_div(reqs->i_read_qps, dbs->i_max_read_qps_per_server);

        // The same thing to writes
        int number_of_servers_to_handle_writes = ceil_div(reqs->i_write_qps, dbs->i_max_write_qps_per_server);

        // The same thing to the whole workload
        int number_of_servers_to_handle_both_reads_and_writes = ceil_div(reqs->i_read_qps + reqs->i_write_qps, dbs->i_max_read_qps_per_server + dbs->i_max_write_qps_per_server);

        // So we need to handle at least read and write workload separately along with the whole read/write workload
        res->number_of_servers = max3(number_of_servers_to_handle_reads, number_of_servers_to_handle_writes, number_of_servers_to_handle_both_reads_and_writes);

        // Now we need to know what is the capacity of one server (keeping in mind the thing that our data set is times bigger because we need to replicate data for the
        // reason of availability and safety)
        //
        // And by the way, the number of servers must not be less than i_number_of_replicas (if it is then we have to increate it to be at least as big as i_number_of_replicas)

        if (res->number_of_servers < reqs->i_number_of_replicas)
                res->number_of_servers = reqs->i_number_of_replicas;

        int number_of_RAM_units_per_server;

        // This cycle is only because we can increase the number of servers if a server happens to have to much RAM
        while (1)
        {
                int server_capacity = ceil_percent(reqs->i_size_of_dataset, dbs->i_overhead_for_dataset_storing) * reqs->i_number_of_replicas / res->number_of_servers;

                // Now that we have server_capacity (number of gigs per server) we can calculate a cost of a server
                // Starting with resources

                // The main idea about the cost of a server is that a sever must have SSD disks, spinning disks and RAM according to
                // ratio of those resources that our database system is able to use

                res->number_of_SSD_per_server = reqs->i_disks_per_RAID * ceil_div(ceil_percent(server_capacity, dbs->i_data_SSD_ratio), hp->i_SSD_size);
                res->number_of_spinning_per_server = reqs->i_disks_per_RAID * ceil_div(ceil_percent(server_capacity, dbs->i_data_spinning_ratio), hp->i_spinning_size);

                // This is amount of memory (in Mb) per server
                int RAM_per_server = ceil_percent(server_capacity, dbs->i_data_RAM_ratio);

                // Adjust it if it is lower then the mininum for this database
                if (RAM_per_server < dbs->i_min_RAM_amount_per_server)
                        RAM_per_server = dbs->i_min_RAM_amount_per_server;

                number_of_RAM_units_per_server = ceil_div(RAM_per_server, hp->i_RAM_unit_size);

                res->amount_of_RAM_per_server = number_of_RAM_units_per_server * hp->i_RAM_unit_size;

                // If we're using more memory than we can afford or more than can be put to a server then increase a number of servers
                if (number_of_RAM_units_per_server > hp->i_max_RAM_units_per_server ||
                        res->amount_of_RAM_per_server > dbs->i_max_RAM_amount_per_server)
                        res->number_of_servers = max2(
                                res->number_of_servers * ceil_div(number_of_RAM_units_per_server, hp->i_max_RAM_units_per_server),
                                res->number_of_servers * ceil_div(res->amount_of_RAM_per_server, dbs->i_max_RAM_amount_per_server));
                else
                        break;
	}

	res->cost_of_server = hc->i_cost_server_body +
		res->number_of_SSD_per_server * hc->i_SSD_price +
		res->number_of_spinning_per_server * hc->i_spinning_price +
		number_of_RAM_units_per_server * hc->i_RAM_unit_price;

	// This is easy
	res->total_upfront_cost = res->cost_of_server * res->number_of_servers;

	res->read_qps_per_server = reqs->i_read_qps / res->number_of_servers;
	res->write_qps_per_server = reqs->i_write_qps / res->number_of_servers;

	// Determine the bottleneck
	int r_bn = res->read_qps_per_server*100/dbs->i_max_read_qps_per_server;
	int w_bn = res->write_qps_per_server*100/dbs->i_max_write_qps_per_server;
	int max_ram_bn = res->amount_of_RAM_per_server*100/dbs->i_max_RAM_amount_per_server;
	int min_ram_bn = dbs->i_min_RAM_amount_per_server*100/res->amount_of_RAM_per_server;

	if (r_bn > 85 && r_bn >= max3(w_bn, max_ram_bn, min_ram_bn))
		res->bottle_neck = "Database system should handle more reads per second";
	else
	if (w_bn > 85 && w_bn >= max3(r_bn, max_ram_bn, min_ram_bn))
		res->bottle_neck = "Database system should handle more writes per second";
	else
	if (max_ram_bn > 85 && max_ram_bn >= max3(r_bn, w_bn, min_ram_bn))
		res->bottle_neck = "Database system should be able to use more RAM per server";
	else
	if (min_ram_bn > 85 && min_ram_bn >= max3(r_bn, w_bn, max_ram_bn))
		res->bottle_neck = "Database system should be able to use less RAM per server";
	else
		res->bottle_neck = "Database system is only bound by number of replicas requirement";

        // This is a bit more complicated, but still easy - you host your servers - you pay money
        res->monthly_cost = ceil_div(res->number_of_servers * oc->i_units_per_server, oc->i_units_per_rack) * oc->i_rack_monthly_price +
		res->number_of_servers * (dbs->i_monthly_support_per_server + dbs->i_monthly_license_fee_per_server);

        // Grand total per month including upfront cost smashed by months
        res->grand_total_monthly = res->monthly_cost + (res->total_upfront_cost / oc->i_amortization_period) * (100 + oc->i_amortization_period * oc->i_cost_of_money/12) / 100;
}

