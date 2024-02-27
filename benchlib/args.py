def make_read_experiement_config(threads, coros, payload) -> dict:
    return {
        "random": True,
        "doorbell": False,
        "two_qp": False,
        "add_sync": False,
        "read_write": False,
        "use_read": True,
        "threads": threads,
        "coros": coros,
        "payload": payload,
    }


def make_write_experiment_config(threads, coros, payload) -> dict:
    return {
        "random": True,
        "doorbell": False,
        "two_qp": False,
        "add_sync": False,
        "read_write": False,
        "use_read": False,
        "threads": threads,
        "coros": coros,
        "payload": payload,
    }


def make_thread_config(numa_type, force_use_numa_node, use_numa_node = 0) -> dict:
    numa_dict = {
        "numa_type": numa_type,
        "force_use_numa_node": force_use_numa_node,
    }
    if force_use_numa_node:
        numa_dict["use_numa_node"] = use_numa_node
    return numa_dict


def build_sudo_exec_cmd(binary, cmd_dict: dict):
    return "sudo -S " + binary + " " + " ".join([f"--{k}={v}" for k, v in cmd_dict.items()])


def build_exec_cmd(binary, cmd_dict: dict):
    return binary + " " + " ".join([f"--{k}={v}" for k, v in cmd_dict.items()])
