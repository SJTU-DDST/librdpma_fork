#!/usr/bin/env python3

import json, os
from pathlib import Path

import benchlib.file as benchfile
import benchlib.draw as benchdraw
project_name = "librdpma_fork"

config_dir = Path("./configs/")
machine_config_name = "machines.yaml"
connection_config_name = "connections.yaml"

res_dir = Path("./benchres/")

def figure():
    machine_config = benchfile.load_config(config_dir / machine_config_name)
    connection_config = benchfile.load_config(config_dir / connection_config_name)
    drawer = benchdraw.Drawer()

    for res_file in os.listdir(res_dir):
        print(os.listdir(res_dir))
        with open(res_dir / res_file, "r") as res:
            res_dict = json.load(res)
            for i in connection_config:
                if res_dict[0]["server"] == i["server"] and res_dict[0]["clients"] == i["clients"]:
                    drawer.reset_one_testcase_dict(
                        server=i["server"],
                        clients=i["clients"],
                        thread_count_list= i["thread_count"],
                        payload_list=i["payload"],
                    )
            for res_index, res_item in enumerate(res_dict):
                drawer.update_one_testcase_throughput(threads_index=int(res_index / 11), payload_index=res_index % 11, throughput=res_item["throughput"])
                drawer.update_one_testcase_latency(threads_index=int(res_index / 11), payload_index=res_index % 11, latency=res_item["latency"])
            
            this_time_str = benchfile.generate_time_str()
            cn = ",".join(drawer.one_testcase_dict["clients"])
            drawer.draw_testcase_pictures(f'benchres_{cn}_{drawer.one_testcase_dict["server"]}_{this_time_str}')
            print("--- copy it if you need")
            print(drawer.one_testcase_dict)    
        drawer.add_one_to_all_list_and_clear_one()

    # END for clients, server, thread_count_list, payload_list in cstp_pairs

    print("--- copy it if you need")
    print(drawer.all_testcases_dict_list)

    drawer.draw_compare_testcases_pictures()


if __name__ == "__main__":
    figure()
