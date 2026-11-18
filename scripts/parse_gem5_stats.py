#!/usr/bin/env python3
# File: scripts/parse_gem5_stats.py
import re
import sys
import os

def parse_roi_stats(stats_file_path, clock_mhz=866.0):
    if not os.path.exists(stats_file_path):
        print(f"Error: Stats file {stats_file_path} not found.")
        sys.exit(1)

    with open(stats_file_path, 'r') as f:
        content = f.read()

    # Split on "---------- End Simulation Statistics ----------"
    dumps = content.split("---------- End Simulation Statistics ----------")
    if len(dumps) < 2:
        print("Warning: Only 1 stats dump found (missing ROI markers). Parsing Dump 0.")
        roi_dump = dumps[0]
    else:
        # Dump 1 is the Region of Interest (between reset_stats and dump_stats)
        roi_dump = dumps[1]

    metrics = {
        'sim_seconds': 0.0,
        'sim_ticks': 0,
        'roi_cycles': 0,
        'avg_packet_latency': 0.0,
        'packets_injected': 0,
        'l2_demand_hits': 0,
        'l2_demand_misses': 0
    }

    sim_ticks_m = re.search(r'simTicks\s+([0-9]+)', roi_dump)
    if sim_ticks_m:
        metrics['sim_ticks'] = int(sim_ticks_m.group(1))
        # 1 tick = 1 ps in gem5. Cycle time for 866 MHz = 1154.734 ps
        cycle_ps = (1e12 / (clock_mhz * 1e6))
        metrics['roi_cycles'] = int(metrics['sim_ticks'] / cycle_ps)

    sim_sec_m = re.search(r'simSeconds\s+([0-9.e+-]+)', roi_dump)
    if sim_sec_m:
        metrics['sim_seconds'] = float(sim_sec_m.group(1))

    pkt_lat_m = re.search(r'system\.ruby\.network\.average_packet_latency\s+([0-9.]+)', roi_dump)
    if pkt_lat_m:
        metrics['avg_packet_latency'] = float(pkt_lat_m.group(1))

    pkt_inj_m = re.search(r'system\.ruby\.network\.packets_injected::total\s+([0-9]+)', roi_dump)
    if pkt_inj_m:
        metrics['packets_injected'] = int(pkt_inj_m.group(1))

    # Sum L2 demand hits and misses across all 64 L2 banks
    l2_hits = sum(int(x) for x in re.findall(r'system\.ruby\.l2_cntrl[0-9]+\.demand_hits\s+([0-9]+)', roi_dump))
    l2_miss = sum(int(x) for x in re.findall(r'system\.ruby\.l2_cntrl[0-9]+\.demand_misses\s+([0-9]+)', roi_dump))
    metrics['l2_demand_hits'] = l2_hits
    metrics['l2_demand_misses'] = l2_miss

    return metrics

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: ./parse_gem5_stats.py <path_to_stats.txt>")
        sys.exit(1)
    res = parse_roi_stats(sys.argv[1])
    print("Parsed ROI Metrics:")
    for k, v in res.items():
        print(f"  {k}: {v}")
