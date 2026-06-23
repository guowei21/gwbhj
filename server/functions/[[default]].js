import { sign } from '@noble/ed25519';

const SALT = 'GWBHJ_2026_SALT_v2';
const NONCE_CACHE = new Map();
const RATE_LIMIT = new Map();

function getEnvValue(env, name) {
    return env?.[name] || globalThis[name] || '';
}

function getKV(env) {
    return env?.GWBHJ_KV || globalThis.GWBHJ_KV || globalThis.gwbhj_kv || null;
}

function jsonResp(data, status = 200) {
    return new Response(JSON.stringify(data), {
        status,
        headers: {
            'Content-Type': 'application/json; charset=utf-8',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        }
    });
}

function errorResp(message, status = 400) {
    return jsonResp({ error: message }, status);
}

function verifyTimestamp(ts) {
    if (!ts) return true;
    const now = Math.floor(Date.now() / 1000);
    return Math.abs(now - Number(ts)) < 300;
}

function checkNonce(nonce) {
    if (!nonce) return true;
    if (NONCE_CACHE.has(nonce)) return false;
    NONCE_CACHE.set(nonce, Date.now());
    setTimeout(() => NONCE_CACHE.delete(nonce), 300000);
    return true;
}

function checkRateLimit(deviceHash) {
    const now = Date.now();
    const timestamps = (RATE_LIMIT.get(deviceHash) || []).filter(t => now - t < 60000);
    if (timestamps.length >= 5) return false;
    timestamps.push(now);
    RATE_LIMIT.set(deviceHash, timestamps);
    return true;
}

function getLockDuration(clashCount) {
    if (clashCount <= 3) return 3600;
    if (clashCount <= 5) return 14400;
    if (clashCount <= 8) return 86400;
    if (clashCount === 9) return 604800;
    return -1;
}

function generateCode(prefix = 'GWBHJ') {
    const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
    let code = prefix;
    for (let i = 0; i < 4; i++) {
        code += '-';
        for (let j = 0; j < 4; j++) {
            code += chars[Math.floor(Math.random() * chars.length)];
        }
    }
    return code;
}

function toBase64(bytes) {
    let binary = '';
    for (const byte of bytes) binary += String.fromCharCode(byte);
    return btoa(binary);
}

async function signMessage(privateKeyHex, message) {
    if (!privateKeyHex || privateKeyHex.startsWith('REPLACE_')) {
        throw new Error('ED25519_PRIVATE_KEY is not configured');
    }
    const bytes = new TextEncoder().encode(message);
    const sig = await sign(bytes, privateKeyHex);
    return toBase64(sig);
}

async function signLicense(env, licenseObj) {
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
    return signMessage(getEnvValue(env, 'ED25519_PRIVATE_KEY'), fields.join('|'));
}

async function signClashInfo(env, clashObj) {
    const message = JSON.stringify({
        code: clashObj.code,
        is_kicked: clashObj.is_kicked,
        lock_status: clashObj.lock_status,
        lock_remaining_seconds: clashObj.lock_remaining_seconds
    });
    return signMessage(getEnvValue(env, 'ED25519_PRIVATE_KEY'), message);
}

async function readKV(kv, key) {
    if (!kv) throw new Error('GWBHJ_KV is not bound');
    return kv.get(key, { type: 'json' });
}

async function writeKV(kv, key, value) {
    if (!kv) throw new Error('GWBHJ_KV is not bound');
    return kv.put(key, JSON.stringify(value));
}

