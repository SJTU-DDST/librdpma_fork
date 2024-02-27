import time
import paramiko
from typing import List
from paramiko.channel import ChannelFile

from . import color


class SessionConfig:
    def __init__(self, host: str, username: str, password: str):
        self.host = host
        self.username = username
        self.password = password


class Session:
    def __init__(self, config: SessionConfig):
        self.host = config.host
        self.username = config.username
        self.password = config.password
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        self.client.connect(self.host, username=self.username, password=self.password)
        self.channel = self.client.invoke_shell()


    def show_input(self, command):
        print(f"{color.GREEN}{color.BOLD}{self.username}@{self.host}{color.RESET}:{color.YELLOW}{color.BOLD}stdin{color.RESET}${color.YELLOW} {command}{color.RESET}")


    def quick_show_output(self, stdout: ChannelFile, stderr: ChannelFile, sleep_time: float):
        # no loop request (wait till .exit_status_ready()), async with ssh execute
        # so only for a quick output / error output just after .exec_command()
        time.sleep(sleep_time)
        if stdout.channel.recv_ready():
            quick_data = stdout.channel.recv(16384).decode("utf-8")
            print(f"{color.GREEN}{color.BOLD}{self.username}@{self.host}{color.RESET}:{color.BOLD}quick stdout{color.RESET}$ {quick_data}")
        if stderr.channel.recv_stderr_ready():
            quick_data = stderr.channel.recv_stderr(16384).decode("utf-8")
            print(f"{color.GREEN}{color.BOLD}{self.username}@{self.host}{color.RESET}:{color.RED}{color.BOLD}quick stderr{color.RESET}${color.RED} {quick_data}{color.RESET}")


    def execute(self, command: str, quick_show_out=True, quick_show_sleep_time=0.2, sudo_S_num=0, password=''):
        self.show_input(command)
        stdin, stdout, stderr = self.client.exec_command(command)

        if sudo_S_num > 0:
            stdin.write(''.join([password, '\n'] * sudo_S_num))

        if quick_show_out:
            self.quick_show_output(stdout, stderr, sleep_time=quick_show_sleep_time)

        return stdout.read().decode(), stderr.read().decode()


    def execute_many(self, commands: List[str], quick_show_out=True, quick_show_sleep_time=0.2, sudo_S_num=0, password=''):
        command = " && ".join(commands)
        self.show_input(command)
        stdin, stdout, stderr = self.client.exec_command(command)

        if sudo_S_num > 0:
            stdin.write(''.join([password, '\n'] * sudo_S_num))

        if quick_show_out:
            self.quick_show_output(stdout, stderr, sleep_time=quick_show_sleep_time)

        return stdout.read().decode(), stderr.read().decode()


    def execute_many_non_blocking(self, commands: List[str], quick_show_out=True, quick_show_sleep_time=0.2, sudo_S_num=0, password=''):
        command = " && ".join(commands)
        self.show_input(command)
        stdin, stdout, stderr = self.client.exec_command(command)

        if sudo_S_num > 0:
            stdin.write(''.join([password, '\n'] * sudo_S_num))

        if quick_show_out:
            self.quick_show_output(stdout, stderr, sleep_time=quick_show_sleep_time)

        return stdout, stderr


    def close(self):
        try:
            self.client.close()
        except:
            pass


if __name__ == "__main__":
    config = SessionConfig("0.0.0.0", "?", "?")
    session = Session(config)
    stdout, stderr = session.execute("ls")
    print(stdout)
    stdout, stderr = session.execute_many(["cd oceanbase", "ls"])
    print(stdout)
