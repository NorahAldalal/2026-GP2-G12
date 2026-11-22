// Fetch only the currently authorized page. Keep the fault-report modal untouched.
(() => {
  let busy = false;
  async function refresh() {
    if (busy || document.hidden || document.activeElement?.matches('input, textarea')) return;
    busy = true;
    try {
      const response = await fetch(location.href, {credentials: 'same-origin', cache: 'no-store'});
      if (!response.ok || response.redirected) throw new Error('Page unavailable');
      const doc = new DOMParser().parseFromString(await response.text(), 'text/html');
      const next = doc.querySelector('#siraj-live-status');
      const current = document.querySelector('#siraj-live-status');
      if (!next || !current) throw new Error('Session or page unavailable');
      current.replaceWith(next);
      document.querySelector('#siraj-refresh-warning')?.remove();
    } catch (_) {
      let warning = document.querySelector('#siraj-refresh-warning');
      if (!warning) {
        warning = document.createElement('p'); warning.id = 'siraj-refresh-warning';
        warning.setAttribute('role', 'status');
        document.querySelector('#siraj-live-status')?.before(warning);
      }
      warning.textContent = 'Live refresh unavailable. Displayed values may be outdated.';
    } finally { busy = false; }
  }
  // Replace the existing button to remove any old reload listener from main.js.
  const old = document.querySelector('#refresh-status');
  if (old) { const button = old.cloneNode(true); old.replaceWith(button); button.addEventListener('click', refresh); }
  setInterval(refresh, 10000);
})();
