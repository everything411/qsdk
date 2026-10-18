#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#

#
# usage: sh iperf3_load_balance.sh
#
# We run the script along with IPERF command like below
# to perform the LOAD distribution of threads across the cores.
#
# Usage example :
# taskset 0x3 iperf3 -c 2001::2 -u -b1.5G -P8 -R -i2 -t20 --skip-rx-copy --offload --omit 5 & sh iperf3_load_balance.sh
#

# We ensure that IPERF3 control/data path channels are initiated and
# stabilized before load balancing action kicks in.
sleep 2

# Step 1: Get the main PID of iperf3
main_pid=$(pgrep -x iperf3)
if [ -z "$main_pid" ]; then
	echo "iperf3 process not found."
	exit 1
fi
echo "Main PID: $main_pid"

# Step 2: Get the CPU affinity mask
cpu_mask=$(taskset -p "$main_pid" | awk -F': ' '{print $2}')
echo "CPU mask: $cpu_mask"

# Convert hex mask to list of CPU cores
cpu_list=""
i=0
mask=$(printf "%d" 0x$cpu_mask)

while [ $i -lt 64 ]; do
	bit=$((1 << i))
	if [ $((mask & bit)) -ne 0 ]; then
		cpu_list="$cpu_list $i"
	fi
	i=$((i + 1))
done

if [ -z "$cpu_list" ]; then
	echo "No CPUs found in mask."
	exit 1
fi
echo "Allowed CPUs:$cpu_list"

# Step 3: Get thread IDs (excluding main PID)
thread_ids=""
for tid in /proc/"$main_pid"/task/*; do
	tid_basename=$(basename "$tid")
	if [ "$tid_basename" != "$main_pid" ]; then
		thread_ids="$thread_ids $tid_basename"
	fi
done

echo "Allowed thread_ids:$thread_ids"

# Step 4: Round-robin assign threads to CPUs
cpu_array=$(echo "$cpu_list" | tr ' ' '\n' | grep -v '^$')
cpu_count=$(echo "$cpu_array" | wc -l | tr -d ' ')
index=0

echo "CPU array:"
echo "$cpu_array"
echo "CPU count: $cpu_count"

for tid in $thread_ids; do
	cpu=$(echo "$cpu_array" | sed -n "$((index % cpu_count + 1))p")
	echo "Assigning TID $tid to CPU $cpu"
	if [ -n "$cpu" ]; then
		taskset -p -c "$cpu" "$tid" > /dev/null
	else
		echo "Warning: CPU not found for TID $tid"
	fi
	index=$((index + 1))
done

echo "Thread affinity updated for all relevant TIDs."
