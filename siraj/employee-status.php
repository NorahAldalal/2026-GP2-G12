<?php
require_once __DIR__ . '/includes/security.php';
require_once __DIR__ . '/config/db.php';
requireEmployee();
$activePage = 'status';
$userId = currentUserId();
$areaId = (int)($_SESSION['user_area'] ?? 0);

if (!$areaId) {
    header('Location: employee-home.php');
    exit;
}

$msg = '';

// ── Submit fault report ───────────────────────────────────
if ($_SERVER['REQUEST_METHOD']==='POST' && isset($_POST['report_lamp'])) {
    $lampId  = (int)$_POST['lamp_id'];
    $details = trim($_POST['details'] ?? '');

    if (empty($details)) {
        $msg = 'error:Please write fault details before submitting.';
    } else {
        // Verify lamp belongs to employee's area
        $check = db()->prepare('SELECT LampID FROM lamp WHERE LampID=? AND AreaID=?');
        $check->execute([$lampId, $areaId]);
        if (!$check->fetch()) {
            $msg = 'error:Invalid lamp selection.';
        } else {
            db()->prepare('INSERT INTO report (LampID, EmployeeID, Details) VALUES (?,?,?)')
               ->execute([$lampId, $userId, $details]);
            $msg = 'success:Report submitted successfully. Your admin has been notified.';
        }
    }
}

// ── Fetch lamps for employee's area ──────────────────────
$filter = $_GET['filter'] ?? 'all';
$sql = 'SELECT l.*, a.AreaName, a.Pollution_level FROM lamp l JOIN area a ON l.AreaID=a.AreaID WHERE l.AreaID=?';
$params = [$areaId];
$sql .= ' ORDER BY l.LampID';
$stmt = db()->prepare($sql); $stmt->execute($params);
$lamps = $stmt->fetchAll();

require_once __DIR__ . '/siraj_status_bridge.php';
$lamps = sirajStatusOverlay($lamps);
$totalOn = count(array_filter($lamps, fn($l) => $l['Status'] === 'on'));
$totalOff = count(array_filter($lamps, fn($l) => $l['Status'] === 'off'));
$totalOther = count($lamps) - $totalOn - $totalOff;
if (in_array($filter, ['on', 'off'], true)) {
    $lamps = array_values(array_filter($lamps, fn($l) => $l['Status'] === $filter));
}


// Area info
$areaStmt = db()->prepare('SELECT AreaName FROM area WHERE AreaID=?'); $areaStmt->execute([$areaId]);
$areaName = $areaStmt->fetchColumn();

[$msgType, $msgText] = $msg ? explode(':', $msg, 2) : ['',''];
?>
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/><meta name="viewport" content="width=device-width,initial-scale=1.0"/>
  <title>Lamp Status — SIRAJ</title>
  <link rel="stylesheet" href="assets/css/global.css"/>
  <link rel="stylesheet" href="assets/css/dashboard.css"/>
