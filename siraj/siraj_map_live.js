// Only the authorized page returns data; no cloud credentials enter the browser.
function startSirajLiveMap(map, initial) {
  let lamps = initial, busy = false;
  const markers = new Map();
  const color = s => ({on:'#4CAF7D',off:'#E05C5C',fault:'#F0B429'}[s] || '#9299A5');
  const esc = x => String(x ?? '').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const num = x => typeof x === 'number' && Number.isFinite(x) ? x.toFixed(1) : 'Unavailable';
  function popup(l) {
    let html = `<strong>Lamp #${esc(l.id)}</strong><br>${esc(l.area)}<br><span style="color:${color(l.status)}">${esc(l.label)}</span><br>${l.cloud ? 'Ambient lux' : 'Lux (database)'}: ${num(l.lux)}`;
    if (l.cloud) html += `<br>Reported PWM: ${num(l.pwm)}%<br>Model command: ${num(l.command)}%<br>Battery: ${num(l.battery)} V<br>Road lux: ${num(l.road_lux)}<br>Last received: ${esc(l.received || 'Unavailable')}`;
    else html += '<br>Stored database value; live status not verified';
    return `<div style="overflow-wrap:anywhere;line-height:1.6">${html}</div>`;
  }
  function render() {
    const ids = new Set();
    for (const l of lamps) {
      ids.add(l.id);
      if (!Number.isFinite(l.lat) || !Number.isFinite(l.lng) || Math.abs(l.lat)>90 || Math.abs(l.lng)>180) continue;
      const icon=L.divIcon({className:'',html:`<div style="width:14px;height:14px;border-radius:50%;background:${color(l.status)};border:2px solid white"></div>`,iconSize:[14,14],iconAnchor:[7,7]});
      let marker=markers.get(l.id);
      if (!marker) { marker=L.marker([l.lat,l.lng],{icon}).bindPopup(popup(l),{maxWidth:280}); markers.set(l.id,marker); }
      else { marker.setLatLng([l.lat,l.lng]); marker.setIcon(icon); marker.setPopupContent(popup(l)); }
      const filter=l.status==='on'?'show-on':l.status==='off'?'show-off':l.status==='fault'?'show-fault':'show-unknown';
      if(document.getElementById(filter)?.checked) { if(!map.hasLayer(marker)) marker.addTo(map); }
      else map.removeLayer(marker);
    }
    for(const [id,marker] of markers) if(!ids.has(id)){map.removeLayer(marker);markers.delete(id);}
  }
  ['show-on','show-off','show-fault','show-unknown'].forEach(id=>document.getElementById(id)?.addEventListener('change',render));
  async function refresh() {
    if(busy || document.hidden) return;
    busy=true;
    const notice=document.getElementById('siraj-map-notice');
    try {
      const url=new URL(location.href);url.searchParams.set('siraj_map_data','1');
      const response=await fetch(url,{credentials:'same-origin',cache:'no-store'});
      if(!response.ok || response.redirected) throw new Error('Unavailable');
      const data=await response.json();
      if(!Array.isArray(data.lamps)) throw new Error('Invalid data');
      lamps=data.lamps;render();
      notice.textContent='Map refreshed: '+new Date().toLocaleTimeString();
    } catch (_) {
      lamps=lamps.map(l=>l.cloud?{...l,status:'unknown',label:'Refresh unavailable - last known readings'}:l);
      render();notice.textContent='Live refresh unavailable. Last known readings may be outdated.';
    } finally {busy=false;}
  }
  render();setInterval(refresh,10000);
  document.getElementById('siraj-map-refresh')?.addEventListener('click',refresh);
}
