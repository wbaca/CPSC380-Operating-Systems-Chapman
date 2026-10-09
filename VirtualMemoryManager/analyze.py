import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("summaryStride.csv")
numeric_cols = ["Frames", "Total", "Page Fault Rate", "TLB Hit Rate", "Page Faults", "TLB Hits", "TLB Misses"]
df[numeric_cols] = df[numeric_cols].apply(pd.to_numeric, errors="coerce")
df["Label"] = df["TLB Policy"] + "/" + df["Page Policy"] + "/" + df["Frames"].astype(int).astype(str) + "fr"
df = df.sort_values("Label")

fig, axes = plt.subplots(1, 3, figsize=(18, 6))
fig.suptitle("Virtual Memory Manager — Policy Comparison - Stride", fontsize=14)

def bar_chart(ax, col, title, ylabel, color):
    ax.bar(df["Label"], df[col], color=color)
    ax.set_title(title)
    ax.set_ylabel(ylabel)
    ax.set_ylim(0, df[col].max() * 1.15)
    ax.tick_params(axis="x", rotation=85)
    for i, v in enumerate(df[col]):
        label = f"{v:.1f}%" if "Rate" in col else str(int(v))
        ax.text(i, v + df[col].max() * 0.02, label, ha="center", fontsize=9)

bar_chart(axes[0], "Page Fault Rate", "Page Fault Rate",  "Rate (%)",   "steelblue")
bar_chart(axes[1], "TLB Hit Rate",    "TLB Hit Rate",     "Rate (%)",   "darkorange")
bar_chart(axes[2], "Page Faults",     "Total Page Faults","Count",      "firebrick")

plt.tight_layout()
plt.savefig("policy_comparison_stride.png", dpi=150)
plt.show()
print(df[["Label", "Page Fault Rate", "TLB Hit Rate", "Page Faults", "TLB Hits", "TLB Misses"]].to_string(index=False))