</head>
<body class="dashboard-page">
<?php include 'includes/nav.php'; ?>
<div class="dashboard-content">
  <div class="page-header">
    <div class="page-header-left">
      <h1>Lamp Status</h1>
      <p>📍 <?= htmlspecialchars($areaName) ?> — Page loaded: <?= date('H:i:s') ?></p>
    </div>
    <button id="refresh-status" class="btn btn-outline">🔄 Refresh</button>
  </div>

  <?php if ($msgText): ?>
    <div class="alert alert-<?= $msgType ?> visible" style="margin-bottom:16px;"><?= htmlspecialchars($msgText) ?></div>
  <?php endif; ?>

  <div id="siraj-live-status"><div class="status-bar">
    <div class="status-badge on-badge"><span class="status-dot on"></span> <?= $totalOn ?> On</div>
    <div class="status-badge off-badge"><span class="status-dot off"></span> <?= $totalOff ?> Off</div>
    <div class="status-badge"><?= $totalOther ?> Fault / Unknown</div>
  </div>

  <div class="filter-bar">
    <a href="?filter=all" class="filter-btn <?= $filter==='all'?'active':'' ?>">All</a>
    <a href="?filter=on"  class="filter-btn <?= $filter==='on' ?'active':'' ?>">● On</a>
    <a href="?filter=off" class="filter-btn <?= $filter==='off'?'active':'' ?>">● Off</a>
  </div>

  <?php if (empty($lamps)): ?>
    <div style="text-align:center;padding:60px;color:var(--secondary);"><div style="font-size:40px;margin-bottom:12px;">💡</div><p>No lamps found.</p></div>
  <?php else: ?>
  <div class="lamp-grid">
    <?php foreach ($lamps as $l): $isOn=$l['Status']==='on'; ?>
    <div class="lamp-card <?= $isOn?'is-on':'is-off' ?>" <?= $l['Status'] === 'unknown' ? 'style="border-color:#888"' : '' ?>>
      <div class="lamp-id">Lamp #<?= (int)$l['LampID'] ?></div>
      <div class="lamp-area">📍 <?= htmlspecialchars($l['AreaName']) ?></div>
      <div class="lamp-status-row">
        <span class="status-dot <?= $isOn ? 'on' : 'off' ?>" <?= $l['Status'] === 'unknown' ? 'style="background:#888"' : '' ?>></span>
        <?= sirajEscape(sirajStatusText($l)) ?>
      </div>
      <?php sirajStatusDetails($l); ?>
      <div class="lamp-lux"><?= $l['_cloud'] ? 'Ambient lux' : 'Lux (database)' ?>: <strong><?= sirajNumber($l['Lux_Value']) ?></strong></div>
      <div style="margin-top:6px;"><span class="pollution-badge <?= $l['Pollution_level'] ?>"><?= $l['Pollution_level'] ?></span></div>

      <!-- Report button — always visible, highlighted when lamp is off -->
      <div class="lamp-actions">
        <button class="btn btn-danger btn-sm btn-full"
                onclick="openReportModal(<?= (int)$l['LampID'] ?>, <?= sirajEscape(json_encode($l['AreaName'])) ?>)">
          🚨 Report Fault
        </button>
      </div>
    </div>
    <?php endforeach; ?>
  </div>
  <?php endif; ?>
</div><!-- siraj-live-status -->
</div>

<!-- ── REPORT FAULT MODAL ── -->
<div class="modal-overlay" id="report-modal">
  <div class="modal-box" style="max-width:460px;">
    <button class="modal-close">✕</button>
    <div style="text-align:center;margin-bottom:16px;">
      <div style="font-size:40px;">🚨</div>
    </div>
    <div class="modal-title">Report Lamp Fault</div>
    <p class="modal-sub" id="report-modal-sub">Lamp details will appear here.</p>

    <form method="POST">
      <input type="hidden" name="report_lamp" value="1"/>
      <input type="hidden" name="lamp_id" id="report-lamp-id"/>

      <div class="form-group">
        <label class="form-label">Fault Details *</label>
        <textarea class="report-textarea" name="details" id="report-details"
                  placeholder="Describe the fault: e.g. lamp is completely off, flickering, damaged physically, wrong lux level, etc."
                  required></textarea>
      </div>

      <div style="background:rgba(224,92,92,.07);border:1px solid rgba(224,92,92,.2);border-radius:var(--radius-sm);padding:12px;font-size:12px;color:var(--secondary);margin-bottom:18px;">
        📋 This report will be sent immediately to your admin for review.
      </div>

      <div style="display:flex;gap:10px;">
        <button type="submit" class="btn btn-danger" style="flex:1;">🚨 Send Report</button>
        <button type="button" class="btn btn-outline" onclick="closeModal('report-modal')">Cancel</button>
      </div>
    </form>
  </div>
</div>

<?php include 'includes/footer.php'; ?>
<script src="assets/js/main.js"></script>
<script src="siraj_status_refresh.js"></script>
<script>
function openReportModal(lampId, areaName) {
  document.getElementById('report-lamp-id').value   = lampId;
  document.getElementById('report-modal-sub').textContent = `Lamp #${lampId} — 📍 ${areaName}`;
  document.getElementById('report-details').value   = '';
  openModal('report-modal');
}
</script>
</body>
</html>
