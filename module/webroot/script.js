const MOD = '/data/adb/modules/gwbhj_jailbreak';
const SERVER = 'https://REPLACE_WITH_YOUR_DOMAIN';
const SALT = 'GWBHJ_2026_SALT_v2';

function exec(cmd) {
  return new Promise((resolve, reject) => {
    if (window.ksu) {
      window.ksu.exec(cmd, {}, (code, out, err) => {
        if (code === 0) resolve(out);
        else reject(err || 'exit ' + code);
      });
    } else if (window.kernelSU) {
      window.kernelSU.exec(cmd, (code, out) => {
        if (code === 0) resolve(out);
        else reject('exit ' + code);
      });
    } else {
      reject('非 KernelSU 环境');
    }
  });
}

async function readFile(path) {
  const out = await exec('cat "' + path + '" 2>/dev/null');
  return typeof out === 'string' ? out.trim() : out.join('\n').trim();
}

async function writeFile(path, content) {
  const tmp = '/data/local/tmp/.gwbhj_web';
  await exec('cat > "' + tmp + '" << \'GWBHEOF\'\n' + content + '\nGWBHEOF');
  await exec('cp "' + tmp + '" "' + path + '"');
  await exec('rm -f "' + tmp + '"');
  await exec('chmod 644 "' + path + '"');
  await exec('chcon u:object_r:system_file:s0 "' + path + '" 2>/dev/null');
}

async function deleteFile(path) {
  await exec('rm -f "' + path + '"');
}

async function fileExists(path) {
  try {
    await exec('test -f "' + path + '"');
    return true;
  } catch { return false; }
}

async function sha256(text) {
  const enc = new TextEncoder();
  const data = enc.encode(text);
  const hash = await crypto.subtle.digest('SHA-256', data);
  return Array.from(new Uint8Array(hash)).map(b => b.toString(16).padStart(2, '0')).join('');
}

async function getDeviceHash() {
  const cmdline = await readFile('/proc/cmdline');
  if (!cmdline) return '';
  const fields = [
    'androidboot.cpuid', 'androidboot.chipid', 'androidboot.emmcid',
    'oplusboot.serialno', 'androidboot.serialno'
  ];
  for (const f of fields) {
    const idx = cmdline.indexOf(f + '=');
    if (idx < 0) continue;
    let val = cmdline.substring(idx + f.length + 1);
    const sp = val.search(/[\s]/);
    if (sp >= 0) val = val.substring(0, sp);
    val = val.trim();
    if (val) return await sha256(val + SALT);
  }
  return '';
}

function show(id) {
  document.querySelectorAll('.page').forEach(p => p.classList.add('hidden'));
  document.getElementById('page-' + id).classList.remove('hidden');
}

function navigateTo(page) {
  if (page === 'whitelist') loadWhitelist();
  else if (page === 'freezelist') loadFreezelist();
  else if (page === 'clash') loadClash();
  else if (page === 'status') loadStatus();
  show(page);
}

function showMsg(id, text, type) {
  const el = document.getElementById(id);
  el.textContent = text;
  el.className = 'msg ' + type;
  setTimeout(() => { el.className = 'msg hidden'; }, 4000);
}

function showLoading() { document.getElementById('loading').classList.remove('hidden'); }
function hideLoading() { document.getElementById('loading').classList.add('hidden'); }

function formatTime(ts) {
  if (!ts || ts <= 0) return '-';
  const d = new Date(ts * 1000);
  return d.getFullYear() + '-' +
    String(d.getMonth() + 1).padStart(2, '0') + '-' +
    String(d.getDate()).padStart(2, '0') + ' ' +
    String(d.getHours()).padStart(2, '0') + ':' +
    String(d.getMinutes()).padStart(2, '0');
}

function formatDuration(sec) {
  if (sec <= 0) return '0秒';
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  if (h >= 24) return Math.floor(h / 24) + '天' + (h % 24) + '小时';
  if (h > 0) return h + '小时' + m + '分钟';
  return m + '分钟';
}

let cachedLicense = null;

