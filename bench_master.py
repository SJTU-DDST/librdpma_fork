#!/usr/bin/env python3

import re
import os
import json
import time
import select
import argparse
from typing import Dict
from pathlib import Path

import benchlib.color as color
import benchlib.args as benchargs
import benchlib.file as benchfile
import benchlib.session as benchsession

project_name = "librdpma_fork"

bak_dir = Path("./scripts/")
machine_bak_name = "machines.bak"
connection_bak_name = "connections.bak"

config_dir = Path("./configs/")
machine_config_name = "machines.yaml"
connection_config_name = "connections.yaml"

bin_dir = Path("./scripts/")
client_bin_name = "nvm_client"
server_bin_name = "nvm_server"
numa_server_bin_name = "nvm_userver"


def main_benchmark(args):
    machine_config = benchfile.load_config(args.machine)
    connection_config = benchfile.load_config(args.connect)

    cstp_pairs = []
    for i in connection_config:
        if i["enable"]:
            cstp_pairs.append(
                (i["clients"], i["server"], i["thread"], i["payload"], i["corotine"])
            )

    for clients, server, thread_list, payload_list, corotine_list in cstp_pairs:
        this_time_str = benchfile.generate_time_str()
        bench_result_filename = benchfile.generate_bench_result_filename('./', clients, server, this_time_str)
        with open(bench_result_filename, "a+") as output_f:
            output_f.write("[ \n")  # this space is fit for ",\n"

        print(f"{color.CYAN}=== run: server = {server}, clients = {clients} ==={color.RESET}")
        print(
            f"{color.CYAN}===      thread = {thread_list}, coroutine_count = {corotine_list}, payload = {payload_list} ==={color.RESET}"
        )
        print(
            f"{color.CYAN}=== output will all print to {bench_result_filename} ==={color.RESET}"
        )

        if machine_config[server]["user"] == "YOUR_USER_NAME":
            raise ValueError(
                f"Did you forget to change the user in `{config_dir / machine_config_name}`?"
            )
        for i in clients:
            if machine_config[i]["user"] == "YOUR_USER_NAME":
                raise ValueError(
                    f"Did you forget to change the user in `{config_dir / machine_config_name}`?"
                )

        t = -1
        for thread in thread_list:
            t += 1
            c = -1
            for corotine in corotine_list:
                c += 1
                p = -1
                for payload in payload_list:
                    p += 1
                    print(
                        f"{color.PURPLE}run: thread = {thread}, coroutine_count = {corotine}, payload = {payload}{color.RESET}"
                    )

                    server_addr = (
                        f'{machine_config[server]["ip"]}:{machine_config[server]["port"]}'
                    )
                    server_nic_idx = machine_config[server]["available_nic"][0]
                    server_config = {
                        "host": machine_config[server]["ip"],
                        "port": machine_config[server]["port"],
                        "use_nvm": False,
                        "touch_mem": True,
                        "nvm_sz": 2,
                        "use_nic_idx": server_nic_idx,
                    }

                    client_configs = []
                    for name, config in machine_config.items():
                        if name not in clients:
                            continue
                        thread = thread
                        physical_thread_max_num = config["threads_per_socket"] * config["socket_count"]
                        if thread > physical_thread_max_num:
                            print(
                                f"WARNING: {name} thread {thread} > physical_thread_max_num {physical_thread_max_num}"
                            )
                            thread = physical_thread_max_num
                        if args.bench_type == "read":
                            exp_config = benchargs.make_read_experiement_config(
                                thread, corotine, payload
                            )
                        elif args.bench_type == "write":
                            exp_config = benchargs.make_write_experiment_config(
                                thread, corotine, payload
                            )
                        else:
                            assert False, "wrong bench type"
                        thread_config = benchargs.make_thread_config(
                            config["numa_type"], True, 0
                        )

                        client_config = {**exp_config, **thread_config}
                        client_config.update(
                            {
                                "remote_nic_idx": server_nic_idx,
                                "use_nic_idx": config["available_nic"][0],
                                "address_space": 1,
                                "addr": server_addr,
                            }
                        )
                        client_configs.append(client_config)

                    thpt, lat = run_single_testcase(
                        server_name=server,
                        server_args=server_config,
                        client_names=clients,
                        clients_args=client_configs,
                        machine_config=machine_config,
                    )

                    with open(bench_result_filename, "a+") as output_f:
                        result = {
                            "throughput": thpt,
                            "latency": lat,
                            "server": server,
                            "clients": clients,
                            "thread": thread,
                            "corotine": corotine,
                            "payload": payload,
                        }
                        json.dump(result, output_f)
                        output_f.write(",\n")

                    print(
                        f"{color.BLUE}done with thread = {thread}, coroutine_count = {corotine}, payload = {payload}{color.RESET}"
                    )
                    print(f"{color.BLUE}result: throughput = {thpt}, latency = {lat}{color.RESET}")

                    w = 3
                    print(f"--- time.sleep {w} seconds\n\n")
                    time.sleep(w)
                # END for payload in  such as [16, 256, 512, 8192]
            # END for coroutine_count in such as [1, 2, 4, 8, 16]
        # END for thread in such as [1, 36]
           

        with open(bench_result_filename, "rb+") as output_f:
            output_f.seek(-2, os.SEEK_END)
            output_f.truncate()
        with open(bench_result_filename, "a+") as output_f:
            output_f.write("\n]")

    # END for clients, server, thread_list, payload_list, corotine_list in cstp_pairs


