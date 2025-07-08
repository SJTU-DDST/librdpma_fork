# default configuation
# Purdue Best: coro = 1; NUMA Node= 1; thread = 56 -> 18Mops/s
# Purdue Best: coro = 2; NUMA Node= 1; thread = 28 -> 21Mops/s
./nvm_client -addr="db2.cs.purdue.edu:8964" --force_use_numa_node=false --use_numa_node=1 --numa_type=1 --threads=56 --coros=1 --id=0 --use_nic_idx=0 --remote_nic_idx=0 --payload=64 --use_read=false --add_sync=false --address_space=8 --random=true -read_write=false --doorbell=true --batch=1 -two_qp=false
# ./nvm_client -addr="localhost:8964" --force_use_numa_node=false --use_numa_node=1 --numa_type=1 --threads=56 --coros=1 --id=0 --use_nic_idx=0 --remote_nic_idx=0 --payload=64 --use_read=false --add_sync=false --address_space=8 --random=true -read_write=false --doorbell=true --batch=1 -two_qp=false