async function handleBind(request, env, kv) {
    const body = await request.json();
    const { code, device_hash, device_info, nonce, ts } = body;

    if (!code || !device_hash) return errorResp('Missing required fields');
    if (!verifyTimestamp(ts)) return errorResp('Invalid timestamp', 403);
    if (!checkNonce(nonce)) return errorResp('Replay detected', 403);
    if (!checkRateLimit(device_hash)) return errorResp('Rate limited', 429);

    const codeData = await readKV(kv, `code:${code}`);
    if (!codeData) return errorResp('Invalid code', 400);
    if (codeData.status === 'permanent_locked') return errorResp('Code permanently locked', 429);

    const now = Math.floor(Date.now() / 1000);
    if (codeData.status === 'locked' && codeData.lock_until && now < codeData.lock_until) {
        return errorResp('Code is locked', 409);
    }

    const isDifferentDevice = codeData.active_device_hash && codeData.active_device_hash !== device_hash;
    if (isDifferentDevice) {
        codeData.clash_count = (codeData.clash_count || 0) + 1;
        const lockDuration = getLockDuration(codeData.clash_count);
        if (lockDuration === -1) {
            codeData.status = 'permanent_locked';
            await writeKV(kv, `code:${code}`, codeData);
            return errorResp('Code permanently locked due to excessive clashes', 429);
        }
        codeData.lock_until = now + lockDuration;
        codeData.status = 'locked';
        codeData.clash_log = codeData.clash_log || [];
        codeData.clash_log.push({
            time: new Date().toISOString(),
            device_hash_prefix: device_hash.substring(0, 16),
            device_info: device_info || {},
            action: 'kicked previous device'
        });
    }

    codeData.active_device_hash = device_hash;
    codeData.active_device_info = device_info || {};
    codeData.active_time = new Date().toISOString();
    codeData.status = 'active';
    await writeKV(kv, `code:${code}`, codeData);

    const license = {
        code_id: code,
        device_hash,
        active: true,
        revoked: false,
        last_server_ok: now,
        check_interval: 600,
        grace_seconds: 86400,
        lock_until: null,
        clash_count: codeData.clash_count || 0,
        signature: ''
    };
    license.signature = await signLicense(env, license);
    return jsonResp(license);
}

async function handleVerify(request, env, kv) {
    const body = await request.json();
    const { code, device_hash, nonce, ts } = body;

    if (!code || !device_hash) return errorResp('Missing required fields');
    if (!verifyTimestamp(ts)) return errorResp('Invalid timestamp', 403);
    if (!checkNonce(nonce)) return errorResp('Replay detected', 403);
    if (!checkRateLimit(device_hash)) return errorResp('Rate limited', 429);

    const codeData = await readKV(kv, `code:${code}`);
    if (!codeData) return errorResp('Invalid code', 400);
    if (codeData.status === 'permanent_locked') return errorResp('Code permanently locked', 429);

    const now = Math.floor(Date.now() / 1000);
    if (codeData.active_device_hash && codeData.active_device_hash !== device_hash) {
        codeData.clash_count = (codeData.clash_count || 0) + 1;
        const lockDuration = getLockDuration(codeData.clash_count);
        if (lockDuration === -1) {
            codeData.status = 'permanent_locked';
            await writeKV(kv, `code:${code}`, codeData);
            return errorResp('Permanently locked', 429);
        }
        codeData.lock_until = now + lockDuration;
        codeData.status = 'locked';
        codeData.clash_log = codeData.clash_log || [];
        codeData.clash_log.push({
            time: new Date().toISOString(),
            device_hash_prefix: device_hash.substring(0, 16),
            action: 'clash detected on verify'
        });
        await writeKV(kv, `code:${code}`, codeData);
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
        device_hash,
        active: codeData.status === 'active',
        revoked: codeData.status === 'revoked',
        last_server_ok: now,
        check_interval: 600,
        grace_seconds: 86400,
        lock_until: codeData.lock_until || null,
        clash_count: codeData.clash_count || 0,
        signature: ''
    };
    license.signature = await signLicense(env, license);
    return jsonResp(license);
}

