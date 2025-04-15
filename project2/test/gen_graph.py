import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D

mfolders = ["mmap", "fmap"]

def read_data():
    buf_sizes = [4096, 65536, 1048576, 16777216, 268435456]
    threads = [1, 2, 4, 8, 16, 32, 64]

    all_est_data = []
    all_st_data = []

    for mf in mfolders:
        for buf in buf_sizes:
            for th in threads:
                path = os.path.join(mf, str(buf), str(th))
                est_file = os.path.join(path, "est.csv")
                st_file = os.path.join(path, "st.csv")

                if os.path.exists(est_file):
                    est_df = pd.read_csv(est_file, header=None, names=[
                        "thread", "totaltime", "avgtime", "totalext", "totalpage", "avgext", "avgpage"])
                    est_df["mfolder"] = mf
                    est_df["buf_size"] = buf
                    if len(est_df) == 2: # added to check using subset of data
                        est_df = pd.concat([est_df]*500, ignore_index=True)
                    all_est_data.append(est_df)

                if os.path.exists(st_file):
                    st_df = pd.read_csv(st_file, header=None, names=[
                        "thread", "totaltime", "avgtime"])
                    st_df["mfolder"] = mf
                    st_df["buf_size"] = buf
                    if len(st_df) == 2: # added to check using subset of data
                        st_df = pd.concat([st_df]*500, ignore_index=True)
                    all_st_data.append(st_df)

    est_combined = pd.concat(all_est_data, ignore_index=True)
    st_combined = pd.concat(all_st_data, ignore_index=True)
    return est_combined, st_combined


def plot_3d_graphs(est_df, st_df):
    buf_groups = {
        "small": [4096, 65536, 1048576],
        "large": [16777216, 268435456]
    }

    for mf in mfolders:
        for group_name, buffers in buf_groups.items():
            fig = plt.figure()
            ax = fig.add_subplot(111, projection='3d')

            handles = []
            labels = []

            for label, df, color in [("est", est_df, 'blue'), ("st", st_df, 'red')]:
                subset = df[(df["mfolder"] == mf) & (df["buf_size"].astype(int).isin(buffers))]
                subset = subset.copy()
                subset["buf_size"] = subset["buf_size"].astype(int)
                subset["thread"] = subset["thread"].astype(int)

                unique_threads = sorted(subset["thread"].unique())
                thread_map = {t: i * 4 for i, t in enumerate(unique_threads)}
                subset["thread_stretched"] = subset["thread"].map(thread_map)

                pivot = subset.pivot_table(index="thread_stretched", columns="buf_size", values="avgtime", aggfunc='mean')
                X, Y = np.meshgrid(pivot.columns, pivot.index)
                Z = pivot.values

                surf = ax.plot_surface(X, Y, Z, color=color, alpha=0.7)
                handles.append(Line2D([0], [0], color=color, lw=4))
                labels.append(label.upper())

            for yval in thread_map.values():
                ax.plot([X.min(), X.max()], [yval, yval], [Z.min(), Z.min()], 'k--', linewidth=0.5)

            ax.set_xlabel("Buffer Size")
            ax.set_ylabel("Threads")
            ax.set_yticks(list(thread_map.values()))
            ax.set_yticklabels(list(thread_map.keys()))
            ax.set_zlabel("Avg Time")
            ax.set_title(f"{mf.upper()} - {group_name.capitalize()} Buffers")
            ax.legend(handles, labels, loc='best')
            plt.savefig(f"{mf}_{group_name}_avgtime_surface.png")
            plt.close()


def plot_extents_graph(est_df):
    buf_set = [1048576, 16777216, 268435456]

    for mf in mfolders:
        fig = plt.figure()
        ax = fig.add_subplot(111, projection='3d')

        subset = est_df[(est_df["mfolder"] == mf) & (est_df["buf_size"].isin(buf_set))]
        subset = subset.copy()
        subset["buf_size"] = subset["buf_size"].astype(int)
        subset["thread"] = subset["thread"].astype(int)

        unique_threads = sorted(subset["thread"].unique())
        thread_map = {t: i * 4 for i, t in enumerate(unique_threads)}
        subset["thread_stretched"] = subset["thread"].map(thread_map)

        pivot = subset.pivot_table(index="thread_stretched", columns="buf_size", values="totalext", aggfunc='mean')
        X, Y = np.meshgrid(pivot.columns, pivot.index)
        Z = pivot.values

        surf = ax.plot_surface(X, Y, Z, cmap='viridis', alpha=0.8)

        ax.set_xlabel("Buffer Size")
        ax.set_ylabel("Threads")
        ax.set_yticks(sorted(subset["thread_stretched"].unique()))
        ax.set_zlabel("Total Extents")
        ax.set_title(f"{mf.upper()} - Selected Buffers - Extents")
        plt.savefig(f"{mf}_selected_extents_surface.png")
        plt.close()


def plot_cdn_log_extents(est_df):
    buf_set = [16777216, 268435456]

    for mf in mfolders:
        fig, ax = plt.subplots()
        subset = est_df[(est_df["mfolder"] == mf) & (est_df["buf_size"].astype(int).isin(buf_set))]
        subset = subset.copy()
        subset["buf_size"] = subset["buf_size"].astype(int)
        subset["thread"] = subset["thread"].astype(int)

        unique_threads = sorted(subset["thread"].unique())
        thread_map = {t: i * 4 for i, t in enumerate(unique_threads)}
        subset["thread_stretched"] = subset["thread"].map(thread_map)

        for buf in sorted(subset["buf_size"].unique()):
            buf_df = subset[subset["buf_size"] == buf]
            grouped = buf_df.groupby("thread_stretched")["totalext"].mean()
            reverse_log = np.log1p(1 / (grouped.values + 1e-5))
            ax.plot(reverse_log, grouped.index, label=f"Buf {buf}")

        for yval in thread_map.values():
            ax.axhline(y=yval, color='gray', linestyle='--', linewidth=0.5)

        ax.set_ylabel("Threads")
        ax.set_yticks(list(thread_map.values()))
        ax.set_yticklabels(list(thread_map.keys()))
        ax.set_xlabel("log(1 + 1 / Total Extents)")
        ax.set_title(f"Reverse CDN: {mf.upper()} - Threads vs log(1 + 1 / Extents)")
        ax.legend(title="Buffer Size")
        plt.savefig(f"reverse_cdn_{mf}_log1_inv_extents_vs_threads.png")
        plt.close()


if __name__ == "__main__":
    est_df, st_df = read_data()
    plot_3d_graphs(est_df, st_df)
    plot_extents_graph(est_df)
    plot_cdn_log_extents(est_df)
