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


def make_thread_config(numa_type, force_use_numa_node, use_numa_node) -> dict:
    return {
        "numa_type": numa_type,
        "force_use_numa_node": force_use_numa_node,
        "use_numa_node": use_numa_node,
    }


def build_exec_cmd(binary, cmd_dict: dict):
    return binary + " " + " ".join([f"--{k}={v}" for k, v in cmd_dict.items()])
