import yaml
import argparse
from bench_utils.session import SessionConfig, Session
from typing import List, Dict
from time import sleep
import select
import re
import os
from datetime import datetime
import json


def make_read_experiement_config(threads, coros, payload):
    return {
        "random": True,
        "doorbell": False,
        "two_qp": False,
        "add_sync": False,
        "read_write": False,
        "use_read": True,
        "threads": threads,
        "coros": coros,
        "payload": payload
    }


def make_write_experiment_config(threads, coros, payload):
    return {
        "random": True,
        "doorbell": False,
        "two_qp": False,
        "add_sync": False,
        "read_write": False,
        "use_read": False,
        "threads": threads,
        "coros": coros,
        "payload": payload
    }


def make_thread_config(numa_type, force_use_numa_node, use_numa_node):
    return {
        'numa_type': numa_type,
        'force_use_numa_node': force_use_numa_node,
        "use_numa_node": use_numa_node
    }


def make_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--machine", type=str, default="configs/machines.yaml")
    parser.add_argument("-c", "--connect", type=str, default="configs/connections.yaml")
    parser.add_argument("-t", "--bench_type", type=str, default="read")
    return parser.parse_args()


def load_config(config_file_path: str) -> Dict:
    with open(config_file_path, "r") as config_file:
        config_yaml = yaml.load(config_file, yaml.Loader)
        return config_yaml


def build_exec_cmd(binary, cmd_dict: Dict):
    return binary + ' ' + ' '.join([f"--{k}={v}" for k, v in cmd_dict.items()])


def select_client_recv_channels(client_recv_channel_map: Dict):
    selected_channels = [channel['stdout'] for channel in client_recv_channel_map.values()]
    
    channel_client_map = {
        hash(c['stdout'].channel): {
            'name': name,
            'buffer': ""
        }
        for name, c in client_recv_channel_map.items()
    }
    # use select to listen to all the client socket file descriptors
    while len(selected_channels) > 0:
        sleep(1)
        channel_list = [c.channel for c in selected_channels]
        if any([c.recv_ready() for c in channel_list]):
            rl, _, _ = select.select(channel_list, [], [], 0.0)
            for c in rl:
                length = 16384
                print(f"--- will receive up to {length} bytes from stdout, if it's too short or too long, please change it manuually")
                recved = c.recv(length).decode('utf-8')
                channel_client_map[hash(c)]['buffer'] = channel_client_map[hash(c)]['buffer'] + recved
                print(f"Output (stdout):\n{recved}")
        # remove file discriptors that have closed.
        selected_channels = [c for c in selected_channels if not c.channel.exit_status_ready()]
        
    return {x['name']: x['buffer'] for x in channel_client_map.values()}


STATICS_PATTERN = re.compile(".*?epoch[ :@\t]*([0-9\+e\.]*)[ \t]*([0-9\+e\.]*)[ :\t]*thpt[ :@\t]*([0-9\+e\.]*)[ \t]*reqs/sec")
"""[statucs.hh:78] epoch @ 0 2.02875 : thpt: 1.41981e+06 reqs/sec,"""
"""[statucs.hh:78] epoch @ 8 1.73391 : thpt: 596948 reqs/sec,"""

def parse_buffer_into_statics(buffer: str):
    stats = []
    for line in buffer.split('\n'):
        match_res = STATICS_PATTERN.match(line)
        if match_res:
            epoch = float(match_res.group(1))
            latency = float(match_res.group(2))
            throughput = float(match_res.group(3))
            if epoch >= 5:
                stats.append({
                    'epoch': epoch,
                    "latency": latency,
                    "throughput": throughput
                })

    return stats


def aggregate_statics(name_buffer_map: Dict[str, str]):
    name_stats_map = {
        x: parse_buffer_into_statics(y) for (x, y) in name_buffer_map.items()
    }
    
    thpts = []
    latencys = []
    
    for stat_list in name_stats_map.values():
        cur_thpt = sum([stat['throughput'] for stat in stat_list]) / len(stat_list) if len(stat_list) != 0 else 0
        cur_latency = sum([stat['latency'] for stat in stat_list]) / len(stat_list) if len(stat_list) != 0 else 0
        thpts.append(cur_thpt)
        latencys.append(cur_latency)
        
    return sum(thpts), sum(latencys) / len(latencys)