async function loadStatus() {
  show('status');
  try {
    const rootSol = await exec('which ksud 2>/dev/null && echo KSU || (which magisk 2>/dev/null && echo Magisk || (which apd 2>/dev/null && echo APatch || echo Unknown))');
    document.getElementById('root-sol').textContent = rootSol.toString().trim().split('\n').pop();
  } catch { document.getElementById('root-sol').textContent = 'Unknown'; }

  document.getElementById('mod-status').textContent = await fileExists(MOD) ? '已安装' : '未安装';
  document.getElementById('serial-val').textContent = (await readFile(MOD + '/serial.txt')) || '未设置';

  const wl = await readFile(MOD + '/whitelist.txt');
  const wlPkgs = wl.split('\n').filter(l => l.trim() && !l.startsWith('#'));
  document.getElementById('wl-count').textContent = wlPkgs.length + ' 个包名';

  const hasLic = await fileExists(MOD + '/license.json');
  if (hasLic) {
    try {
      const licRaw = await readFile(MOD + '/license.json');
      cachedLicense = JSON.parse(licRaw);
      document.getElementById('auth-status').textContent = cachedLicense.active ? '已授权' : '未激活';
      document.getElementById('auth-status').className = cachedLicense.active ? 'status-ok' : 'status-err';
      document.getElementById('license-code').textContent = cachedLicense.code_id || '-';
      document.getElementById('last-verify').textContent = formatTime(cachedLicense.last_server_ok);
      const exp = cachedLicense.last_server_ok + (cachedLicense.grace_seconds || 86400);
      document.getElementById('expiry').textContent = formatTime(exp);
      document.getElementById('clash-count').textContent = String(cachedLicense.clash_count || 0);
    } catch {
      document.getElementById('auth-status').textContent = '解析失败';
      document.getElementById('auth-status').className = 'status-err';
    }
  } else {
    cachedLicense = null;
    document.getElementById('auth-status').textContent = '未绑定';
    document.getElementById('auth-status').className = 'status-warn';
    document.getElementById('license-code').textContent = '-';
    document.getElementById('last-verify').textContent = '-';
    document.getElementById('expiry').textContent = '-';
    document.getElementById('clash-count').textContent = '-';
  }
}

async function doBind() {
  const code = document.getElementById('code-input').value.trim();
  if (!code) { showMsg('auth-msg', '请输入卡密', 'error'); return; }

  const deviceHash = await getDeviceHash();
  if (!deviceHash) { showMsg('auth-msg', '无法获取设备ID', 'error'); return; }

  showLoading();
  try {
    const serial = await readFile(MOD + '/serial.txt');
    const body = {
      code: code,
      device_hash: deviceHash,
      device_info: { brand: 'Android', model: 'Device', serial: serial },
      nonce: crypto.randomUUID(),
      ts: Math.floor(Date.now() / 1000)
    };

    const resp = await fetch(SERVER + '/api/bind', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });

    const data = await resp.json();

    if (!resp.ok) {
      hideLoading();
      const msgs = { 400: '无效的卡密', 403: '设备不匹配', 409: '卡密已锁定', 429: '卡密被永久锁定' };
      showMsg('auth-msg', msgs[resp.status] || data.error || '绑定失败: ' + resp.status, 'error');
      return;
    }

    await writeFile(MOD + '/license.json', JSON.stringify(data));
    hideLoading();
    navigateTo('status');
  } catch (e) {
    hideLoading();
    showMsg('auth-msg', '网络错误: ' + e.message, 'error');
  }
}

async function doUnbind() {
  if (!confirm('确定要解绑卡密吗？解绑后伪装功能将关闭。')) return;
  await deleteFile(MOD + '/license.json');
  cachedLicense = null;
  navigateTo('status');
}

async function doAction(cmd) {
  showLoading();
  try {
    await exec('sh ' + MOD + '/action.sh ' + cmd);
  } catch {}
  hideLoading();
  if (cmd === 'reset-serial') navigateTo('status');
}

async function loadWhitelist() {
  const wl = await readFile(MOD + '/whitelist.txt');
  const pkgs = wl.split('\n').filter(l => l.trim() && !l.startsWith('#')).map(l => l.trim());
  const container = document.getElementById('wl-list');
  container.innerHTML = '';
  pkgs.forEach(pkg => {
    const div = document.createElement('div');
    div.className = 'pkg-item';
    div.innerHTML = '<span>' + esc(pkg) + '</span><button class="del-btn" onclick="removeWl(\'' + esc(pkg) + '\')">删除</button>';
    container.appendChild(div);
  });
}

