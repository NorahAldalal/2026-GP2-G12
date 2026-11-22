<?php
// ================================================================
//  SIRAJ — Light Pollution Classification (single source of truth)
//
//  Classification is based on WEEKLY averages (weekly_reading.AvgAmbient),
//  not on instantaneous readings.
//  Thresholds (lux, BH1750 ground-level illuminance):
//    Low    : lux <= 6          (Schirmer et al. 2019 — ecological threshold)
//    Medium : 6  < lux <= 20    (within CIE residential road limit — Fotios & Gibbons 2018)
//    High   : lux > 20          (exceeds CIE limit => severe light pollution)
// ================================================================

const POLLUTION_ECOLOGICAL_LUX = 6.0;
const POLLUTION_CIE_LUX        = 20.0;

// Classify a single lux value into 'Low' | 'Medium' | 'High'
function pollutionLevelFromLux(float $lux): string
{
    if ($lux <= POLLUTION_ECOLOGICAL_LUX) return 'Low';
    if ($lux <= POLLUTION_CIE_LUX)        return 'Medium';
    return 'High';
}

// Recalculate an area's level from its MOST RECENT weekly_reading row
// (AvgAmbient = weekly average of the BH1750 ambient-light readings)
// and store it in Area.Pollution_level. Weeks with no readings are ignored.
function refreshAreaPollution(PDO $pdo, int $areaId): ?string
{
    $s = $pdo->prepare('SELECT AvgAmbient FROM weekly_reading
                        WHERE AreaID = ? AND ReadingCount > 0
                        ORDER BY WeekStart DESC LIMIT 1');
    $s->execute([$areaId]);
    $avg = $s->fetchColumn();
    if ($avg === false || $avg === null) return null;   // no weekly data -> keep old value

    $level = pollutionLevelFromLux((float)$avg);
    $pdo->prepare('UPDATE Area SET Pollution_level = ? WHERE AreaID = ?')->execute([$level, $areaId]);
    return $level;
}

// Recalculate every area (used by the weekly cron job)
function refreshAllAreasPollution(PDO $pdo): void
{
    foreach ($pdo->query('SELECT AreaID FROM Area')->fetchAll() as $a) {
        refreshAreaPollution($pdo, (int)$a['AreaID']);
    }
}

// One-statement sync of all areas from their latest weekly average.
// Called automatically on every DB connection so the badge is never stale.
function syncAllAreasPollution(PDO $pdo): void
{
    $lo = POLLUTION_ECOLOGICAL_LUX;
    $hi = POLLUTION_CIE_LUX;
    try {
        $pdo->exec("
            UPDATE Area a
            SET a.Pollution_level = (
                SELECT CASE
                    WHEN w.AvgAmbient <= $lo THEN 'Low'
                    WHEN w.AvgAmbient <= $hi THEN 'Medium'
                    ELSE 'High' END
                FROM weekly_reading w
                WHERE w.AreaID = a.AreaID AND w.ReadingCount > 0
                ORDER BY w.WeekStart DESC LIMIT 1)
            WHERE EXISTS (SELECT 1 FROM weekly_reading w2
                          WHERE w2.AreaID = a.AreaID AND w2.ReadingCount > 0)
        ");
    } catch (Throwable $e) {
        error_log('[SIRAJ] syncAllAreasPollution failed: ' . $e->getMessage());
    }
}
