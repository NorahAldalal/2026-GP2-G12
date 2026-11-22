<?php
require_once __DIR__ . '/includes/security.php';
require_once __DIR__ . '/config/db.php';
requireEmployee();
$activePage = 'map';
$areaId = (int)($_SESSION['user_area'] ?? 0);
if (!$areaId) { header('Location: employee-home.php'); exit; }

$areaStmt = db()->prepare('SELECT * FROM area WHERE AreaID=?');
$areaStmt->execute([$areaId]); $area = $areaStmt->fetch();

// Check if this area has weekly data
$hasData = false;
try {
    $dc = db()->prepare('SELECT COUNT(*) FROM weekly_reading WHERE AreaID=?');
    $dc->execute([$areaId]);
    $hasData = (int)$dc->fetchColumn() > 0;
} catch(Exception $e) { $hasData = false; }

$lampsStmt = db()->prepare('SELECT l.*,a.AreaName,a.Latitude,a.Longitude,a.Pollution_level FROM lamp l JOIN area a ON l.AreaID=a.AreaID WHERE l.AreaID=?');
$lampsStmt->execute([$areaId]); $lamps = $lampsStmt->fetchAll();

require_once __DIR__.'/siraj_map_bridge.php';
$mapData = sirajMapData($lamps);
if (isset($_GET['siraj_map_data'])) {
    header('Content-Type: application/json; charset=utf-8');
    header('Cache-Control: no-store');
    echo json_encode(['lamps'=>$mapData], JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_INVALID_UTF8_SUBSTITUTE);
    exit;
}
$lampsJson = json_encode($mapData, JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_INVALID_UTF8_SUBSTITUTE);

