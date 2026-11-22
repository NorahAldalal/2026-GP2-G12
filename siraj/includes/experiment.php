<?php
// ================================================================
//  SIRAJ — Light-Pollution Experiment helpers (read-only)
//
//  Reads the latest row of `experiment_result` and its 3 rows of
//  `experiment_cycle`. Nothing here writes to the DB, touches the
//  live-monitoring endpoints, or talks to the hardware.
//
//  Metric (as approved by the team):
//    Cycle reduction  = 100 * (F - M) / (F - B)
//    Overall          = 100 * (1 - SUM(M_i - B_i) / SUM(F_i - B_i))
//  i.e. a pooled ratio, NOT the simple mean of the cycle percentages.
// ================================================================

if (!function_exists('xh')) {
    function xh($v): string { return htmlspecialchars((string)$v, ENT_QUOTES, 'UTF-8'); }
}

// Returns ['exp' => row, 'cycles' => [rows], 'sum_full' => float, 'sum_model' => float]
// or null when the tables are missing / empty (page shows an empty state).
function getLatestExperiment(PDO $pdo): ?array
{
    try {
        $exp = $pdo->query(
            'SELECT e.*, a.AreaName
               FROM experiment_result e
               LEFT JOIN area a ON a.AreaID = e.AreaID
              ORDER BY e.ExperimentDate DESC, e.ExperimentID DESC
              LIMIT 1'
        )->fetch();
        if (!$exp) return null;

        $st = $pdo->prepare(
            'SELECT * FROM experiment_cycle WHERE ExperimentID = ? ORDER BY CycleNumber'
        );
        $st->execute([(int)$exp['ExperimentID']]);
        $cycles = $st->fetchAll();
    } catch (Throwable $e) {
        return null;
    }

    $sumFull = $sumModel = 0.0;
    foreach ($cycles as $c) {
        $sumFull  += (float)$c['FullExcessHz'];
        $sumModel += (float)$c['ModelExcessHz'];
    }
    return ['exp' => $exp, 'cycles' => $cycles, 'sum_full' => $sumFull, 'sum_model' => $sumModel];
}

// Compact card for the two home pages (links to experiment.php).
function renderExperimentCard(?array $x): string
{
    if (!$x || $x['exp']['ReductionPct'] === null) {
        $value = 'N/A';
        $sub   = 'No experiment result recorded yet';
        $pend  = '';
    } else {
        $value = number_format((float)$x['exp']['ReductionPct'], 2) . '%';
        $sub   = 'Sensor-signal reduction vs 100% PWM · '
               . date('d M Y', strtotime($x['exp']['ExperimentDate']));
        $pend  = $x['exp']['PhysicalVerificationStatus'] === 'PENDING_PHYSICAL_CHECK'
               ? '<span class="xp-chip xp-chip-warn">Physical check pending</span>' : '';
    }
    return '<a href="experiment.php" class="xp-home-card">'
         . '<div class="xp-home-main">'
         . '<div class="xp-home-title">Sky-Sensor Experiment — ML vs 100% PWM</div>'
         . '<div class="xp-home-value">' . xh($value) . '</div>'
         . '<div class="xp-home-sub">' . xh($sub) . '</div>'
         . '</div>'
         . '<div class="xp-home-side">' . $pend . '<span class="xp-home-link">View details →</span></div>'
         . '</a>';
}
