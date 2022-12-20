# Caladan

Caladan is a system that enables servers in datacenters to
simultaneously provide low tail latency and high CPU efficiency, by
rapidly reallocating cores across applications.

### Contact

For any questions about Caladan, please email <caladan@csail.mit.edu>.

## Setup

1) Enable monitor/mwait in BIOS.

Check it with
```
lscpu | grep monitor
```

2) Enable intel idle driver.

Make sure `idle=poll/hlt/nomwait` is not set in kernel cmdline in file `/etc/default/grub`.

Check current driver with
```
cat /sys/devices/system/cpu/cpuidle/current_driver
```
It should be `intel_idle`.

3) Install Mellanox OFED.
```
wget "https://content.mellanox.com/ofed/MLNX_OFED-4.9-5.1.0.0/MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64.iso"
sudo mount -o ro,loop MLNX_OFED_LINUX-4.9-5.1.0.0-ubuntu20.04-x86_64.iso /mnt
sudo /mnt/mlnxofedinstall --add-kernel-support --dpdk --upstream-libs # it's fine to see 'Failed to install libibverbs-dev DEB'
sudo /etc/init.d/openibd restart
```
4) Configure IB NIC.

Switch mlx5 nic port to eth mode
```
sudo mstconfig -d <pci addr> set LINK_TYPE_P<port id>=2
sudo mstfwreset -d <pci addr> -l3 -y reset
```

## How to Run Caladan

1) Clone the Caladan repository.

The ib device name should be hard coded with `strncmp(ibv_get_device_name(ib_dev), "mlx5", 4)` in
file `caladan/runtime/net/directpath/mlx5/mlx5_init.c`.

The ib device port should also be hard coded with `dp.port = 0` in file
`caladan/iokernel/dpdk.c`.

2) Set up submodules (e.g., DPDK, SPDK, and rdma-core).

```
make submodules
```

3) Build the scheduler (IOKernel), the Caladan runtime, and Ksched and perform some machine setup.
Before building, set the parameters in build/config (e.g., `CONFIG_SPDK=y` to use
storage, `CONFIG_DIRECTPATH=y` to use directpath, and the MLX4 or MLX5 flags to use
MLX4 or MLX5 NICs, respectively, ). To enable debugging, set `CONFIG_DEBUG=y` before building.
```
make clean && make
pushd ksched
make clean && make
popd
sudo ./scripts/setup_machine.sh
```

4) Install Rust and build a synthetic client-server application.

```
curl https://sh.rustup.rs -sSf | sh
rustup default nightly-2021-01-07
```
```
cd apps/synthetic
cargo clean
cargo update
cargo build --release
```

5) Run the synthetic application with a client and server. The client
sends requests to the server, which performs a specified amount of
fake work (e.g., computing square roots for 10us), before responding.

On the server:
```
sudo ./iokerneld
./apps/synthetic/target/release/synthetic 192.168.1.3:5000 --config server.config --mode spawner-server
```

On the client:
```
sudo ./iokerneld
./apps/synthetic/target/release/synthetic 192.168.1.3:5000 --config client.config --mode runtime-client
```

## Supported Platforms

This code has been tested most thoroughly on Ubuntu 18.04, with kernel
5.2.0.

### NICs
This code has been tested with Intel 82599ES 10 Gbits/s NICs,
Mellanox ConnectX-3 Pro 10 Gbits/s NICs, and Mellanox Connect X-5 40 Gbits/s NICs.
If you use Mellanox NICs, you should install the Mellanox OFED as described in [DPDK's
documentation](https://doc.dpdk.org/guides/nics/mlx4.html). If you use
Intel NICs, you should insert the IGB UIO module and bind your NIC
interface to it (e.g., using the script `./dpdk/usertools/dpdk-setup.sh`).

#### Directpath
Directpath allows runtime cores to directly send packets to/receive packets from the NIC, enabling
higher throughput than when the IOKernel handles all packets.
Directpath is currently only supported with Mellanox ConnectX-5 using Mellanox OFED v4.6 or newer.
NIC firmware must include support for User Context Objects (DEVX) and Software Managed Steering Tables.
For the ConnectX-5, the firmware version must be at least 16.26.1040. Additionally, directpath requires
Linux kernel version 5.0.0 or newer.

To enable directpath, set `CONFIG_DIRECTPATH=y` in build/config before building and add `enable_directpath`
to the config file for all runtimes that should use directpath. Each runtime launched with directpath must
currently run as root and have a unique IP address.

### Storage
This code has been tested with an Intel Optane SSD 900P Series NVMe device.
If your device has op latencies that are greater than 10us, consider updating the device_latency_us
variable (or the known_devices list) in runtime/storage.c.

## More Examples

#### Running a simple block storage server
Ensure that you have compiled Caladan with storage support by setting the appropriate flag in build/config,
and that you have built the synthetic client application.

Compile the C++ bindings and the storage server:
```
make -C bindings/cc
make -C apps/storage_service
```

On the server:
```
sudo ./iokerneld
sudo spdk/scripts/setup.sh
sudo apps/storage_service/storage_server storage_server.config
```

On the client:
```
sudo ./iokerneld
sudo apps/synthetic/target/release/synthetic --config=storage_client.config --mode=runtime-client --mpps=0.55 --protocol=reflex --runtime=10 --samples=10 --threads=20 --transport=tcp 192.168.1.3:5000
```

#### Running with interference

Ensure that you have built the synthetic application on client and server.

Compile the C++ bindings and the memory/cache antagonist:
```
make -C bindings/cc
make -C apps/netbench
```

On the server, run the IOKernel with the interference-aware scheduler (ias),
the synthetic application, and the cache antagonist:
```
sudo ./iokerneld ias
./apps/synthetic/target/release/synthetic 192.168.1.8:5000 --config victim.config --mode spawner-server
./apps/netbench/stress antagonist.config 20 10 cacheantagonist:4090880
```

On the client:
```
sudo ./iokerneld
./apps/synthetic/target/release/synthetic 192.168.1.8:5000 --config client.config --mode runtime-client
```

You should observe that you can stop and start the antagonist and that the
synthetic application's latency is not impacted. In contrast, if you use
Shenango's default scheduler (`sudo ./iokerneld`) on the server, when you run
the antagonist with the synthetic application, the synthetic application's
latency degrades.