def run_single_testcase(
    server_name, server_args, client_names, clients_args, machine_config
):
    # (1) init server

    server_config = machine_config[server_name]

    server_session_config = benchsession.SessionConfig(
        host=server_config["ip"],
        username=server_config["user"],
        password=str(server_config["passwd"]),
    )
    server_session = benchsession.Session(server_session_config)
    server_repo_dir = server_config.get("repo_dir", None)
    if server_repo_dir == "None" or server_repo_dir is None:
        server_repo_dir = f"/home/{server_config['user']}/{project_name}"
    server_binary = str(bin_dir / server_bin_name)
    if server_config["numa_type"] == 3:  # DPU should use userver instead of server
        server_binary = str(bin_dir / numa_server_bin_name)
    server_exec_cmd_list = [
        benchargs.build_sudo_exec_cmd(server_binary, server_args)
    ]
    server_exec_cmd_list.insert(0, f"cd {server_repo_dir}")

    print("setup server session OK")
    if server_config["numa_type"] == 3:
        server_session.execute(f"sudo -S killall {numa_server_bin_name} || true", sudo_S_num=1, password=str(server_config["passwd"]))
    else:
        server_session.execute(f"sudo -S killall {server_bin_name} || true", sudo_S_num=1, password=str(server_config["passwd"]))

    # todo: add a thread to show the server's stdout and stderr
    # note: if you add a thread for its stdout and stderr, please make `quick_show_out=False`
    # stdout, stderr = ...
    _, _ = server_session.execute_many_non_blocking(
        server_exec_cmd_list, sudo_S_num=1, password=str(server_config["passwd"])
    )

    # todo: change to wait for server_session to execute, but seems difficult
    w = 3
    print(
        f"--- will wait [SERVER] for {w} seconds, if it's too short or too long, please change it manually"
    )
    time.sleep(w)

    # (2) init client and print result to stdout/stderr

    client_recv_channels = {}
    num = 0
    for client_name, client_args in zip(client_names, clients_args):
        num += 1
        client_config = machine_config[client_name]
        client_session_config = benchsession.SessionConfig(
            host=client_config["ip"],
            username=client_config["user"],
            password=str(client_config["passwd"]),
        )
        client_session = benchsession.Session(client_session_config)
        client_repo_dir = client_config.get("repo_dir", None)
        if client_repo_dir == "None" or client_repo_dir is None:
            client_repo_dir = f"/home/{client_config['user']}/{project_name}"
        client_binary = str(bin_dir / client_bin_name)
        client_exec_cmd_list = [
            benchargs.build_sudo_exec_cmd(client_binary, client_args)
        ]
        client_exec_cmd_list.insert(0, f"cd {client_repo_dir}")

        print(f"setup client {num} session OK")
        client_session.execute(f"sudo -S killall {client_bin_name} || true", sudo_S_num=1, password=str(client_config["passwd"]))
        stdout, stderr = client_session.execute_many_non_blocking(
            client_exec_cmd_list, quick_show_out=False, sudo_S_num=1, password=str(client_config["passwd"])
        )

        w = 0
        print(
            f"--- will wait [CLIENT] for {w} seconds, if it's too short or too long, please change it manually"
        )
        time.sleep(w)

        # todo: make stderr strip from the stdout
        # this maybe hard to do in select_client_recv_channels()
        stdout.channel.set_combine_stderr(True)
        client_recv_channels[client_name] = stdout

    # (3) deal with stdout/stderr and count latency and throughput

    name_buffer_map = select_client_recv_channels(
        client_recv_channel_map=client_recv_channels
    )
    thpt, lat = aggregate_statics(name_buffer_map)

    # (4) close client and server

    num = 0
    for client_name, client_args in zip(client_names, clients_args):
        num += 1
        client_config = machine_config[client_name]
        client_session_config = benchsession.SessionConfig(
            host=client_config["ip"],
            username=client_config["user"],
            password=str(client_config["passwd"]),
        )
        client_session = benchsession.Session(client_session_config)
        client_session.execute(f"sudo -S killall {client_bin_name} || true", sudo_S_num=1, password=str(server_config["passwd"]))
        print(f"done, close client {num} session OK")

    if server_config["numa_type"] == 3:
        server_session.execute(f"sudo -S killall {numa_server_bin_name} || true", sudo_S_num=1, password=str(server_config["passwd"]))
    else:
        server_session.execute(f"sudo -S killall {server_bin_name} || true", sudo_S_num=1, password=str(server_config["passwd"]))
    server_session.close()
    print("done, close server session OK")

    return thpt, lat