async function handleClashInfo(request, env, kv) {
    const body = await request.json();
    const { code, device_hash } = body;
    if (!code || !device_hash) return errorResp('Missing required fields');

    const codeData = await readKV(kv, `code:${code}`);
    if (!codeData) return errorResp('Code not found', 404);

    const now = Math.floor(Date.now() / 1000);
    const isKicked = codeData.active_device_hash && codeData.active_device_hash !== device_hash;
    const lockRemaining = codeData.lock_until && codeData.lock_until > now ? codeData.lock_until - now : 0;

    const clashInfo = {
        code,
        current_device_hash: device_hash,
        is_kicked: isKicked,
        kick_details: codeData.clash_log || [],
        lock_status: codeData.status === 'permanent_locked' ? 'permanent' : (lockRemaining > 0 ? 'locked' : 'none'),
        lock_remaining_seconds: lockRemaining,
        next_lock_level: (codeData.clash_count || 0) < 10 ? 'escalating' : 'permanent',
        signature: ''
    };
    clashInfo.signature = await signClashInfo(env, clashInfo);
    return jsonResp(clashInfo);
}

async function handleAdminGenerate(request, env, kv) {
    const body = await request.json();
    const adminKey = getEnvValue(env, 'ADMIN_KEY');
    const { count, prefix, admin_key } = body;

    if (!adminKey || admin_key !== adminKey) return errorResp('Unauthorized', 403);
    if (!count || count < 1 || count > 100) return errorResp('Count must be 1-100');

    const codes = [];
    for (let i = 0; i < count; i++) {
        const code = generateCode(prefix || 'GWBHJ');
        await writeKV(kv, `code:${code}`, {
            code,
            status: 'unused',
            active_device_hash: null,
            active_device_info: null,
            active_time: null,
            clash_count: 0,
            lock_until: null,
            clash_log: [],
            created_at: new Date().toISOString()
        });
        codes.push(code);
    }
    return jsonResp({ codes, count: codes.length });
}

async function handleAdminDelete(request, env, kv) {
    const body = await request.json();
    const adminKey = getEnvValue(env, 'ADMIN_KEY');
    const { codes, admin_key } = body;

    if (!adminKey || admin_key !== adminKey) return errorResp('Unauthorized', 403);
    if (!codes || !Array.isArray(codes)) return errorResp('Codes must be an array');

    const deleted = [];
    for (const code of codes) {
        const key = `code:${code}`;
        const exists = await readKV(kv, key);
        if (exists) {
            await kv.delete(key);
            deleted.push(code);
        }
    }
    return jsonResp({ deleted });
}

async function handleAdminList(request, env, kv) {
    const url = new URL(request.url);
    const adminKey = getEnvValue(env, 'ADMIN_KEY');
    const admin_key = url.searchParams.get('admin_key');

    if (!adminKey || admin_key !== adminKey) return errorResp('Unauthorized', 403);

    const list = await kv.list({ prefix: 'code:' });
    const codes = [];
    for (const item of list.keys || []) {
        const key = item.key || item.name;
        const data = await kv.get(key, { type: 'json' });
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
    return jsonResp({ codes, total: codes.length });
}

export async function onRequest(context) {
    const { request, env } = context;
    const url = new URL(request.url);
    const path = url.pathname;
    const method = request.method;

    if (method === 'OPTIONS') return jsonResp({}, 204);

    try {
        const kv = getKV(env);

        if (path === '/health') {
            return jsonResp({ status: 'ok', platform: 'edgeone', time: Math.floor(Date.now() / 1000) });
        }
        if (path === '/api/bind' && method === 'POST') return handleBind(request, env, kv);
        if (path === '/api/verify' && method === 'POST') return handleVerify(request, env, kv);
        if (path === '/api/clash-info' && method === 'POST') return handleClashInfo(request, env, kv);
        if (path === '/admin/generate' && method === 'POST') return handleAdminGenerate(request, env, kv);
        if (path === '/admin/delete' && method === 'POST') return handleAdminDelete(request, env, kv);
        if (path === '/admin/list' && method === 'GET') return handleAdminList(request, env, kv);

        return errorResp('Not found', 404);
    } catch (e) {
        console.error(e);
        return errorResp(e.message || 'Internal server error', 500);
    }
}
