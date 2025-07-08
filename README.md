# LibRDPMA, tools to analysis the performance when accessing NVM with RDMA

LibRDPMA provides a set of tools  to analyze the behavior when accessing NVM (i.e., Intel Optane DC persistent memory) with RDMA. These including benchmarks for one-sided RDMA and two-sided RDMA and tools to analyze NVM behavior. 



## Getting Started

Building the tools of librdpma is straightforward since it will automatically install dependencies. Specifically, using the following steps:

- Clone the project with `git clone git@github.com:SJTU-DDST/librdpma_fork.git --recursive`
- `sudo cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1`
- `cd build && sudo make -j`

- Set huge pages `sudo sysctl -w vm.nr_hugepages=16384`

### Running benchmarks


```shell
sudo ./scripts/nvm_server --host=localhost --port=8964 -use_nvm=false -touch_mem=true --nvm_sz=8 --nvm_file=/dev/dax12.0
```

A running example of the client：

```shell
./scripts/nvm_client -addr="localhost:8964" --force_use_numa_node=false --use_numa_node=0 --threads=28 --coros=1 --id=0 --use_nic_idx=0 --use_read=true --payload=256 --add_sync=false --address_space=8 --random=true -read_write=false -two_qp=false
```

Parameter Explaination：

* Numa Setting:
    * numa_type = 3: y = x 
    * force_use_numa_node = 1, use_numa_node = 0: 0-17, 36-53
    * force_use_numa_node = 1, use_numa_node = 1: 18-35, 53-71
    * force_use_numa_node = 0, numa_type = 1, use_numa_node = 0: y= 2 * x 
    * force_use_numa_node = 0, numa_type = 1, use_numa_node = 1: y= 2 * x + 1  
    * force_use_numa_node = 0, numa_type = 1, use_numa_node = 2: y= x
* threads, coros: 线程数和协程数
* id: 编号，设为0即可
* use_nix_idx: 使用的RDMA 网卡编号
* use_true: true：测RDMA read, false: 测 RDMA write
* payload: READ read/write payload的大小
* add_sync: 是否doorbell batching
* address_space: 必须 <= server端的nvm_sz, 单位是GB。
* random: 读/写的远端地址是固定的，还是随机一个地址

* 两个后续加的特殊的参数，二者都有些词不达意，所以重点解释下：
    * 默认client的行为：根据use_read的真假不断执行payload 大小的 RDMA read/write, 其中每个coro一个QP
    * two_qp: 为true时，每个coro开两个QP，根据 use_read的真假不断执行两个QP并发执行payload大小的RDMA read/write。此选项为true时，要求 read_write=false
    * read_write: 为true时，每个coro开两个QP。每次操作为：先根据 use_read的真假不断执行两个QP并发执行payload大小的RDMA read/write，然后再用其中一个QP做一个8bytes的write，此选项为true时，要求 two_qp=false
    * doorbell: 为true时，使用doorbell batching来增加并行性。要求two_qp和read_write为false。且设置batch参数，指定doorbell batching的数量。
    * search/update: 为true时，模拟learned index的search和update行为
    * CAS：为true时，测试CAS的性能
    * To do：建议用bench=1,2,3,4,...来替代上面的选项

* 运行脚本在./scripts/

#### original


For the built binaries, `./nvm_rrtserver` and ``./nvm_rrtclient` are used for evaluating two-sided performance, while `./nvm_server` and `./nvm_client` are used for evaluating one-sided performance. We provide scripts to run experiments. For how to configure these binaries, please use `binary --help` to check.

We've also provide scripts to run experiments. For example, to run one-sided evaluations, use the following:

- `cd scripts; ./bootstrap-proxy.py -f run_one.toml`; Note that `run_one.toml` should be configured according to your hardawre setting. It is straightforward to configure it  based on its content. 

  

### Other tools

We provide some tools for make system configurations or monitor NVM statistics. 

- To tune DDIO setups, use `cd ddio_tools; cmake; make;` and then use `setup_dca`. 

- To monitor NVM read/write amplications, use `cd nvm; python analysis.py`.  Note that `ipmctl` should be installed. 



## Check our results

To check the results of these benchmarks, please refer to our paper: 

[**ATC**] Characterizing and Optimizing Remote Persistent Memory with RDMA and NVM. Xingda Wei and Xiating Xie and Rong Chen and Haibo Chen and Binyu Zang. 2021 USENIX Annual Technical Conference. 