?>
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/><meta name="viewport" content="width=device-width,initial-scale=1.0"/>
<title>My Area Map — SIRAJ</title>
<link rel="stylesheet" href="assets/css/global.css"/>
<link rel="stylesheet" href="assets/css/dashboard.css"/>
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<style>
.leaflet-popup-content-wrapper{background:var(--primary);color:white;}
.leaflet-popup-tip{background:var(--primary);}
.chart-label{font-size:10px;font-weight:700;letter-spacing:1.5px;text-transform:uppercase;color:var(--glow);margin-bottom:10px;}
.chart-canvas-wrap{position:relative;height:130px;}
.chart-loading{display:flex;align-items:center;justify-content:center;height:130px;font-size:12px;color:var(--text-muted);}
.view-more-btn{display:block;width:100%;margin-top:10px;padding:8px;background:rgba(74,144,184,.10);border:1px solid rgba(74,144,184,.25);color:var(--glow);font-size:12px;font-weight:700;font-family:'Lato',sans-serif;cursor:pointer;transition:var(--transition);}
.view-more-btn:hover{background:rgba(74,144,184,.20);}
.analytics-modal{position:fixed;inset:0;z-index:600;background:rgba(3,8,18,.85);backdrop-filter:blur(10px);display:flex;align-items:center;justify-content:center;opacity:0;pointer-events:none;transition:opacity .3s;}
.analytics-modal.open{opacity:1;pointer-events:all;}
.analytics-modal-box{background:rgba(10,20,40,.97);border:1px solid rgba(126,200,227,.14);width:min(880px,95vw);max-height:88vh;overflow-y:auto;position:relative;transform:scale(.92) translateY(20px);transition:transform .35s cubic-bezier(.175,.885,.32,1.275);padding:36px;}
.analytics-modal.open .analytics-modal-box{transform:scale(1) translateY(0);}
body.light-mode .analytics-modal-box{background:rgba(255,255,255,.98);border-color:rgba(13,27,46,.10);}
.mcls{position:absolute;top:14px;right:16px;width:30px;height:30px;background:rgba(255,255,255,.06);border:1px solid rgba(255,255,255,.10);cursor:pointer;font-size:14px;display:flex;align-items:center;justify-content:center;color:var(--text-muted);}
.mcls:hover{background:rgba(224,92,92,.15);color:#ff9494;}
.modal-atitle{font-family:'Cinzel',serif;font-size:20px;color:var(--glow-soft);margin-bottom:6px;}
body.light-mode .modal-atitle{color:var(--primary);}
.modal-asub{font-size:13px;color:var(--text-muted);margin-bottom:28px;}
.line-wrap{position:relative;height:270px;margin-bottom:28px;}
.no-data-msg{text-align:center;padding:22px;font-size:13px;color:var(--text-muted);}
.data-badge{font-size:9px;font-weight:700;letter-spacing:.8px;text-transform:uppercase;padding:2px 7px;background:rgba(46,204,138,.15);color:var(--success);border:1px solid rgba(46,204,138,.3);white-space:nowrap;flex-shrink:0;}
.data-badge.no-data{background:rgba(255,255,255,.05);color:var(--text-muted);border-color:rgba(255,255,255,.10);}
body.light-mode .data-badge.no-data{background:rgba(13,27,46,.05);border-color:rgba(13,27,46,.12);}
</style>
</head>
<body class="dashboard-page map-page">
<?php include 'includes/nav.php'; ?>
<div class="map-layout">
  <!-- SIDEBAR -->
  <div class="map-sidebar">
    <div class="map-sidebar-header">
      <div class="map-sidebar-title">My Assigned Area</div>
    </div>
    <div class="map-areas">
      <div class="area-item active">
        <div style="display:flex;align-items:center;justify-content:space-between;gap:6px;">
          <div class="area-name"><?= htmlspecialchars($area['AreaName']) ?></div>
          <?php if ($hasData): ?>
          <span class="data-badge" title="Analytics data available">Data</span>
          <?php else: ?>
          <span class="data-badge no-data" title="No analytics data yet">No Data</span>
          <?php endif; ?>
        </div>
        <div class="area-meta">
          <span class="pollution-badge <?= $area['Pollution_level'] ?>"><?= $area['Pollution_level'] ?></span>
          <?= count($lamps) ?> lamps
        </div>
      </div>
    </div>


    <!-- Filters -->
    <div style="padding:14px 16px;border-top:1px solid rgba(255,255,255,.07);">
      <div class="map-sidebar-title">FILTER</div>
      <label style="display:flex;align-items:center;gap:8px;font-size:13px;color:var(--text-muted);margin-bottom:8px;cursor:pointer;"><input type="checkbox" id="show-on" checked style="accent-color:var(--success);"> On</label>
      <label style="display:flex;align-items:center;gap:8px;font-size:13px;color:var(--text-muted);cursor:pointer;"><input type="checkbox" id="show-off" checked style="accent-color:var(--danger);"> Off</label>
      <label style="display:block;font-size:13px;margin-top:8px"><input type="checkbox" id="show-fault" checked> Fault</label>
      <label style="display:block;font-size:13px;margin-top:8px"><input type="checkbox" id="show-unknown" checked> Unknown / stale</label>
      <button type="button" class="btn btn-outline btn-sm" id="siraj-map-refresh" style="margin-top:12px">Refresh</button>
      <p id="siraj-map-notice" role="status" style="font-size:12px">Lamp 1: cloud readings. Other lamps: stored database values.</p>

    </div>
    <div style="padding:14px 16px;">
      <a href="employee-status.php" class="btn btn-danger btn-sm btn-full">Report a Fault</a>
    </div>
  </div>

  <!-- MAP -->
  <div class="map-view">
    <div id="map"></div>
    <div class="map-legend">
      <div class="legend-item"><div class="legend-dot" style="background:var(--success)"></div>On</div>
      <div class="legend-item"><div class="legend-dot" style="background:var(--danger)"></div>Off</div>
      <div class="legend-item"><div class="legend-dot" style="background:#F0B429"></div>Fault</div>
      <div class="legend-item"><div class="legend-dot" style="background:#9299A5"></div>Unknown</div>
    </div>
  </div>
</div>



<?php include 'includes/footer.php'; ?>
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script src="assets/js/main.js"></script>
<script src="siraj_map_live.js"></script>
<script>
const LAMPS=<?= $lampsJson ?>, AID=<?= $areaId ?>;
const map=L.map('map',{center:[<?= $area['Latitude'] ?>,<?= $area['Longitude'] ?>],zoom:15});
L.tileLayer(
  'https://basemaps.cartocdn.com/rastertiles/dark_all/{z}/{x}/{y}{r}.png?key=cb1_4gl1_1_2adfd4a971d6865cf9f70714',
  {
    attribution:
      '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors &copy; <a href="https://carto.com/attribution/">CARTO</a>',
    maxZoom: 20
  }
).addTo(map);
startSirajLiveMap(map, LAMPS);
</script>
</body>
</html>