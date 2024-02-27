import yaml
from pathlib import Path
from datetime import datetime


def load_config(config_file_path: str) -> dict:
    with open(config_file_path, "r") as config_file:
        config_yaml = yaml.load(config_file, yaml.Loader)
        return config_yaml


def generate_time_str():
    return datetime.now().strftime(r"%Y-%m-%d-%H-%M-%S-%f")


def generate_bench_result_filename(dir: str | Path, clients: list[str], server: str, time_str: str) -> Path:
    dir = Path(dir)
    dir.mkdir(parents=True, exist_ok=True)
    cn = ",".join(clients)
    return dir / f"benchres_{cn}_{server}_{time_str}.json"


def generate_single_testcase_picture_filename(dir: str | Path, type: str, time_str: str) -> Path:
    dir = Path(dir)
    dir.mkdir(parents=True, exist_ok=True)
    return dir / f"{type}_{time_str}.png"


def generate_compare_testcases_picture_filename(dir: str | Path, about: str, fix_value: str, time_str: str) -> Path:
    dir = Path(dir)
    dir.mkdir(parents=True, exist_ok=True)
    return dir / f"{about}_WITH_{fix_value}_TIME_{time_str}.png"
