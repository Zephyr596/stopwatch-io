-- Applied by the database owner. The service gets SELECT on this view only.
-- Car 2 is the only populated vehicle in the inspected installation.
CREATE SCHEMA IF NOT EXISTS stopwatch_api;
CREATE OR REPLACE VIEW stopwatch_api.vehicle_snapshot WITH (security_barrier=true) AS
WITH latest_position AS (
  SELECT date AS observed_at, battery_level AS battery_percent,
         COALESCE(est_battery_range_km, rated_battery_range_km, ideal_battery_range_km) AS range_km,
         CASE WHEN est_battery_range_km IS NOT NULL THEN 'estimated'
              WHEN rated_battery_range_km IS NOT NULL THEN 'rated' ELSE 'ideal' END AS range_kind,
         COALESCE(speed, 0) AS speed_kmh, speed IS NOT NULL AS speed_valid,
         false AS charging
  FROM public.positions
  WHERE car_id = 2 AND ideal_battery_range_km IS NOT NULL AND battery_level IS NOT NULL
  ORDER BY date DESC LIMIT 1
), latest_process AS (
  SELECT id, end_date FROM public.charging_processes WHERE car_id = 2 ORDER BY start_date DESC LIMIT 1
), latest_charge AS (
  SELECT c.date AS observed_at, c.battery_level AS battery_percent,
         COALESCE(c.rated_battery_range_km, c.ideal_battery_range_km) AS range_km,
         CASE WHEN c.rated_battery_range_km IS NOT NULL THEN 'rated' ELSE 'ideal' END AS range_kind,
         0 AS speed_kmh, true AS speed_valid,
         (cp.end_date IS NULL AND COALESCE(c.charger_power, 0) > 0) AS charging
  FROM public.charges c JOIN latest_process cp ON cp.id = c.charging_process_id
  WHERE c.battery_level IS NOT NULL AND COALESCE(c.rated_battery_range_km,c.ideal_battery_range_km) IS NOT NULL
  ORDER BY c.date DESC LIMIT 1
), latest_sample AS (
  SELECT * FROM (SELECT * FROM latest_position UNION ALL SELECT * FROM latest_charge) samples
  ORDER BY observed_at DESC LIMIT 1
)
SELECT s.*, COALESCE((SELECT state::text FROM public.states WHERE car_id = 2 ORDER BY start_date DESC LIMIT 1), 'offline') AS vehicle_state
FROM latest_sample s;
