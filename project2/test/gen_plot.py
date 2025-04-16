import os
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import seaborn as sns
import numpy as np

mfolders = ["mmapr", "fmapr"]

def read_latency_data():
    buf_sizes = [65536, 1048576, 16777216, 268435456]
    threads = [1, 2, 4, 8, 16, 32, 64]

    all_est_latency_data = []
    all_st_latency_data = []

    for mf in mfolders:
        for buf in buf_sizes:
            for th in threads:
                path = os.path.join(mf, str(buf), str(th))

                est_file = os.path.join(path, "est.csv")
                st_file = os.path.join(path, "st.csv")

                if os.path.exists(est_file):
                    est_df = pd.read_csv(est_file, sep=" ", header=None, names=["thread", "timestamp", "cycles"], dtype={"thread": int, "timestamp": int, "cycles": int})
                    est_df["mfolder"] = mf
                    est_df["buf_size"] = buf
                    est_df["thread_count"] = th
                    est_df = est_df.sort_values(by=["thread", "timestamp"])
                    est_df["time_diff"] = est_df.groupby("thread")["timestamp"].diff()
                    all_est_latency_data.append(est_df)

                if os.path.exists(st_file):
                    st_df = pd.read_csv(st_file, sep=" ", header=None, names=["thread", "timestamp", "cycles"], dtype={"thread": int, "timestamp": int, "cycles": int})
                    st_df["mfolder"] = mf
                    st_df["buf_size"] = buf
                    st_df["thread_count"] = th
                    st_df = st_df.sort_values(by=["thread", "timestamp"])
                    st_df["time_diff"] = st_df.groupby("thread")["timestamp"].diff()
                    all_st_latency_data.append(st_df)

    est_combined = pd.concat(all_est_latency_data, ignore_index=True) if all_est_latency_data else pd.DataFrame(columns=["thread", "timestamp", "cycles", "mfolder", "buf_size", "thread_count", "time_diff"])
    st_combined = pd.concat(all_st_latency_data, ignore_index=True) if all_st_latency_data else pd.DataFrame(columns=["thread", "timestamp", "cycles", "mfolder", "buf_size", "thread_count", "time_diff"])

    return est_combined, st_combined

def plot_3d_latency_per_type(df, label):
    for mfolder in mfolders:
        sub = df[df["mfolder"] == mfolder].copy()
        stats = sub.groupby(["thread", "thread_count", "buf_size"])["time_diff"].mean().reset_index()

        stats = stats.dropna()
        stats["thread"] = stats["thread"] + 1
        stats = stats.astype({"thread": int, "thread_count": int, "time_diff": float, "buf_size": int})

        fig = plt.figure(figsize=(12, 8))
        ax = fig.add_subplot(111, projection='3d')

        colors = plt.cm.viridis_r(np.linspace(0, 1, len(stats["buf_size"].unique())))

        for i, buf in enumerate(sorted(stats["buf_size"].unique())):
            buf_stats = stats[stats["buf_size"] == buf]
            buf_stats = buf_stats.copy()
            buf_stats["time_diff_norm"] = (buf_stats["time_diff"] / buf_stats["time_diff"].max())
            ax.plot_trisurf(
                buf_stats['thread'],
                buf_stats['thread_count'],
                buf_stats['time_diff_norm'],
                color=colors[i],
                edgecolor='none',
                alpha=0.8,
                label=f"Bufsize {buf}"
            )

        ax.set_title(f"{label} Latency Surface: Thread vs ThreadCount vs TimeDiff ({mfolder})")
        ax.set_xlabel("Thread")
        ax.set_ylabel("Thread Count")
        ax.set_yticks(range(0, 65, 8))
        ax.set_zlabel("Mean Time Difference (%)")
        
        legend_labels = [f"Bufsize {buf}" for buf in sorted(stats["buf_size"].unique())]
        legend_patches = [plt.Line2D([0], [0], marker='o', color='w', label=lbl,
                                     markerfacecolor=colors[i], markersize=8) for i, lbl in enumerate(legend_labels)]
        ax.legend(handles=legend_patches)
        
        plt.tight_layout()
        filename = f"out/3d_surface_latency_{label.lower()}_{mfolder}.png"
        plt.savefig(filename)
        plt.close()

def main():
    est_df, st_df = read_latency_data()
    plot_3d_latency_per_type(est_df, "Extents")
    plot_3d_latency_per_type(st_df, "Normal")

if __name__ == "__main__":
    main()
