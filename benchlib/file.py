import yaml
from pathlib import Path
from datetime import datetime
from typing import List


def load_config(config_file_path: str) -> dict:
    with open(config_file_path, "r") as config_file:
        config_yaml = yaml.load(config_file, yaml.Loader)
        return config_yaml


def generate_time_str():
    return datetime.now().strftime(r"%Y-%m-%d-%H-%M-%S-%f")


def generate_bench_result_filename(clients: List[str], server: str, time_str: str):
    cn = ",".join(clients)
    return f"benchres_{cn}_{server}_{time_str}.json"


def generate_single_testcase_picture_filename(type: str, time_str: str):
    Path("./img/").mkdir(parents=True, exist_ok=True)
    return f"./img/{type}_{time_str}.png"


def generate_compare_testcases_picture_filename(
    about: str, fix_value: str, time_str: str
):
    Path("./img_compare/").mkdir(parents=True, exist_ok=True)
    return f"./img_compare/{about}_WITH_{fix_value}_TIME_{time_str}.png"
