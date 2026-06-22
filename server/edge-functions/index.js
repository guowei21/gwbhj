import { sign, getPublicKey } from '@noble/ed25519';

const PRIVATE_KEY_HEX = process.env.ED25519_PRIVATE_KEY || 'REPLACE_WITH_PRIVATE_KEY_HEX';
const ADMIN_KEY = process.env.ADMIN_KEY || 'REPLACE_WITH_ADMIN_KEY';
const SALT = 'GWBHJ_2026_SALT_v2';

const NONCE_CACHE = new Map();
const RATE_LIMIT = new Map();

function jsonResp(data, status = 200) {
    return new Response(JSON.stringify(data), {
        status,
        headers: { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' }
    });
}

function errorResp(message, status = 400) {
    return jsonResp({ error: message }, status);
}

async function sha256(message) {
    const encoder = new TextEncoder();
    const data = encoder.encode(message);
    const hashBuffer = await crypto.subtle.digest('SHA-256', data);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
}

async function generateDeviceHash(hardwareId) {
    return sha256(hardwareId + SALT);
}

function verifyTimestamp(ts) {
    const now = Math.floor(Date.now() / 1000);
    return Math.abs(now - ts) < 300;
}

function checkNonce(nonce) {
    if (NONCE_CACHE.has(nonce)) return false;
    NONCE_CACHE.set(nonce, Date.now());
    setTimeout(() => NONCE_CACHE.delete(nonce), 300000);
    return true;
}

function checkRateLimit(deviceHash) {
    const now = Date.now();
    const key = deviceHash;
    if (!RATE_LIMIT.has(key)) {
        RATE_LIMIT.set(key, []);
    }
    const timestamps = RATE_LIMIT.get(key).filter(t => now - t < 60000);
    if (timestamps.length >= 5) return false;
    timestamps.push(now);
    RATE_LIMIT.set(key, timestamps);
    return true;
}

function getLockDuration(clashCount) {
    if (clashCount <= 3) return 3600;
    if (clashCount <= 5) return 14400;
    if (clashCount <= 8) return 86400;
    if (clashCount === 9) return 604800;
    return -1;
}

function generateCode() {
    const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
    let code = 'GWBHJ';
    for (let i = 0; i < 4; i++) {
        code += '-';
        for (let j = 0; j < 4; j++) {
            code += chars[Math.floor(Math.random() * chars.length)];
        }
    }
    return code;
}

async function signLicense(licenseObj) {
    const fields = [
        licenseObj.code_id,
        licenseObj.device_hash,
        String(licenseObj.active),
        String(licenseObj.revoked),
        String(licenseObj.last_server_ok),
        String(licenseObj.check_interval),
        String(licenseObj.grace_seconds),
        String(licenseObj.lock_until || 0),
        String(licenseObj.clash_count)
    ];
    const message = fields.join('|');
    const messageBytes = new TextEncoder().encode(message);
    const sig = await sign(messageBytes, PRIVATE_KEY_HEX);
    return btoa(String.fromCharCode(...sig));
}

async function signClashInfo(clashObj) {
    const message = JSON.stringify({
        code: clashObj.code,
        is_kicked: clashObj.is_kicked,
        lock_status: clashObj.lock_status,
        lock_remaining_seconds: clashObj.lock_remaining_seconds
    });
    const messageBytes = new TextEncoder().encode(message);
    const sig = await sign(messageBytes, PRIVATE_KEY_HEX);
    return btoa(String.fromCharCode(...sig));
}

function readKV(kv, key) {
    return kv ? kv.get(key, { type: 'json' }) : null;
}

function writeKV(kv, key, value) {
    return kv ? kv.put(key, JSON.stringify(value)) : null;
}

export default {
    async fetch(request, env) {
        const url = new URL(request.url);
        const path = url.pathname;
        const method = request.method;

        if (method === 'OPTIONS') {
            return new Response(null, {
                headers: {
                    'Access-Control-Allow-Origin': '*',
                    'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
                    'Access-Control-Allow-Headers': 'Content-Type'
                }
            });
        }

        const KV = env.GWBHJ_KV || null;

        try {
            if (path === '/api/bind' && method === 'POST') {
                return await handleBind(request, KV);
            }
            if (path === '/api/verify' && method === 'POST') {
                return await handleVerify(request, KV);
            }
            if (path === '/api/clash-info' && method === 'POST') {
                return await handleClashInfo(request, KV);
            }
            if (path === '/admin/generate' && method === 'POST') {
                return await handleAdminGenerate(request, KV);
            }
            if (path === '/admin/delete' && method === 'POST') {
                return await handleAdminDelete(request, KV);
            }
            if (path === '/admin/list' && method === 'GET') {
                return await handleAdminList(request, KV);
            }
            if (path === '/health') {
                return jsonResp({ status: 'ok', time: Math.floor(Date.now() / 1000) });
            }
            return errorResp('Not found', 404);
        } catch (e) {
            console.error(e);
            return errorResp('Internal server error', 500);
        }
    }
};

async function handleBind(request, KV) {
    const body = await request.json();
    const { code, device_hash, device_info, nonce, ts } = body;

    if (!code || !device_hash) return errorResp('Missing required fields');
    if (ts && !verifyTimestamp(ts)) return errorResp('Invalid timestamp', 403);
    if (nonce && !checkNonce(nonce)) return errorResp('Replay detected', 403);
    if (!checkRateLimit(device_hash)) return errorResp('Rate limited', 429);

    const codeData = await readKV(KV, `code:${code}`);
    if (!codeData) return errorResp('Invalid code', 400);

    if (codeData.status === 'permanent_locked') return errorResp('Code permanently locked', 429);

    if (codeData.status === 'locked') {
        if (codeData.lock_until && Date.now() / 1000 < codeData.lock_until) {
            return errorResp('Code is locked', 409);
        }
        codeData.status = 'active';
        codeData.lock_until = null;
    }

    const now = Math.floor(Date.now() / 1000);
    const isDifferentDevice = codeData.active_device_hash && codeData.active_device_hash !== device_hash;

    if (isDifferentDevice) {
        codeData.clash_count = (codeData.clash_count || 0) + 1;
        const lockDuration = getLockDuration(codeData.clash_count);

        if (lockDuration === -1) {
            codeData.status = 'permanent_locked';
            await writeKV(KV, `code:${code}`, codeData);
            return errorResp('Code permanently locked due to excessive clashes', 429);
        }

        codeData.clash_log = codeData.clash_log || [];
        codeData.clash_log.push({
            time: new Date().toISOString(),
            device_hash_prefix: device_hash.substring(0, 16),
            device_info: device_info || {},
            action: 'kicked previous device'
        });

        const lockDurationPrev = getLockDuration(codeData.clash_count - 1);
        if (lockDurationPrev && lockDurationPrev > 0) {
            codeData.lock_until = now + lockDuration;
            codeData.status = 'locked';
        }
    }

    codeData.active_device_hash = device_hash;
    codeData.active_device_info = device_info || {};
    codeData.active_time = new Date().toISOString();
    codeData.status = 'active';

    await writeKV(KV, `code:${code}`, codeData);

    const license = {
        code_id: code,
        device_hash: device_hash,
        active: true,
        revoked: false,
        last_server_ok: now,
        check_interval: 600,
        grace_seconds: 86400,
        lock_until: null,
        clash_count: codeData.clash_count || 0,
        signature: ''
    };

    license.signature = await signLicense(license);

    return jsonResp(license);
}

async function handleVerify(request, KV) {
    const body = await request.json();
    const { code, device_hash, nonce, ts } = body;

    if (!code || !device_hash) return errorResp('Missing required fields');
    if (ts && !verifyTimestamp(ts)) return errorResp('Invalid timestamp', 403);
    if (nonce && !checkNonce(nonce)) return errorResp('Replay detected', 403);
    if (!checkRateLimit(device_hash)) return errorResp('Rate limited', 429);

    const codeData = await readKV(KV, `code:${code}`);
    if (!codeData) return errorResp('Invalid code', 400);

    if (codeData.status === 'permanent_locked') {
        return errorResp('Code permanently locked', 429);
    }

    const now = Math.floor(Date.now() / 1000);

    if (codeData.active_device_hash && codeData.active_device_hash !== device_hash) {
        codeData.clash_count = (codeData.clash_count || 0) + 1;
        const lockDuration = getLockDuration(codeData.clash_count);

        if (lockDuration === -1) {
            codeData.status = 'permanent_locked';
            await writeKV(KV, `code:${code}`, codeData);
            return errorResp('Permanently locked', 429);
        }

        codeData.clash_log = codeData.clash_log || [];
        codeData.clash_log.push({
            time: new Date().toISOString(),
            device_hash_prefix: device_hash.substring(0, 16),
            action: 'clash detected on verify'
        });

        if (lockDuration > 0) {
            codeData.lock_until = now + lockDuration;
            codeData.status = 'locked';
        }

        await writeKV(KV, `code:${code}`, codeData);

        return jsonResp({
            error: 'Device clash detected',
            is_kicked: true,
            active_device_hash_prefix: codeData.active_device_hash.substring(0, 16),
            active_device_info: codeData.active_device_info,
            clash_count: codeData.clash_count
        }, 409);
    }

    const license = {
        code_id: code,
        device_hash: device_hash,
        active: codeData.status === 'active',
        revoked: codeData.status === 'revoked',
        last_server_ok: now,
        check_interval: 600,
        grace_seconds: 86400,
        lock_until: codeData.lock_until || null,
        clash_count: codeData.clash_count || 0,
        signature: ''
    };

    license.signature = await signLicense(license);

    return jsonResp(license);
}

async function handleClashInfo(request, KV) {
    const body = await request.json();
    const { code, device_hash } = body;

    if (!code || !device_hash) return errorResp('Missing required fields');

    const codeData = await readKV(KV, `code:${code}`);
    if (!codeData) return errorResp('Code not found', 404);

    const isKicked = codeData.active_device_hash && codeData.active_device_hash !== device_hash;
    const now = Math.floor(Date.now() / 1000);
    let lockRemaining = 0;
    if (codeData.lock_until && codeData.lock_until > now) {
        lockRemaining = codeData.lock_until - now;
    }

    const clashInfo = {
        code: code,
        current_device_hash: device_hash,
        is_kicked: isKicked,
        kick_details: codeData.clash_log || [],
        lock_status: codeData.status === 'permanent_locked' ? 'permanent' : (lockRemaining > 0 ? 'locked' : 'none'),
        lock_remaining_seconds: lockRemaining,
        next_lock_level: codeData.clash_count < 10 ? 'escalating' : 'permanent',
        signature: ''
    };

    clashInfo.signature = await signClashInfo(clashInfo);

    return jsonResp(clashInfo);
}

async function handleAdminGenerate(request, KV) {
    const body = await request.json();
    const { count, prefix, admin_key } = body;

    if (admin_key !== ADMIN_KEY) return errorResp('Unauthorized', 403);
    if (!count || count < 1 || count > 100) return errorResp('Count must be 1-100');

    const codes = [];
    for (let i = 0; i < count; i++) {
        const code = prefix ? `${prefix}-${generateCode().substring(6)}` : generateCode();
        const codeData = {
            code: code,
            status: 'unused',
            active_device_hash: null,
            active_device_info: null,
            active_time: null,
            clash_count: 0,
            lock_until: null,
            clash_log: [],
            created_at: new Date().toISOString()
        };
        await writeKV(KV, `code:${code}`, codeData);
        codes.push(code);
    }

    return jsonResp({ codes: codes, count: codes.length });
}

async function handleAdminDelete(request, KV) {
    const body = await request.json();
    const { codes, admin_key } = body;

    if (admin_key !== ADMIN_KEY) return errorResp('Unauthorized', 403);
    if (!codes || !Array.isArray(codes)) return errorResp('Codes must be an array');

    const deleted = [];
    for (const code of codes) {
        const key = `code:${code}`;
        const exists = await readKV(KV, key);
        if (exists) {
            await KV.delete(key);
            deleted.push(code);
        }
    }

    return jsonResp({ deleted: deleted });
}

async function handleAdminList(request, KV) {
    const url = new URL(request.url);
    const admin_key = url.searchParams.get('admin_key');

    if (admin_key !== ADMIN_KEY) return errorResp('Unauthorized', 403);

    if (!KV) return errorResp('KV not available', 500);

    const list = await KV.list({ prefix: 'code:' });
    const codes = [];

    for (const key of list.keys) {
        const data = await KV.get(key.name, { type: 'json' });
        if (data) {
            codes.push({
                code: data.code,
                status: data.status,
                active_device_info: data.active_device_info,
                active_time: data.active_time,
                clash_count: data.clash_count || 0,
                lock_until: data.lock_until,
                created_at: data.created_at
            });
        }
    }

    return jsonResp({ codes: codes, total: codes.length });
}