def select_client_recv_channels(client_recv_channel_map: Dict):
    selected_channel_files = [
        channel_file for channel_file in client_recv_channel_map.values()
    ]
    channel_client_map = {
        hash(channel_file.channel): {"name": name, "buffer": ""}
        for name, channel_file in client_recv_channel_map.items()
    }

    # use select to listen to all the client socket file descriptors
    while len(selected_channel_files) > 0:
        time.sleep(1)
        channel_list = [channel_file.channel for channel_file in selected_channel_files]
        if any([channel.recv_ready() for channel in channel_list]):
            read_list, _, _ = select.select(channel_list, [], [], 0.0)
            for c in read_list:
                length = 16384
                print(
                    f"--- will receive up to {length} bytes from stdout, if it's too short or too long, please change it manually"
                )
                recved = c.recv(length).decode("utf-8")
                channel_client_map[hash(c)]["buffer"] += recved
                print(f"Output (stdout & stderr):\n{recved}")
        # remove file discriptors that have closed.
        selected_channel_files = [
            channel_file for channel_file in selected_channel_files if not channel_file.channel.exit_status_ready()
        ]

    return {x["name"]: x["buffer"] for x in channel_client_map.values()}


def aggregate_statics(name_buffer_map: Dict[str, str]):
    name_stats_map = {
        x: parse_buffer_into_statics(y) for (x, y) in name_buffer_map.items()
    }

    thpts = []
    latencys = []

    for stat_list in name_stats_map.values():
        cur_thpt = (
            sum([stat["throughput"] for stat in stat_list]) / len(stat_list)
            if len(stat_list) != 0
            else 0
        )
        cur_latency = (
            sum([stat["latency"] for stat in stat_list]) / len(stat_list)
            if len(stat_list) != 0
            else 0
        )
        thpts.append(cur_thpt)
        latencys.append(cur_latency)

    return sum(thpts), sum(latencys) / len(latencys)


STATICS_PATTERN = re.compile(
    ".*?epoch[ :@\t]*([0-9\+e\.]*)[ \t]*([0-9\+e\.]*)[ :\t]*thpt[ :@\t]*([0-9\+e\.]*)[ \t]*reqs/sec"
)
"""[statucs.hh:78] epoch @ 0 2.02875 : thpt: 1.41981e+06 reqs/sec,"""
"""[statucs.hh:78] epoch @ 8 1.73391 : thpt: 596948 reqs/sec,"""


def parse_buffer_into_statics(buffer: str):
    stats = []
    for line in buffer.split("\n"):
        match_res = STATICS_PATTERN.match(line)
        if match_res:
            epoch = float(match_res.group(1))
            latency = float(match_res.group(2))
            throughput = float(match_res.group(3))
            if epoch >= 5:
                stats.append(
                    {"epoch": epoch, "latency": latency, "throughput": throughput}
                )

    return stats


def arg_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--machine", type=str, default=config_dir / machine_config_name)
    parser.add_argument("-c", "--connect", type=str, default=config_dir / connection_config_name)
    parser.add_argument("-t", "--bench_type", type=str, default="read")
    return parser.parse_args()


def new_config():
    Path(config_dir).mkdir(parents=True, exist_ok=True)
    if os.path.exists(config_dir / machine_config_name) and os.path.exists(config_dir / connection_config_name):
        return
    if not os.path.exists(config_dir / machine_config_name):
        os.system(f"cp {bak_dir / machine_bak_name} {config_dir / machine_config_name}")
    if not os.path.exists(config_dir / connection_config_name):
        os.system(f"cp {bak_dir / connection_bak_name} {config_dir / connection_config_name}")
    print(color.RED)
    print(f"{config_dir / machine_config_name} and {config_dir / connection_config_name} are generated")
    print("please check them, then run this again")
    print(color.RESET)
    exit(0)


if __name__ == "__main__":
    new_config()
    args = arg_parser()
    main_benchmark(args)
