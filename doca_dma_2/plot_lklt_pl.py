import json
import matplotlib.pyplot as plt
import os

files = os.listdir('.')
file_name = 'bench_lklt_pl'
save_dir = 'figures'

for file in files:
    if file.startswith(file_name + '_') and file.endswith('.json'):
        try:
            num = int(file[(len(file_name) + 1):-5])
            file_path = file_name + '_' + str(num) + '.json'

            with open(file_path, 'r') as f:
                data = json.load(f)

            plt.figure(figsize=(10, 6))

            setup_lt = [entry['setup_lt'] for entry in data]
            task_lt = [entry['task_lt'] for entry in data]
            cleanup_lt = [entry['cleanup_lt'] for entry in data]

            payload = [entry['file_size'] for entry in data]
            ops = data[0]['ops']
            
            plt.plot(payload, setup_lt, label=f'setup', marker='o', alpha=1, markersize=5, linewidth=2)
            plt.plot(payload, task_lt, label=f'task', marker='o', alpha=1, markersize=5, linewidth=2)
            plt.plot(payload, cleanup_lt, label=f'cleanup', marker='o', alpha=1, markersize=5, linewidth=2)

            plt.xlim(left=0)
            plt.ylim(bottom=0)
            plt.xlabel('Payload (Bytes)')
            plt.ylabel('Latency (µs)')
            plt.title('Setup/Task/Cleanup Latency vs Payload')
            plt.legend()
            plt.grid(True)

            save_path = save_dir + '/' + file_name + '_' + '1' + '_' + str(num) + '.png'
            plt.savefig(save_path)
            print('Figure saved to' + save_path)

        except ValueError:
            print('num error')
            pass


