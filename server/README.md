# GWBHJ 服务端部署指南

## EdgeOne Edge Functions 部署

### 1. 生成 Ed25519 密钥对

```js
const { sign, getPublicKey, utils } = require('@noble/ed25519');

async function generateKeyPair() {
    const privateKey = utils.randomPrivateKey();
    const privateKeyHex = Buffer.from(privateKey).toString('hex');
    const publicKey = await getPublicKey(privateKeyHex);
    const publicKeyHex = Buffer.from(publicKey).toString('hex');
    const publicKeyBase64 = Buffer.from(publicKey).toString('base64');
    console.log('Private Key (HEX):', privateKeyHex);
    console.log('Public Key (HEX):', publicKeyHex);
    console.log('Public Key (Base64):', publicKeyBase64);
}

generateKeyPair();
```

### 2. 配置

1. 将生成的公钥 Base64 填入 `src/spoof_module.cpp` 中的 `SERVER_PUBLIC_KEY_B64`
2. 将私钥 HEX 填入服务端环境变量 `ED25519_PRIVATE_KEY`
3. 设置管理员密钥 `ADMIN_KEY`

### 3. 部署到 EdgeOne

```bash
cd server
npm install
npx wrangler kv:namespace create GWBHJ_KV
# 将返回的 KV namespace ID 填入 wrangler.toml

npx wrangler deploy
```

### 4. API 测试

```bash
# 生成卡密
curl -X POST https://your-domain/api/admin/generate \
  -H "Content-Type: application/json" \
  -d '{"count": 5, "admin_key": "YOUR_ADMIN_KEY"}'

# 绑定卡密
curl -X POST https://your-domain/api/bind \
  -H "Content-Type: application/json" \
  -d '{"code": "GWBHJ-XXXX-XXXX-XXXX", "device_hash": "sha256...", "device_info": {"brand": "Xiaomi", "model": "14"}}'

# 验证
curl -X POST https://your-domain/api/verify \
  -H "Content-Type: application/json" \
  -d '{"code": "GWBHJ-XXXX-XXXX-XXXX", "device_hash": "sha256...", "nonce": "uuid", "ts": 1719012345}'

# 查看互抵
curl -X POST https://your-domain/api/clash-info \
  -H "Content-Type: application/json" \
  -d '{"code": "GWBHJ-XXXX-XXXX-XXXX", "device_hash": "sha256..."}'
```
