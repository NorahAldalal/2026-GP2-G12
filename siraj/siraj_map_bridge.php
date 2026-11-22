<?php
require_once __DIR__.'/siraj_status_bridge.php';
function sirajMapData(array $lamps): array {
    return array_map(function ($l) {
        $r = $l['_reading'] ?? []; $d = $l['_decision'] ?? [];
        return [
            'id'=>(int)$l['LampID'], 'status'=>$l['Status'],
            'label'=>sirajStatusText($l), 'cloud'=>(bool)$l['_cloud'],
            'lux'=>is_numeric($l['Lux_Value']) ? (float)$l['Lux_Value'] : null,
            'lat'=>(float)$l['Latitude']+(float)($l['offset_lat'] ?? 0),
            'lng'=>(float)$l['Longitude']+(float)($l['offset_lng'] ?? 0),
            'area'=>(string)$l['AreaName'],
            'pwm'=>$r['lamp_pct'] ?? null, 'command'=>$d['pwm'] ?? null,
            'battery'=>$r['battery_v'] ?? null, 'road_lux'=>$r['lux_road'] ?? null,
            'received'=>$l['_received'] ?? ''
        ];
    }, sirajStatusOverlay($lamps));
}
