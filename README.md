# InfiniBand/RoCE NIC Monitor

A real-time terminal-based monitoring tool for NVIDIA (Mellanox) InfiniBand and RoCE network interface cards. Monitor hardware counters, performance metrics, and error statistics with an intuitive ncurses-based UI.

## Features

- **Real-time Monitoring**: Live display of InfiniBand/RoCE port statistics and hardware counters
- **Comprehensive Metrics**: 
  - Port transmit/receive data and packet counts
  - Error tracking (CRC, link integrity, constraint errors)
  - RoCE-specific counters (ECN marks, congestion notifications)
  - Hardware counter information
- **Multi-Interface Support**: Monitor up to 16 interfaces simultaneously
- **Color-coded Display**: Visual indicators for active links, errors, and performance states
- **Automatic Refresh**: Configurable refresh intervals (default: 5 seconds)
- **Linux-only**: Optimized for Linux systems with NVIDIA OFED stack

## Prerequisites

- Linux operating system
- NVIDIA (Mellanox) InfiniBand or RoCE NIC
- NVIDIA OFED (OpenFabrics Enterprise Distribution) installed
- Build tools: `gcc`, `make`
- Development libraries: `libncurses5-dev` (or equivalent)
- `tmux` (for the run target in Makefile)

### Installation of Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install build-essential libncurses5-dev tmux
```

**RHEL/CentOS:**
```bash
sudo yum install gcc make ncurses-devel tmux
```

## Building

```bash
make clean
make
```

This creates an executable `ib_test` in the current directory.

## Usage

### Run with tmux (recommended):
```bash
make run
```

This will:
- Create or attach to a tmux session named `ib_monitor`
- Launch the monitoring tool in the tmux window
- Allow you to detach/reattach without losing the session

### Run directly:
```bash
./ib_test
```

### Controls

- **Ctrl+C**: Exit the program
- The display automatically refreshes every 5 seconds

## Monitored Metrics

The tool displays the following counter categories:

### Port Counters
- Port RCV/XMit Data (bytes)
- Port RCV/XMit Packets (count)
- Multicast/Unicast packet statistics
- Port errors and discards
- Link integrity and constraint errors

### Hardware Counters (RoCE-specific)
- Congestion Notification Packets (CNP)
- ECN marked packets
- Out-of-buffer events
- Sequence and ACK timeout errors
- RNR NAK retries
- CQE errors and flushes
- Remote access errors

## Architecture

- **main.c**: Main event loop and ncurses initialization
- **infiniband.c/h**: IB/RoCE metrics collection from sysfs
- **ncurses_display.c/h**: Terminal UI rendering and color scheme
- **extra.c/h**: Utility functions for file I/O and system checks

## Compatibility

- **InfiniBand (IB)**: Full support for traditional IB protocols
- **RoCEv2 (RDMA over Converged Ethernet)**: Full support with hardware-specific counters
- Tested with NVIDIA ConnectX-7 NICs

## Notes

- Requires appropriate permissions to read `/sys/class/infiniband/` counters
  - Run with `sudo` if you encounter permission errors
- Tool automatically detects connected devices; ensure NICs are properly configured and brought up
- Counter values are cumulative; deltas are calculated between refresh intervals


## Credits & References

This project is enhanced from and builds upon:

- **[NVIDIA ib-traffic-monitor](https://github.com/NVIDIA/ib-traffic-monitor)** - Original traffic monitoring concept
- **[NVIDIA/Mellanox MLX5 Documentation](https://enterprise-support.nvidia.com/s/article/understanding-mlx5-linux-counters-and-status-parameters)** - Hardware counter reference guide
- **ncurses library** - Terminal UI framework


