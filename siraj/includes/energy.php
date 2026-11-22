<?php
// ================================================================
//  SIRAJ — Monthly Energy Saving (system-wide)
//
//  Benchmark: a conventional scheduled lighting system runs 100% for
//  12 h and 0% for 12 h  =>  24 h average = 50%.
//
//    Monthly_Energy_Saved_Pct = (1 - (Monthly_Avg_PWM_24h / 50)) * 100
//
//  Source: weekly_reading.AvgLux (weekly average lamp output, lux/ADC units).
//  It is converted to a PWM duty % :  PWM% = AvgLux / ENERGY_MAX_LUX * 100
//  (ENERGY_MAX_LUX = lamp output at 100% duty — adjust to your hardware).
//  Monthly_Avg_PWM_24h = mean of the system-wide weekly averages (all areas)
//  of the last 4 recorded weeks (fewer if fewer exist).
//
//  The result is computed live on each page load — it is not stored in a table.
// ================================================================

const ENERGY_BASELINE_PWM = 50.0;
const ENERGY_MONTH_WEEKS  = 4;
const ENERGY_MAX_LUX      = 255.0;   // AvgLux value that equals 100% PWM

// Pure formula: returns 0.0–100.0, rounded to 1 decimal
function energySavedPct(float $avgPwm): float
{
    $pct = (1 - ($avgPwm / ENERGY_BASELINE_PWM)) * 100;
    return round(max(0.0, min(100.0, $pct)), 1);
}

// Returns ['pct' => ?float, 'weeks' => int, 'avg_pwm' => ?float]
// pct = null when there is no usable data (-> show "N/A").
function getMonthlyEnergySaving(PDO $pdo): array
{
    $none = ['pct' => null, 'weeks' => 0, 'avg_pwm' => null];
    try {
        // System-wide average per week (all areas), newest 4 weeks that have PWM data
        $rows = $pdo->query('
            SELECT WeekStart, AVG(LEAST(100, AvgLux / ' . ENERGY_MAX_LUX . ' * 100)) AS week_pwm
            FROM weekly_reading
            WHERE AvgLux IS NOT NULL AND ReadingCount > 0
            GROUP BY WeekStart
            ORDER BY WeekStart DESC
            LIMIT ' . ENERGY_MONTH_WEEKS
        )->fetchAll();
    } catch (Throwable $e) {
        return $none;   // column/table missing -> N/A instead of an error
    }
    if (!$rows) return $none;

    $avg = array_sum(array_column($rows, 'week_pwm')) / count($rows);
    return [
        'pct'     => energySavedPct((float)$avg),
        'weeks'   => count($rows),
        'avg_pwm' => round((float)$avg, 1),
    ];
}

// Shared HTML card (used by admin-home and employee-home)
function renderEnergyCard(array $e): string
{
    if ($e['pct'] === null) {
        $value = 'N/A';
        $sub   = 'لا توجد بيانات كافية · Not enough data';
    } else {
        $value = number_format($e['pct'], 1) . '%';
        $sub   = ($e['weeks'] >= ENERGY_MONTH_WEEKS)
               ? 'Based on the last 4 weeks'
               : 'Based on ' . $e['weeks'] . ' week' . ($e['weeks'] > 1 ? 's' : '') . ' of data (less than 4)';
        $sub  .= ' · Avg PWM ' . number_format($e['avg_pwm'], 1) . '% vs 50% conventional';
    }
    $bar = $e['pct'] === null ? 0 : $e['pct'];
    return '<div class="energy-card">'
         . '<div class="energy-head"><span class="energy-icon">⚡</span>'
         . '<span class="energy-title">Monthly Energy Savings — Entire System</span></div>'
         . '<div class="energy-value">' . $value . '</div>'
         . '<div class="energy-bar"><div class="energy-bar-fill" style="width:' . $bar . '%"></div></div>'
         . '<div class="energy-sub">' . htmlspecialchars($sub) . '</div>'
         . '</div>';
}
