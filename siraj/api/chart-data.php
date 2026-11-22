<?php
require_once __DIR__.'/../includes/security.php';
require_once __DIR__.'/../config/db.php';
requireLogin();
header('Cache-Control: no-store');
$areaId = (int)($_GET['area_id'] ?? 0);
if (!$areaId || (!isAdmin() && (int)($_SESSION['user_area'] ?? 0) !== $areaId)) {
    http_response_code(403); header('Content-Type: application/json');
    echo json_encode(['error'=>'Access denied']); exit;
}
$stmt=db()->prepare('SELECT a.AreaName FROM lamp l JOIN area a ON a.AreaID=l.AreaID WHERE l.LampID=1 AND l.AreaID=?');
$stmt->execute([$areaId]); $areaName=$stmt->fetchColumn();
if ($areaName === false) {
    if (($_GET['format'] ?? '') === 'csv') { header('Location: ../analytics-export.php?area_id='.$areaId); exit; }
    // Existing database history remains available for areas without the cloud lamp.
    require __DIR__.'/chart-data-legacy.php'; exit;
}
header('Content-Type: application/json; charset=utf-8');
require_once __DIR__.'/../siraj_cloud_client.php';
try {
    $limit=($_GET['weeks'] ?? '4') === 'all' ? 52 : max(1,min(52,(int)($_GET['weeks'] ?? 4)));
    $data=sirajCloudGet('/v1/weekly',['lamp_id'=>1,'weeks'=>$limit]);
    if (($data['status'] ?? '') !== 'ok' || !is_array($data['weeks'] ?? null)) throw new RuntimeException('Invalid response');
    $labels=$ambient=$lux=$table=[];
    foreach ($data['weeks'] as $row) {
        $date=new DateTimeImmutable($row['week_start'],new DateTimeZone('Asia/Riyadh'));
        $label=$date->format('d M Y').' - '.$date->modify('+6 days')->format('d M Y');
        if ($row['in_progress']) $label.=' (in progress)';
        $labels[]=$label; $ambient[]=(float)$row['avg_ambient']; $lux[]=(float)$row['avg_road'];
        $table[]=['week_label'=>$label,'avg_ambient'=>(float)$row['avg_ambient'],
            'avg_lux'=>(float)$row['avg_road'],'reading_count'=>(int)$row['reading_count'],
            'pollution_level'=>'Not assessed','first_received'=>$row['first_received'],
            'last_received'=>$row['last_received']];
    }
    if (($_GET['format'] ?? '') === 'csv') {
        header('Content-Type: text/csv; charset=utf-8');
        header('Content-Disposition: attachment; filename="siraj-lamp1-weekly.csv"');
        $out=fopen('php://output','w');
        fputcsv($out,['LampID','Week (Asia/Riyadh)','Avg ambient lux','Avg road lux','Received samples','First received','Last received']);
        foreach ($table as $r) fputcsv($out,[1,$r['week_label'],$r['avg_ambient'],$r['avg_lux'],$r['reading_count'],$r['first_received'],$r['last_received']]);
        fclose($out); exit;
    }
    echo json_encode(['area_id'=>$areaId,'area_name'=>$areaName,'labels'=>$labels,'ambient'=>$ambient,'lux'=>$lux,'table'=>$table,
        'source'=>'render','scope'=>'Lamp 1 only','timezone'=>'Asia/Riyadh']);
} catch (Throwable $e) {
    http_response_code(503);
    echo json_encode(['error'=>'Cloud weekly data unavailable. Check the Render update and website read key.','labels'=>[],'ambient'=>[],'lux'=>[],'table'=>[]]);
}
