<?php
// Included only AFTER the existing page authorization and area-scoped SQL query.
require_once __DIR__ . '/siraj_cloud_client.php';
function sirajStatusOverlay(array $lamps): array {
    $index = null;
    foreach ($lamps as $i => &$lamp) {
        $lamp['_cloud'] = false;
        if ((int)$lamp['LampID'] === 1) { $index = $i; }
    }
    unset($lamp);
    if ($index === null) { return $lamps; }
    $lamp = &$lamps[$index];
    $lamp['_cloud'] = true;
    $lamp['Status'] = 'unknown';
    $lamp['Lux_Value'] = null;
    $lamp['_label'] = 'Cloud data unavailable';
    $lamp['_reading'] = [];
    $lamp['_decision'] = [];
    $lamp['_received'] = '';
    try {
        $data = sirajCloudGet('/v1/latest', ['lamp_id' => 1]);
        if (($data['status'] ?? '') === 'no_data') {
            $lamp['_label'] = 'No readings received';
            return $lamps;
        }
        $r = $data['record'] ?? null;
        if (($data['status'] ?? '') !== 'ok' || !is_array($r) || (int)($r['lamp_id'] ?? 0) !== 1 || !is_array($r['reading'] ?? null)) {
            throw new RuntimeException('Invalid latest response');
        }
        $reading = $r['reading'];
        $lamp['_reading'] = $reading;
        $lamp['_decision'] = is_array($r['decision'] ?? null) ? $r['decision'] : [];
        $lamp['_received'] = (string)($r['received_at'] ?? '');
        $lamp['Lux_Value'] = $reading['lux_ambient'] ?? null;
        $stamp = strtotime($lamp['_received']);
        $age = $data['age_seconds'] ?? null;
        $fresh = isset($data['stale']) && $data['stale'] === false && is_numeric($age) && $age >= 0 && $age <= 20 && $stamp !== false && time() - $stamp <= 25 && time() - $stamp >= -5;
        if (!$fresh) { $lamp['_label'] = 'Stale readings - device connection unconfirmed'; }
        elseif (!isset($reading['lamp_fault']) || !is_numeric($reading['lamp_fault'])) { $lamp['_label'] = 'Incomplete device status'; }
        elseif ((int)$reading['lamp_fault'] !== 0) { $lamp['Status'] = 'fault'; $lamp['_label'] = 'Device-reported lamp fault'; }
        elseif (in_array($reading['status'] ?? '', ['on', 'off'], true)) { $lamp['Status'] = $reading['status']; $lamp['_label'] = $reading['status'] === 'on' ? 'On' : 'Off'; }
        else { $lamp['_label'] = 'Unknown device status'; }
    } catch (Throwable $e) {
        // Never expose credentials, URLs or backend response bodies to the page.
        $lamp['_label'] = 'Cloud data unavailable';
    }
    return $lamps;
}
function sirajStatusText(array $lamp): string {
    return $lamp['_cloud'] ? $lamp['_label'] : (($lamp['Status'] ?? '') === 'on' ? 'On (database)' : 'Off (database)');
}
function sirajEscape($value): string { return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8'); }
function sirajNumber($value, int $decimals = 1): string { return is_numeric($value) ? number_format((float)$value, $decimals) : 'Unavailable'; }
function sirajStatusDetails(array $lamp): void {
    if (!$lamp['_cloud']) { echo '<div style="font-size:12px">Stored database value; live status not verified</div>'; return; }
    $r = $lamp['_reading']; $d = $lamp['_decision'];
    echo '<div style="font-size:13px;line-height:1.8;margin-top:8px">';
    echo 'Reported PWM: <strong>'.sirajNumber($r['lamp_pct'] ?? null).'</strong> %<br>';
    echo 'Model command: <strong>'.sirajNumber($d['pwm'] ?? null).'</strong> %<br>';
    echo 'Battery: '.sirajNumber($r['battery_v'] ?? null, 2).' V<br>';
    echo 'Road lux: '.sirajNumber($r['lux_road'] ?? null).'<br>';
    echo 'Last received: '.sirajEscape($lamp['_received'] ?: 'Unavailable');
    echo '</div>';
}