def run(server_name, server_args, client_names, clients_args, machine_config):

    # (1) init server

    server_config = machine_config[server_name]

    server_session_config = SessionConfig(
        host=server_config['ip'],
        username=server_config['user'], 
        password=str(server_config['passwd'])
    )
    server_session = Session(server_session_config)
    server_repo_dir = server_config.get("repo_dir", None)
    if server_repo_dir == "path/to/librdpma_fork" or server_repo_dir == "None" or server_repo_dir is None:
        server_repo_dir = f"/home/{server_config['user']}/librdpma_fork"
    server_binary = './scripts/nvm_server'
    if server_config['numa_type'] == 3: # DPU should use userver instead of server
        server_binary = "./scripts/nvm_userver"
    server_exec_cmd_list = [build_exec_cmd(server_binary, server_args)]
    server_exec_cmd_list.insert(0, f"cd {server_repo_dir}")
    
    print("setup server session OK")
    if server_binary.endswith("nvm_server"):
        server_session.execute("killall nvm_server || true")
    else:
        server_session.execute("killall nvm_userver || true")
    stdout, stderr = server_session.execute_many_non_blocking(server_exec_cmd_list)
    
    # todo: change to wait for server_session to execute, but seems difficult
    w = 5
    print(f"--- will wait [SERVER] for {w} seconds, if it's too short or too long, please change it manuually")
    sleep(w)
    
    # (2) init client and print result to stdout/stderr

    client_recv_channels = {}
    num = 0
    for (client_name, client_args) in zip(client_names, clients_args):
        num += 1
        client_config = machine_config[client_name]
        client_session_config = SessionConfig(
            host=client_config['ip'],
            username=client_config['user'],
            password=str(client_config['passwd'])
        )
        client_session = Session(client_session_config)
        client_repo_dir = client_config.get("repo_dir", None)
        if client_repo_dir == "path/to/librdpma_fork" or client_repo_dir == "None" or client_repo_dir is None:
            client_repo_dir = f"/home/{client_config['user']}/librdpma_fork"
        client_exec_cmd_list = [build_exec_cmd( "./scripts/nvm_client", client_args)]
        client_exec_cmd_list.insert(0, f"cd {client_repo_dir}")
        
        print(f'setup client {num} session OK')
        client_session.execute("killall nvm_client || true")
        stdout, stderr = client_session.execute_many_non_blocking(client_exec_cmd_list)

        w = 0
        print(f"--- will wait [CLIENT] for {w} seconds, if it's too short or too long, please change it manuually")
        sleep(w)

        client_recv_channels[client_name] = {
            "stdout": stdout,
            "stderr": stderr
        }
    
    # (3) deal with stdout/stderr and count latency and throughput

    name_buffer_map = select_client_recv_channels(client_recv_channel_map=client_recv_channels) 
    thpt, lat = aggregate_statics(name_buffer_map)

    # (4) close client and server

    num = 0
    for (client_name, client_args) in zip(client_names, clients_args):
        num += 1
        client_config = machine_config[client_name]
        client_session_config = SessionConfig(
            host=client_config['ip'],
            username=client_config['user'],
            password=str(client_config['passwd'])
        )
        client_session = Session(client_session_config)
        client_session.execute("killall nvm_client || true")
        print(f'done, close client {num} session OK')

    if server_binary.endswith("nvm_server"):
        server_session.execute("killall nvm_server || true")
    else:
        server_session.execute("killall nvm_userver || true")
    server_session.close()
    print('done, close server session OK')

    return thpt, lat


def generate_bench_result_filename():
    now = datetime.now()
    cur = now.strftime(r"%Y-%m-%d-%H-%M-%S-%f")
    return f"benchres_{cur}.json"


