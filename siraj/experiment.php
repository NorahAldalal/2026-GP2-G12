<?php
// ================================================================
//  SIRAJ — Light-Pollution Reduction Experiment
//  Read-only results page (admin + employee).
//  Shows the field experiment comparing the ML model against the
//  conventional 100% PWM reference, using the sky-sensor frequency.
// ================================================================

require_once __DIR__ . '/includes/security.php';
require_once __DIR__ . '/config/db.php';
require_once __DIR__ . '/includes/experiment.php';

requireLogin();
$activePage = 'experiment';

$x = getLatestExperiment(db());

if ($x) {
    $e       = $x['exp'];
    $cycles  = $x['cycles'];
    $pct     = (float)$e['ReductionPct'];
    $remain  = max(0, min(100, 100 - $pct));
    $pending = $e['PhysicalVerificationStatus'] === 'PENDING_PHYSICAL_CHECK';
    $numOk   = $e['NumericalStatus'] === 'NUMERIC_PASS';
    $dur     = ($e['StartedAt'] && $e['EndedAt'])
             ? max(1, (int)round((strtotime($e['EndedAt']) - strtotime($e['StartedAt'])) / 60)) : null;
    $chart   = [
        'labels' => array_map(fn($c) => 'Cycle ' . $c['CycleNumber'], $cycles),
        'off'    => array_map(fn($c) => (float)$c['OffHz'],   $cycles),
        'full'   => array_map(fn($c) => (float)$c['FullHz'],  $cycles),
        'model'  => array_map(fn($c) => (float)$c['ModelHz'], $cycles),
    ];
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>Light-Pollution Experiment — SIRAJ</title>
  <link rel="stylesheet" href="assets/css/global.css"/>
  <link rel="stylesheet" href="assets/css/dashboard.css"/>
  <link rel="stylesheet" href="assets/css/experiment.css"/>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
</head>
<body class="dashboard-page">
<?php include 'includes/nav.php'; ?>

<div class="dashboard-content xp-page">

<?php if (!$x): ?>
  <div class="page-header"><div class="page-header-left">
    <h1>Light-Pollution Experiment</h1>
    <p>ML model vs 100% PWM · sky-sensor field test</p>
  </div></div>
  <div class="xp-card xp-empty">
    No experiment result has been recorded yet.<br>
    <small>Import the <code>experiment_result</code> and <code>experiment_cycle</code> tables in phpMyAdmin.</small>
  </div>

<?php else: ?>

  <!-- ── Header ─────────────────────────────────────────── -->
  <div class="page-header">
    <div class="page-header-left">
      <h1>Light-Pollution Experiment</h1>
      <p>
        <?= xh($e['AreaName'] ?? ('Area #' . $e['AreaID'])) ?> ·
        <?= xh(date('d M Y', strtotime($e['ExperimentDate']))) ?>
        <?php if ($e['StartedAt']): ?> · <?= xh(date('H:i', strtotime($e['StartedAt']))) ?>–<?= xh(date('H:i', strtotime($e['EndedAt']))) ?> (<?= xh($e['SourceTimezone']) ?>)<?php endif; ?>
      </p>
    </div>
    <div class="xp-chips">
      <span class="xp-chip <?= $numOk ? 'xp-chip-ok' : 'xp-chip-bad' ?>">
        <?= $numOk ? '✓ Numeric checks passed' : '✕ ' . xh($e['NumericalStatus']) ?>
      </span>
      <span class="xp-chip <?= $pending ? 'xp-chip-warn' : 'xp-chip-info' ?>">
        <?= $pending ? '◔ Physical check pending' : xh($e['PhysicalVerificationStatus']) ?>
      </span>
    </div>
  </div>

  <!-- ── Hero result ────────────────────────────────────── -->
  <section class="xp-card xp-hero">
    <div class="xp-hero-num">
      <div class="xp-eyebrow">Sensor-signal reduction vs 100% PWM</div>
      <div class="xp-big"><?= number_format($pct, 2) ?><span>%</span></div>
      <p class="xp-hero-note">
        Drop in the sky-sensor signal above the natural background, measured while the
        ML model controlled the lamp during real traffic, compared with running it at full power.
      </p>
    </div>

    <div class="xp-hero-viz" role="img"
         aria-label="Model signal is <?= number_format($remain, 2) ?>% of the 100% PWM signal">
      <div class="xp-bar-row">
        <div class="xp-bar-label"><span>Conventional · 100% PWM</span><b>100%</b></div>
        <div class="xp-track"><div class="xp-fill xp-fill-ref" style="width:100%"></div></div>
      </div>
      <div class="xp-bar-row">
        <div class="xp-bar-label"><span>SIRAJ · ML model</span><b><?= number_format($remain, 2) ?>%</b></div>
        <div class="xp-track">
          <div class="xp-fill xp-fill-model" style="width:<?= $remain ?>%"></div>
          <div class="xp-fill xp-fill-saved" style="width:<?= $pct ?>%">−<?= number_format($pct, 2) ?>%</div>
        </div>
      </div>
      <div class="xp-viz-foot">Share of the sensor signal above background (ambient OFF level = 0)</div>
    </div>
  </section>

  <!-- ── Verification status (always visible, not a tooltip) ── -->
  <section class="xp-card xp-verify <?= $pending ? 'is-pending' : '' ?>">
    <div class="xp-verify-icon"><?= $pending ? '⚠' : 'ℹ' ?></div>
    <div>
      <h2>Verification status</h2>
      <?php if ($pending): ?>
      <p>
        The numerical checks on this experiment passed. However, this figure is the reduction in the
        <strong>light-sensor signal</strong>, not yet a confirmed reduction of total sky pollution.
        Before it can be generalised that way, a physical field check is still required to confirm that
        the sensor was not affected by side reflections or by vehicle headlights during the run.
      </p>
      <?php else: ?>
      <p>Numerical status: <strong><?= xh($e['NumericalStatus']) ?></strong> ·
         Physical verification: <strong><?= xh($e['PhysicalVerificationStatus']) ?></strong>.</p>
      <?php endif; ?>
    </div>
  </section>

  <!-- ── How it was measured ────────────────────────────── -->
  <section class="xp-card">
    <h2 class="xp-h">How it was measured</h2>
    <p class="xp-lead">
      The sky sensor reports a frequency (<code>sky_hz</code>, in Hz) that rises with the light it receives.
      Each of the 3 cycles records three states, and the background is re-checked at the end.
    </p>
    <ol class="xp-steps">
      <li><b>B</b><span>Lamp OFF</span><small>Natural background</small></li>
      <li><b>F</b><span>100% PWM</span><small>Full-power reference</small></li>
      <li><b>M</b><span>ML model</span><small>Real model decisions</small></li>
      <li class="xp-step-end"><b>✓</b><span>Lamp OFF</span><small>Background stability check</small></li>
    </ol>
    <div class="xp-formulas">
      <div>
        <div class="xp-f-label">Per cycle</div>
        <code>Reduction % = 100 × (F − M) / (F − B)</code>
      </div>
      <div>
        <div class="xp-f-label">Overall (pooled across the 3 cycles, not a simple average)</div>
        <code>100 × [1 − Σ(M − B) / Σ(F − B)]</code>
        <div class="xp-f-calc">
          = 100 × [1 − <?= number_format($x['sum_model'], 2) ?> / <?= number_format($x['sum_full'], 2) ?>]
          = <strong><?= number_format($pct, 2) ?>%</strong>
          <small>(Σ in Hz above background)</small>
        </div>
      </div>
    </div>
  </section>

  <!-- ── Details: cycles + chart ────────────────────────── -->
  <section class="xp-card">
    <h2 class="xp-h">Experiment details</h2>

    <div class="xp-table-wrap">
      <table class="xp-table">
        <thead>
          <tr>
            <th>Cycle</th>
            <th>OFF mean (Hz)</th>
            <th>100% mean (Hz)</th>
            <th>Model mean (Hz)</th>
            <th>Mean applied PWM (%)</th>
            <th>Calculated reduction (%)</th>
          </tr>
        </thead>
        <tbody>
        <?php foreach ($cycles as $c): ?>
          <tr>
            <td><strong><?= (int)$c['CycleNumber'] ?></strong></td>
            <td><?= number_format((float)$c['OffHz'], 2) ?></td>
            <td><?= number_format((float)$c['FullHz'], 2) ?></td>
            <td><?= number_format((float)$c['ModelHz'], 2) ?></td>
            <td><?= number_format((float)$c['MeanModelPwm'], 2) ?>%</td>
            <td><strong><?= number_format((float)$c['ReductionPct'], 2) ?>%</strong></td>
          </tr>
        <?php endforeach; ?>
        </tbody>
        <tfoot>
          <tr>
            <td colspan="5">Overall (pooled)</td>
            <td><strong><?= number_format($pct, 2) ?>%</strong></td>
          </tr>
        </tfoot>
      </table>
    </div>

    <div class="xp-split">
      <div class="xp-sample">
        <div class="xp-f-label">Model-phase samples</div>
        <div class="xp-sample-grid">
          <div><b><?= (int)$e['RetainedModelSamples'] ?></b><span>stable samples retained</span></div>
          <div><b><?= number_format((float)$e['MeanAppliedPwm'], 2) ?>%</b><span>mean applied light</span></div>
          <div><b><?= number_format((float)$e['MinAppliedPwm'], 0) ?>–<?= number_format((float)$e['MaxAppliedPwm'], 0) ?>%</b><span>applied range</span></div>
        </div>
        <?php if (count($cycles) === 3 && (float)$cycles[2]['ReductionPct'] < (float)$cycles[0]['ReductionPct']): ?>
        <p class="xp-small">
          Cycle <?= (int)$cycles[2]['CycleNumber'] ?> shows a lower reduction
          (<?= number_format((float)$cycles[2]['ReductionPct'], 2) ?>%) because the model applied a higher
          mean output in that cycle (<?= number_format((float)$cycles[2]['MeanModelPwm'], 2) ?>%).
        </p>
        <?php endif; ?>
        <?php if (!empty($e['ScenarioDescription'])): ?>
        <p class="xp-small"><strong>Scenario:</strong> <?= xh($e['ScenarioDescription']) ?></p>
        <?php endif; ?>
      </div>

      <div class="xp-chart-box">
        <div class="xp-f-label">Sensor frequency per cycle</div>
        <div class="xp-chart"><canvas id="xp-chart"></canvas></div>
      </div>
    </div>
  </section>

<?php endif; ?>

  <!-- ── Planned feature (inactive) ─────────────────────── -->
  <section class="xp-card xp-planned">
    <div class="xp-planned-head">
      <h2 class="xp-h" style="margin:0;">Planned: Automated Weekly Monitoring</h2>
      <span class="xp-chip xp-chip-plan">Planned feature — not active in this version</span>
    </div>
    <p>
      Future versions will schedule nightly reference comparisons, check the quality of each measurement,
      and publish a weekly summary of the valid results.
    </p>
  </section>

</div>

<?php include 'includes/footer.php'; ?>
<script src="assets/js/main.js"></script>
<?php if ($x): ?>
<script>
(function () {
  const D = <?= json_encode($chart, JSON_HEX_TAG | JSON_HEX_AMP) ?>;
  let chart = null;

  function colors() {
    const light = document.body.classList.contains('light-mode');
    return {
      text: light ? 'rgba(13,27,46,.80)' : 'rgba(255,255,255,.80)',
      grid: light ? 'rgba(13,27,46,.12)' : 'rgba(255,255,255,.10)',
      off:  light ? '#8795A6' : '#6F8399',
      full: '#E0A93B',
      model:light ? '#2D7AAB' : '#7EC8E3'
    };
  }

  function draw() {
    const c = colors();
    if (chart) chart.destroy();
    chart = new Chart(document.getElementById('xp-chart'), {
      type: 'bar',
      data: {
        labels: D.labels,
        datasets: [
          { label: 'OFF (background)', data: D.off,   backgroundColor: c.off,   borderRadius: 2 },
          { label: '100% PWM',         data: D.full,  backgroundColor: c.full,  borderRadius: 2 },
          { label: 'ML model',         data: D.model, backgroundColor: c.model, borderRadius: 2 }
        ]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: { labels: { color: c.text, font: { size: 12 } } },
          tooltip: { callbacks: { label: ctx => ` ${ctx.dataset.label}: ${ctx.parsed.y.toFixed(2)} Hz` } }
        },
        scales: {
          x: { ticks: { color: c.text }, grid: { display: false } },
          y: {
            beginAtZero: true,
            title: { display: true, text: 'Hz', color: c.text },
            ticks: { color: c.text },
            grid: { color: c.grid }
          }
        }
      }
    });
  }

  draw();
  // Re-draw when the theme toggle flips light/dark
  new MutationObserver(draw).observe(document.body, { attributes: true, attributeFilter: ['class'] });
})();
</script>
<?php endif; ?>
</body>
</html>
