import paramiko
from typing import List

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

    def execute(self, command: str):
        _, stdout, stderr = self.client.exec_command(command)
        return stdout.read().decode(), stderr.read().decode()
    
    def execute_many(self, commands: List[str]):
        _, stdout, stderr = self.client.exec_command(" && ".join(commands))
        return stdout.read().decode(), stderr.read().decode()
    
    def execute_many_non_blocking(self, commands: List[str]):
        _, stdout, stderr = self.client.exec_command(" && ".join(commands))
        return stdout, stderr
        

    def close(self):
        try:
            self.client.close() 
        except:
            pass
        
if __name__ == '__main__':
    config = SessionConfig("192.168.98.74", "jingxiang", "1111")
    session = Session(config)
    stdout, stderr = session.execute("ls")
    print(stdout)
    stdout, stderr = session.execute_many(['cd oceanbase', 'ls'])
    print(stdout)