def main():
    args = make_args()
    machine_config = load_config(args.machine)
    connection_config = load_config(args.connect)

    cstp_pairs = []
    for i in connection_config:
        if i["enable"]:
            cstp_pairs.append((i["clients"], i["server"], i["thread_count"], i["payload"]))

    # for each testcase pair, generate one .json and one picture

    for clients, server, thread_count_list, payload_list in cstp_pairs:

        this_time_filename = generate_bench_result_filename()

        print(f"\033[96m=== run: server = {server}, clients = {clients} ===\033[0m")
        print(f"\033[96m=== output will all print to {this_time_filename} ===\033[0m")

        if machine_config[server]["user"] == "YOUR_USER_NAME":
            raise ValueError("Did you forget to change the user in `./configs/machines.ymal`?")
        for i in clients:
            if machine_config[i]["user"] == "YOUR_USER_NAME":
                raise ValueError("Did you forget to change the user in `./configs/machines.ymal`?")
        
        result_dist = {
            'throughput': [],
            'latency': [],
            'threads': [],
            'coros': [],
            'payload': [],
            'server': server,
            'clients': clients,
        }

        for thread_count in thread_count_list:
            for payload in payload_list:

                print(f"\033[91mrun: thread_count = {thread_count}, payload = {payload}\033[0m")

                server_addr = f'{machine_config[server]["ip"]}:{machine_config[server]["port"]}'
                server_nic_idx = machine_config[server]['available_nic'][0]
                server_config = {
                    "host": {machine_config[server]["ip"]},
                    "port": {machine_config[server]["port"]},
                    "use_nvm": False,
                    "touch_mem": True,
                    "nvm_sz": 1,
                    "use_nic_idx": server_nic_idx
                }
                
                client_configs = []
                for name, config in machine_config.items():
                    if name not in clients:
                        continue
                    threads = thread_count
                    if threads > config['threads_per_socket']:
                        print(f"WARNING: {name} threads {threads} > config.threads_per_socket {config['threads_per_socket']}")
                        threads = config['threads_per_socket']
                    coros = 1
                    if args.bench_type == 'read':
                        exp_config = make_read_experiement_config(threads, coros, payload)
                    elif args.bench_type == 'write':
                        exp_config = make_write_experiment_config(threads, coros, payload)
                    else:
                        assert False, "wront bench type"
                    thread_config = make_thread_config(config['numa_type'], True, 0)
                    
                    client_config = {**exp_config, **thread_config}
                    client_config.update({
                        "remote_nic_idx": server_nic_idx,
                        "use_nic_idx": config['available_nic'][0],
                        "address_space": 1,
                        "addr": server_addr,
                    })
                    client_configs.append(client_config)
                
                thpt, lat = run(
                    server_name=server, 
                    server_args=server_config, 
                    client_names=clients, 
                    clients_args=client_configs, 
                    machine_config=machine_config
                )

                result_dist["throughput"].append(thpt)
                result_dist["latency"].append(lat)
                result_dist["threads"].append(threads)
                result_dist["coros"].append(coros)
                result_dist["payload"].append(payload)

                with open(this_time_filename, "a+") as output_f:
                    result = {
                        'throughput': thpt,
                        'latency': lat,
                        'server': server,
                        'clients': clients,
                        'threads': threads,
                        'coros': coros,
                        'payload': payload
                    }
                    json.dump(result, output_f)
                
                print(f"\033[94mdone with thread_count = {thread_count}, payload = {payload}\033[0m")
                print(f"\033[94mresult: throughput = {thpt}, latency = {lat}\033[0m")

                w = 3
                print(f"--- sleep {w} seconds\n\n")
                sleep(w)

            # for thread_count in [1, 36]:
        # for payload in [16, 256, 512, 8192]:

        # todo: use this to draw pictures
        print(result_dist)

    # for clients, server in clients_server_pairs:


def new_config():
    if os.path.exists("./configs/machines.yaml") and os.path.exists("./configs/connections.yaml"):
        return
    if not os.path.exists("./configs/machines.yaml"):
        os.system("cp ./scripts/machines.bak ./configs/machines.yaml")
    if not os.path.exists("./configs/connections.yaml"):
        os.system("cp ./scripts/connections.bak ./configs/connections.yaml")
    print("\033[91m")
    print("./configs/machines.yaml and ./configs/connections.yaml are generated")
    print("please check them, then run this again")
    print("\033[0m")
    exit(0)


if __name__ == '__main__':
    new_config()
    main()
