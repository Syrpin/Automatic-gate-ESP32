# Current-sensor decision

The primary motor-current sensor for the gate controller is the **ZMCT103C** current transformer. The firmware configuration on `main` already selects ZMCT103C by default (`CT_SEL_ZMCT`) and uses these nominal settings:

- Effective sensitivity: `22 mV/A` (`ct_k_mv_per_a`)
- ADC reference: `3300 mV` (`ct_vref_mv`)
- Full-scale current: `30 A` (`ct_i_full_a`)
- Calculated signal gain: approximately `2.20`

The effective sensitivity is the design value for the complete CT + burden resistor + buffer-amplifier chain. It must be verified and calibrated against the assembled hardware before relying on current thresholds for motor protection.

This decision is authoritative for sensor selection and supersedes any older recommendations in `SCHEMATIC_2026.md` that describe a Hall-effect sensor as the primary motor-current sensor. The Hall sensor may remain an optional alternative, but it is not the primary sensor.