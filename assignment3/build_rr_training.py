import csv
import sys
import math
from pathlib import Path
from typing import List, Dict

def usage() -> None:
    print("Usage:")
    print("  python build_rr_training.py [rr_results.csv]") 

# Copied from ml_rr_predict.py with minor adjustments for training data generation
def parse_workload(workload_path: Path) -> Dict[str, float]:
    
    bursts: List[float] = []
    arrivals: List[float] = []
    
    with workload_path.open("r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                continue
            
            parts = line.split()
            if parts[0].lower() == "pid":
                continue
            
            if len(parts) < 4:
                raise ValueError(
                    f"Malformed workload line in {workload_path}: {raw_line.rstrip()}"
                )

            try:
                arrival = float(parts[1])
                burst = float(parts[2])
            except ValueError as exc:
                raise ValueError(
                    f"Non-numeric arrival/burst in {workload_path}: {raw_line.rstrip()}"
                ) from exc

            arrivals.append(arrival)
            bursts.append(burst)

    if not bursts:
        raise ValueError(f"No processes found in workload file: {workload_path}")

    arrivals_sorted = sorted(arrivals)
    if len(arrivals_sorted) <= 1:
        avg_arrival_gap = 0.0
    else:
        gaps = [
            arrivals_sorted[i] - arrivals_sorted[i - 1]
            for i in range(1, len(arrivals_sorted))
        ]
        avg_arrival_gap = sum(gaps) / len(gaps)

    avg_burst = sum(bursts) / len(bursts)
    variance = sum((b - avg_burst) ** 2 for b in bursts) / len(bursts)
    std_burst = math.sqrt(variance)

    return {
        "num_processes": float(len(bursts)),
        "avg_burst": round(float(avg_burst), 1),
        "max_burst": float(max(bursts)),
        "min_burst": float(min(bursts)),
        "std_burst": round(float(std_burst), 1),
        "avg_arrival_gap": round(float(avg_arrival_gap), 1),
    }

def main(argv: List[str]) -> int:
    if len(argv) not in (2,3):
        usage()
        return 1
    
    source_csv = Path(argv[1])
    target_csv = Path("rr_training_data.csv")
    
    if not source_csv.exists():
        print(f"Error: Source file '{source_csv}' does not exist.")
        return 1

    with source_csv.open("r", encoding="utf-8") as f:
        reader = csv.reader(f)
        results = list(reader)
    # Expected CSV format: workload_path, quantum, avg_turnaround_time
    workloads = {}
    for row in results:
        if len(row) < 3:
            print(f"Warning: Skipping malformed row: {row}")
            continue
        # Strip whitespace and parse values
        workload = row[0].strip() 
        quantum = int(row[1].strip())  
        avg_turnaround = float(row[2].strip())  
        # Group by workload to find the best quantum for each workload
        if workload not in workloads:
            workloads[workload] = []
        workloads[workload].append((quantum, avg_turnaround))
    # For each workload, determine the quantum with the lowest average turnaround time and extract workload characteristics
    training_data = []
    for workload_str, entries in workloads.items():
        # Find the quantum with the minimum turnaround time
        best_quantum = min(entries, key=lambda x: x[1])[0]
        workload_path = Path(workload_str)
        try:
            characteristics = parse_workload(workload_path)
            training_data.append({
                "num_processes": characteristics["num_processes"],
                "avg_burst": characteristics["avg_burst"],
                "max_burst": characteristics["max_burst"],
                "min_burst": characteristics["min_burst"],
                "std_burst": characteristics["std_burst"],
                "avg_arrival_gap": characteristics["avg_arrival_gap"],
                "optimal_quantum": best_quantum,
            })
        except (ValueError, FileNotFoundError) as exc:
            print(f"Warning: Skipping workload '{workload_str}' due to error: {exc}")

    if not training_data:
        print("Error: No training data was generated. Check your workload paths.")
        return 1
    # Write the training data to the target CSV file
    with target_csv.open("w", encoding="utf-8", newline="") as f:
        fieldnames = [
            "num_processes", "avg_burst", "max_burst", "min_burst", 
            "std_burst", "avg_arrival_gap", "optimal_quantum"
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(training_data)
        
    print(f"Success: Wrote {len(training_data)} records to {target_csv}.")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))