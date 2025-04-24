import csv
import os
import subprocess
import sys

import yaml

benchmark_config = str(sys.argv[1])
bench_name = str(sys.argv[2])
bench_subname = str(sys.argv[3])

yaml_file = open(benchmark_config)
yaml_data = yaml.load(yaml_file, Loader=yaml.FullLoader)
cluster_data = yaml_data["acc_cluster"]
accelerators = list()
memobjects = list()
for acc in cluster_data:
    accelerators.append(acc)

for params in accelerators:
    for item in params.items():
        for param_type in item:
            if type(param_type) is list:
                for params_list in param_type:
                    if "Var" in params_list.keys():
                        for mem_object in params_list.items():
                            if mem_object[1][0]["Type"] == "SPM":
                                memobjects.append(mem_object[1][0])
                            else:
                                print(
                                    "Streaming Buffer Found - Dumping Parameters"
                                )
                                print(
                                    benchmark_config
                                    + " "
                                    + bench_name
                                    + " "
                                    + bench_subname
                                )
                                print(yaml.dump(mem_object[1][0]))

process = subprocess.Popen(
    "$M5_PATH/tools/cacti-SALAM/clearStats.sh", shell=True
)
process.wait()

memobjectsList = []

if not memobjects:  # Checks if the memobjects list is empty
    print(
        f"No 'Type: SPM' components found in {benchmark_config} for benchmark '{bench_name}'."
    )
    print(
        "Skipping CACTI analysis and results processing for this configuration."
    )
    sys.exit(0)  # Exit the script gracefully (0 = success)

for element in memobjects:
    memobjectsList.append(element["Name"])
    with open(
        os.path.expandvars("$M5_PATH/tools/cacti-SALAM/results/stdout.txt"),
        "a",
    ) as out, open(
        os.path.expandvars("$M5_PATH/tools/cacti-SALAM/results/stderr.txt"),
        "a",
    ) as err:
        # cacti has a lower limit of 2048 for size
        if element["Size"] < 2048:
            element["Size"] = 2048
        size_str = str(element["Size"])
        ports_str = str(element["Ports"])
        cmd_str = (
            f"$M5_PATH/tools/cacti-SALAM/cactiStats.sh {size_str} {ports_str}"
        )
        process = subprocess.Popen(cmd_str, shell=True, stdout=out, stderr=err)
        process.wait()
        out.write(element["Name"] + "(above) \n")
        out.write("/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\ \n\n")

process = subprocess.Popen(
    "$M5_PATH/tools/cacti-SALAM/getcactiResults.sh", shell=True
)
process.wait()

results_csv = list()
with open(
    os.path.expandvars("$M5_PATH/tools/cacti-SALAM/results/out.csv")
) as results_file:
    results = csv.reader(results_file)
    for row in results:
        results_csv.append(row)

results_csv[0].insert(0, "Benchmark")
results_csv[0].insert(1, "Config")
results_csv[0].insert(2, "Acc")

index = 0
for row in results_csv[1:]:
    row.insert(0, str(bench_name))
    row.insert(1, str(bench_subname))
    row.insert(2, str(memobjectsList[index]))
    index = index + 1

salam_out_csv_path = os.path.expandvars(
    "$M5_PATH/tools/cacti-SALAM/results/SALAM-out.csv"
)

if not os.path.exists(salam_out_csv_path):
    results_file = open(salam_out_csv_path, "w+")
    writer = csv.writer(results_file)
    writer.writerow(results_csv[0])
    results_file.close()


with open(salam_out_csv_path, "a") as results_file:
    writer = csv.writer(results_file)
    writer.writerows(results_csv[1:])
