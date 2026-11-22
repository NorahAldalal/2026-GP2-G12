<?php
require_once __DIR__ . '/includes/security.php';
// admin-status.php — All lamps all areas
require_once __DIR__ . '/config/db.php';
requireAdmin();
$activePage = 'status';

$filter = $_GET['filter'] ?? 'all';
$search = trim($_GET['search'] ?? '');

$sql = 'SELECT l.*, a.AreaName, a.Pollution_level FROM lamp l JOIN area a ON l.AreaID=a.AreaID WHERE 1=1';
$params = [];
if ($search) { $sql .= ' AND (a.AreaName LIKE ? OR l.LampID LIKE ?)'; $params[] = "%$search%"; $params[] = "%$search%"; }
$sql .= ' ORDER BY a.AreaName, l.LampID';
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

?>
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/><meta name="viewport" content="width=device-width,initial-scale=1.0"/>
  <title>Streetlight Status — SIRAJ</title>
  <link rel="stylesheet" href="assets/css/global.css"/>
  <link rel="stylesheet" href="assets/css/dashboard.css"/>
</head>
<body class="dashboard-page">
<?php include 'includes/nav.php'; ?>
<div class="dashboard-content">
  <div class="page-header">
    <div class="page-header-left"><h1>Streetlight Status</h1><p>All lamps across all city areas — Page loaded: <?= date('H:i:s') ?></p></div>
    <button id="refresh-status" class="btn btn-outline">🔄 Refresh</button>
  </div>

  <div id="siraj-live-status"><div class="status-bar">
    <div class="status-badge on-badge"><span class="status-dot on"></span> <?= $totalOn ?> On</div>
    <div class="status-badge off-badge"><span class="status-dot off"></span> <?= $totalOff ?> Off</div>
    <div class="status-badge"><?= $totalOther ?> Fault / Unknown</div>
  </div>

  <form method="GET">
    <div class="filter-bar">
      <button type="submit" name="filter" value="all"  class="filter-btn <?= $filter==='all' ?'active':'' ?>">All</button>
      <button type="submit" name="filter" value="on"   class="filter-btn <?= $filter==='on'  ?'active':'' ?>">● On</button>
      <button type="submit" name="filter" value="off"  class="filter-btn <?= $filter==='off' ?'active':'' ?>">● Off</button>
      <input type="text" name="search" class="search-input" placeholder="Search area or lamp…" value="<?= htmlspecialchars($search) ?>" onchange="this.form.submit()"/>
    </div>
  </form>

  <?php if (empty($lamps)): ?>
    <div style="text-align:center;padding:60px;color:var(--secondary);"><div style="font-size:40px;margin-bottom:12px;">💡</div><p>No lamps found.</p></div>
  <?php else: ?>
  <div class="lamp-grid">
    <?php foreach ($lamps as $l): $isOn=$l['Status']==='on'; ?>
    <div class="lamp-card <?= $isOn?'is-on':'is-off' ?>" <?= $l['Status'] === 'unknown' ? 'style="border-color:#888"' : '' ?>>
      <div class="lamp-id">Lamp #<?= (int)$l['LampID'] ?></div>
      <div class="lamp-area">📍 <?= htmlspecialchars($l['AreaName']) ?></div>
      <div class="lamp-status-row"><span class="status-dot <?= $isOn ? 'on' : 'off' ?>" <?= $l['Status'] === 'unknown' ? 'style="background:#888"' : '' ?>></span><?= sirajEscape(sirajStatusText($l)) ?></div>
      <?php sirajStatusDetails($l); ?>
      <div class="lamp-lux"><?= $l['_cloud'] ? 'Ambient lux' : 'Lux (database)' ?>: <strong><?= sirajNumber($l['Lux_Value']) ?></strong></div>
      <div style="margin-top:8px;"><span class="pollution-badge <?= $l['Pollution_level'] ?>"><?= $l['Pollution_level'] ?></span></div>
    </div>
    <?php endforeach; ?>
  </div>
  <?php endif; ?>
</div><!-- siraj-live-status -->
</div>
<?php include 'includes/footer.php'; ?>
<script src="assets/js/main.js"></script>
<script src="siraj_status_refresh.js"></script>
</body>
</html>
