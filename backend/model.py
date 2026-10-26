"""Map the restricted database view into the firmware contract; no network side effects."""
from datetime import datetime, timezone
import math


def vehicle_payload(row, now=None):
    if not row:
        raise ValueError('no observation')
    now = now or datetime.now(timezone.utc)
    observed = row['observed_at']
    if observed.tzinfo is None:
        observed = observed.replace(tzinfo=timezone.utc)  # TeslaMate's timestamp columns store UTC.
    age = (now - observed).total_seconds()
    if age < -30:
        raise ValueError('observation is in the future')
    values = {}
    for key, maximum in [('battery_percent', 100), ('range_km', 1500), ('speed_kmh', 350)]:
        value = float(row[key])
        if not math.isfinite(value) or not 0 <= value <= maximum:
            raise ValueError('invalid numeric observation')
        values[key] = value
    state = row['vehicle_state']
    if state not in ('online', 'offline', 'asleep') or row['range_kind'] not in ('estimated', 'rated', 'ideal'):
        raise ValueError('unknown source metadata')
    if row['charging'] and values['speed_kmh'] != 0:
        raise ValueError('inconsistent observation')
    return dict(schema_version=1, source='teslamate', **values,
                charging=bool(row['charging']), observed_at=int(observed.timestamp()),
                data_age_ms=min(4294967295, max(0, int(age * 1000))),
                vehicle_state=state, speed_valid=bool(row['speed_valid']), range_kind=row['range_kind'])
