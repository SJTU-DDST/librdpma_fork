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
            res_list = json.load(res)
            for config in connection_config:
                if res_list[0]["server"] == config["server"] and res_list[0]["clients"] == config["clients"]:
                    drawer.reset_one_testcase_dict(
                        server=config["server"],
                        clients=config["clients"],
                        thread_list=config["thread"],
                        payload_list=config["payload"],
                        corotine_list=config["corotine"]
                    )
                    break

            for res_item in res_list:
                drawer.update_one_testcase(
                    thread=res_item["thread"],
                    payload=res_item["payload"],
                    corotine=res_item["corotine"],
                    throughput=res_item["throughput"],
                    latency=res_item["latency"]
                )

            drawer.draw_testcase_pictures()
            print("--- copy it if you need")
            print(drawer.one_testcase_dict)

        drawer.add_one_to_all_list_and_clear_one()

    print("--- copy it if you need")
    print(drawer.all_testcases_dict_list)

    drawer.draw_compare_testcases_pictures()


if __name__ == "__main__":
    figure()