async function addWhitelist() {
  const input = document.getElementById('wl-input');
  const pkg = input.value.trim();
  if (!pkg) return;
  const wl = await readFile(MOD + '/whitelist.txt');
  const pkgs = wl.split('\n').filter(l => l.trim() && !l.startsWith('#')).map(l => l.trim());
  if (pkgs.includes(pkg)) return;
  pkgs.push(pkg);
  await writeFile(MOD + '/whitelist.txt', pkgs.join('\n'));
  input.value = '';
  loadWhitelist();
}

async function removeWl(pkg) {
  const wl = await readFile(MOD + '/whitelist.txt');
  const pkgs = wl.split('\n').filter(l => l.trim() && !l.startsWith('#')).map(l => l.trim()).filter(p => p !== pkg);
  await writeFile(MOD + '/whitelist.txt', pkgs.join('\n'));
  loadWhitelist();
}

async function loadFreezelist() {
  const fl = await readFile(MOD + '/freeze_list.txt');
  const pkgs = fl.split('\n').filter(l => l.trim() && !l.startsWith('#')).map(l => l.trim());
  const container = document.getElementById('fl-list');
  container.innerHTML = '';
  pkgs.forEach(pkg => {
    const div = document.createElement('div');
    div.className = 'pkg-item';
    div.innerHTML = '<span>' + esc(pkg) + '</span><button class="del-btn" onclick="removeFl(\'' + esc(pkg) + '\')">删除</button>';
    container.appendChild(div);
  });
}

async function addFreezelist() {
  const input = document.getElementById('fl-input');
  const pkg = input.value.trim();
  if (!pkg) return;
  const fl = await readFile(MOD + '/freeze_list.txt');
  const pkgs = fl.split('\n').filter(l => l.trim() && !l.startsWith('#')).map(l => l.trim());
  if (pkgs.includes(pkg)) return;
  pkgs.push(pkg);
  await writeFile(MOD + '/freeze_list.txt', pkgs.join('\n'));
  input.value = '';
  loadFreezelist();
}

async function removeFl(pkg) {
  const fl = await readFile(MOD + '/freeze_list.txt');
  const pkgs = fl.split('\n').filter(l => l.trim() && !l.startsWith('#')).map(l => l.trim()).filter(p => p !== pkg);
  await writeFile(MOD + '/freeze_list.txt', pkgs.join('\n'));
  loadFreezelist();
}

async function loadClash() {
  const container = document.getElementById('clash-content');
  if (!cachedLicense) {
    container.innerHTML = '<p>未绑定卡密，无互抵信息</p>';
    return;
  }

  showLoading();
  try {
    const deviceHash = await getDeviceHash();
    const resp = await fetch(SERVER + '/api/clash-info', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code: cachedLicense.code_id, device_hash: deviceHash })
    });
    const data = await resp.json();
    hideLoading();

    let html = '';
    if (data.is_kicked) {
      html += '<div class="lock-banner">!! 卡密异地登录，请妥善管理卡密<br>您的设备已被其他设备踢出</div>';
    }

    const lockStatus = data.lock_status || 'none';
    if (lockStatus === 'permanent') {
      html += '<div class="lock-banner">卡密已被永久锁定</div>';
    } else if (lockStatus === 'locked') {
      html += '<div class="lock-banner">卡密已锁定<br>剩余时间: ' + formatDuration(data.lock_remaining_seconds || 0) + '</div>';
    }

    if (data.kick_details && data.kick_details.length > 0) {
      data.kick_details.forEach(d => {
        html += '<div class="clash-item">';
        html += '<div class="time">' + esc(d.time || '') + '</div>';
        if (d.device_model) html += '<div class="device">设备: ' + esc(d.device_model) + '</div>';
        if (d.device_serial) html += '<div class="device">序列号: ' + esc(d.device_serial) + '</div>';
        html += '<div class="action">' + esc(d.action || '') + '</div>';
        html += '</div>';
      });
    } else {
      html += '<p style="color:#9e9e9e">暂无互抵记录</p>';
    }

    container.innerHTML = html;
  } catch (e) {
    hideLoading();
    container.innerHTML = '<p style="color:#ef5350">查询失败: ' + esc(e.message) + '</p>';
  }
}

function esc(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

async function init() {
  try {
    const deviceHash = await getDeviceHash();
    document.getElementById('device-hash').textContent = deviceHash ? deviceHash.substring(0, 32) + '...' : '无法获取';
  } catch {
    document.getElementById('device-hash').textContent = '无法获取';
  }

  const hasLic = await fileExists(MOD + '/license.json');
  if (hasLic) {
    navigateTo('status');
  } else {
    show('auth');
  }
}

init();
