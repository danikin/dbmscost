#include <stdio.h>

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

struct hardware_cost
{
        // Hardware costs in $
        int i_cost_server_body;
        int i_SSD_price;
        int i_spinning_price;
        int i_RAM_unit_price;
};

struct hardware_cost StandardHardwareCost = {2000, 500, 100, 30};

struct hardware_params
{
        int i_SSD_size;
        int i_spinning_size;
        int i_RAM_unit_size;

        // We can't put too many units of RAM to a server because it doesn't have enough slots
        int i_max_RAM_units_per_server; // In units
};

struct hardware_params StandardParams = {500 * 1024, 1000 * 1024, 16 * 1024, 16};

struct other_costs
{
        int i_units_per_rack;
        int i_rack_monthly_price; // $
        int i_units_per_server;
        int i_cost_of_money; // annual bank interest, %
        int i_amortization_period; // a period in months after the price of a server is zero
};

struct other_costs StandardOtherCost = {20, 1000, 2, 3, 36};

struct requirements
{
        // Requirements

        int i_read_qps;
        int i_write_qps;
        int i_write_qps;
        int i_size_of_dataset; // Mb
        int i_number_of_replicas;
        int i_disks_per_RAID;
};

struct requirements ReadWriteHeavyBigDataset = {100000, 50000, 1024 * 1024, 2, 2};
struct requirements ReadHeavyBigDataset = {100000, 100, 1024 * 1024, 2, 2};
struct requirements JustBigDataset = {100, 20, 1024 * 1024, 2, 2};
struct requirements ReadWriteHeavySmallDataset = {100000, 50000, 128 * 1024, 2, 2};
struct requirements ReadHeavySmallDataset = {100000, 100, 128 * 1024, 2, 2};
struct requirements SuperBigSuperHeavy = {3000000, 1000000, 1024 * 1024, 2, 2};

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
};

struct database_system Tarantool = {100000, 100000, 110, 100, 0, 100, 1 * 1024, 256 * 1024};
struct database_system MySQL = {10000, 1000, 130, 0, 100, 10, 32 * 1024, 1024 * 1024};

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
};

void do_calc(const struct hardware_cost *hc,
                const struct hardware_params *hp,
                const struct other_costs *oc,
                const struct requirements *reqs,
                const struct database_system *dbs,
                struct calc_result *res)
{
        // To handle all the reads we must have at least as many servers as our read workload requires with respect to maximum server read capacity
        int number_of_servers_to_handle_reads = ceil_div(reqs->i_read_qps, dbs->i_max_read_qps_per_server);

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

        // This is a bit more complicated, but still easy - you host your servers - you pay money
        res->monthly_cost = ceil_div(res->number_of_servers * oc->i_units_per_server, oc->i_units_per_rack) * oc->i_rack_monthly_price;

        // Grand total per month including upfront cost smashed by months
        res->grand_total_monthly = res->monthly_cost + (res->total_upfront_cost / oc->i_amortization_period) * (100 + oc->i_amortization_period * oc->i_cost_of_money/12) / 100;
}
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

void print_result(struct calc_result *res)
{
        printf("number_of_servers=%d\n\
server is %dxSSD, %dxSATA, %dxRAMxGb, for $%d\n\
total_upfront_cost = $%d\n\
monthly_cost = $%d\n\
grand_total_monthly = $%d\n",
                res->number_of_servers,
                res->number_of_SSD_per_server, res->number_of_spinning_per_server, res->amount_of_RAM_per_server / 1024, res->cost_of_server,
                res->total_upfront_cost,
                res->monthly_cost,
                res->grand_total_monthly);
}

void Tarantool_vs_MySQL(struct requirements *reqs)
{
        printf("Requirements: %d read QPS, %d write QPS, %dGb dataset\n\n", reqs->i_read_qps, reqs->i_write_qps, reqs->i_size_of_dataset / 1024);

        struct calc_result res;
        do_calc(&StandardHardwareCost, &StandardParams, &StandardOtherCost, reqs, &Tarantool, &res);

        printf("Tarantool:\n\n");
        print_result(&res);
        printf("\n");

        do_calc(&StandardHardwareCost, &StandardParams, &StandardOtherCost, reqs, &MySQL, &res);

        printf("MySQL:\n\n");
        print_result(&res);

        printf("\n");
}

int main()
{
        printf("Tarantool: ");
        print_db_params(&Tarantool);

        printf("MySQL:");
        print_db_params(&MySQL);

        printf("\n");

        Tarantool_vs_MySQL(&ReadWriteHeavyBigDataset);
        Tarantool_vs_MySQL(&ReadHeavyBigDataset);
        Tarantool_vs_MySQL(&JustBigDataset);
        Tarantool_vs_MySQL(&ReadWriteHeavySmallDataset);
        Tarantool_vs_MySQL(&ReadHeavySmallDataset);
        Tarantool_vs_MySQL(&SuperBigSuperHeavy);

        return 0;
}


