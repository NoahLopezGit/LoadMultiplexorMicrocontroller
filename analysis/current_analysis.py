import re
import matplotlib.pyplot as plt


def parse_current_msgs(filename):
    """
    Parse CAN log lines like:
    Received on CAN2: ID=0x200 Data=46 3 46 3 20 3 53 3

    Each ID=0x200 line contains 4 current measurements.
    For each measurement:
      value = low_byte + (high_byte << 8)

    Returns:
        samples: list of sample indices
        currents: list of 4 lists, one per current channel
    """
    pattern = re.compile(r'ID=0x200\s+Data=([0-9A-Fa-f ]+)')

    currents = [[], [], [], []]
    sample_indices = []

    with open(filename, "r") as f:
        sample = 0
        for line in f:
            match = pattern.search(line)
            if not match:
                continue

            byte_strs = match.group(1).split()
            if len(byte_strs) != 8:
                # Skip malformed lines
                continue

            try:
                bytes_int = [int(b, 16) for b in byte_strs]
            except ValueError:
                continue

            # 4 measurements: (low, high) pairs
            values = []
            for i in range(0, 8, 2):
                low = bytes_int[i]
                high = bytes_int[i + 1]
                value_ma = low + (high << 8)
                values.append(value_ma)

            for ch in range(4):
                currents[ch].append(values[ch])

            sample_indices.append(sample)
            sample += 1

    return sample_indices, currents


def plot_currents(samples, currents):
    plt.figure(figsize=(10, 6))

    for i, channel_data in enumerate(currents, start=1):
        plt.plot(samples, channel_data, label=f"Current {i}")

    plt.xlabel("Sample")
    plt.ylabel("Current (mA)")
    plt.title("Current vs Sample")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    filename = "data/current msgs 50 mA.txt"
    samples, currents = parse_current_msgs(filename)
    plot_currents(samples, currents)