import re
import argparse
from pathlib import Path
from collections import defaultdict

import pandas as pd
import matplotlib.pyplot as plt


"""
Example usage
```batch
python analyze_task_timings.py "your_log.txt" -o output_folder
```
"""

TASK_PATTERN = re.compile(r"\[Task\]\s+(.+?)\s+took\s+(\d+)\s+us")


def parse_task_timings(txt_path: Path) -> pd.DataFrame:
    text = txt_path.read_text(errors="ignore")
    matches = TASK_PATTERN.findall(text)

    if not matches:
        raise ValueError(
            f"No task timing lines were found in {txt_path}. "
            "Expected lines like: [Task] RelayCtrl took 5 us"
        )

    rows = []
    for i, (task, duration) in enumerate(matches):
        rows.append(
            {
                "event_index": i,
                "task": task,
                "duration_us": int(duration),
            }
        )

    return pd.DataFrame(rows)


def summarize_tasks(df: pd.DataFrame) -> pd.DataFrame:
    summary = (
        df.groupby("task")["duration_us"]
        .agg(
            count="count",
            min_us="min",
            max_us="max",
            mean_us="mean",
            median_us="median",
            std_us="std",
            unique_values="nunique",
        )
        .reset_index()
        .sort_values("task")
    )

    summary["std_us"] = summary["std_us"].fillna(0.0)
    return summary


def save_distribution_plots(df: pd.DataFrame, output_dir: Path) -> None:
    for task_name, sub in df.groupby("task"):
        values = sub["duration_us"]

        plt.figure(figsize=(8, 5))
        n_unique = values.nunique()

        if n_unique <= 10:
            counts = values.value_counts().sort_index()
            plt.bar(counts.index.astype(str), counts.values)
            plt.xlabel("Duration (us)")
            plt.ylabel("Count")
        else:
            bins = min(30, max(5, int(len(values) ** 0.5)))
            plt.hist(values, bins=bins)
            plt.xlabel("Duration (us)")
            plt.ylabel("Frequency")

        plt.title(f"{task_name} timing distribution")
        plt.tight_layout()
        plt.savefig(output_dir / f"{task_name}_timing_distribution.png", dpi=180, bbox_inches="tight")
        plt.close()


def save_timeline_plot(df: pd.DataFrame, output_dir: Path) -> None:
    plt.figure(figsize=(12, 6))

    for task_name, sub in df.groupby("task"):
        plt.plot(
            sub["event_index"],
            sub["duration_us"],
            marker="o",
            linestyle="-",
            label=task_name,
        )

    plt.xlabel("Event index")
    plt.ylabel("Duration (us)")
    plt.title("Task timing timeline")
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_dir / "task_timing_timeline.png", dpi=180, bbox_inches="tight")
    plt.close()


def save_mean_plot(summary_df: pd.DataFrame, output_dir: Path) -> None:
    plt.figure(figsize=(8, 5))
    plt.bar(summary_df["task"], summary_df["mean_us"])
    plt.xlabel("Task")
    plt.ylabel("Mean duration (us)")
    plt.title("Mean task duration")
    plt.xticks(rotation=30, ha="right")
    plt.tight_layout()
    plt.savefig(output_dir / "mean_task_duration.png", dpi=180, bbox_inches="tight")
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Analyze task timing lines in a TXT log file."
    )
    parser.add_argument(
        "input_txt",
        type=Path,
        help="Path to the input TXT log file",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path("timing_plots"),
        help="Directory where plots and CSV summary will be saved",
    )
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)

    df = parse_task_timings(args.input_txt)
    summary_df = summarize_tasks(df)

    summary_df.to_csv(args.output_dir / "timing_summary.csv", index=False)
    df.to_csv(args.output_dir / "timing_events.csv", index=False)

    save_distribution_plots(df, args.output_dir)
    save_timeline_plot(df, args.output_dir)
    save_mean_plot(summary_df, args.output_dir)

    print(f"Parsed {len(df)} task timing events from {args.input_txt}")
    print(f"Found tasks: {', '.join(sorted(df['task'].unique()))}")
    print(f"Outputs saved in: {args.output_dir}")


if __name__ == "__main__":
    main()
