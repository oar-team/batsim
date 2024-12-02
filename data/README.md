# Carbon Trace CSV Format

This document describes the standard format for carbon trace CSV files used in Batsim for carbon footprint tracking.

## Format Requirements

The carbon trace CSV file should contain the following columns:

1. **host_id**: The ID of the host
1. **timestamp**: Unix timestamp in seconds
2. **carbon_intensity**: Value in gCO2eq/kWh (grams of CO2 equivalent per kilowatt-hour)

### Specifications:

- File must be in CSV format with comma (,) as the delimiter
- The first line should be a header row with column names
- Timestamps should be ordered chronologically
- Carbon intensity values must be non-negative numeric values

## Example:

```csv
host_id,timestamp,carbon_intensity
Host1,1609459200,45.2
Host2,1609462800,42.8
Host1,1609466400,38.5
Host3,1609470000,35.9
Host3,1609473600,39.7
```

In this example:
- First column: Host identifier
- Second column: Unix timestamps (representing time points at 1-hour intervals)
- Third column: Carbon intensity values in gCO2eq/kWh

## Usage Notes:

- When carbon footprint tracking is enabled, each machine in the platform file must have a `carbon_intensity` property
- The carbon trace can be used to model time-varying carbon intensity for more realistic environmental impact simulations
- For constant carbon intensity, use the value in the platform file
- For dynamic carbon intensity, the CSV file values will override the platform file values at the specified timestamps
- **⚠️ Be careful with incomplete rows or empty cells in the CSV file — they may cause the simulation to behave incorrectly or yield unexpected results. Always ensure that each row has valid values for all required fields.**