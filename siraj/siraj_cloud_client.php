<?php
// Include this from your existing authenticated dashboard PHP code.
// Do NOT expose the API key in HTML or JavaScript.
function sirajCloudGet(string $route, array $params = []): array {
    if (!in_array($route, ['/v1/latest', '/v1/history', '/v1/weekly'], true)) {
        throw new InvalidArgumentException('Unsupported route');
    }
    $configPath = getenv('SIRAJ_CLOUD_CONFIG') ?: __DIR__.'/siraj_cloud_config.php';
    $config = require $configPath;
    if (strpos($config['base_url'], 'https://') !== 0 || strlen($config['read_key']) < 32) {
        throw new RuntimeException('Configure cloud URL and read key first');
    }
    $url = rtrim($config['base_url'], '/') . $route . '?' . http_build_query($params);
    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_HTTPHEADER => ['X-API-Key: '.$config['read_key'], 'Accept: application/json'],
        CURLOPT_CONNECTTIMEOUT => 3,
        CURLOPT_TIMEOUT => 8,
        CURLOPT_SSL_VERIFYPEER => true,
        CURLOPT_SSL_VERIFYHOST => 2,
        CURLOPT_FOLLOWLOCATION => false,
    ]);
    $body = curl_exec($ch);
    $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    if ($body === false || $status !== 200) {
        throw new RuntimeException('Cloud API unavailable, HTTP '.$status);
    }
    return json_decode($body, true, 512, JSON_THROW_ON_ERROR);
}
