# GWBHJ 服务端部署指南

## 架构

```
客户端(.so) <--> 服务端(Cloudflare Workers + KV) <--> WebUI(script.js)
                     |
                  Ed25519 签名验证
```

## 方式一：一键部署（推荐）

```bash
cd server
bash deploy.sh
```

脚本会自动：安装依赖 → 登录 Cloudflare → 创建 KV → 设置密钥 → 部署。

## 方式二：手动部署

### 1. 前置条件

- Node.js 18+
- Cloudflare 账号（免费即可）

### 2. 安装依赖

```bash
cd server
npm install
```

### 3. 生成 Ed25519 密钥对

```bash
npm run generate-keys
```

输出示例：
```
Private Key (HEX): a1b2c3d4e5f6...
Public Key (HEX):  1234abcd...
Public Key (Base64): ASNFQ29...
```

**务必保存私钥，丢失后所有授权失效。**

### 4. 登录 Cloudflare

```bash
npx wrangler login
```

浏览器会打开授权页面，点击允许。

### 5. 创建 KV 命名空间

```bash
npx wrangler kv:namespace create GWBHJ_KV
```

输出类似：
```
⛅️ Created namespace "GWBHJ_KV" with id "xxxxxxxxxxxx"
```

将返回的 `id` 填入 `wrangler.toml`：

```toml
[[kv_namespaces]]
binding = "GWBHJ_KV"
id = "xxxxxxxxxxxx"    # <-- 填这里
```

### 6. 设置密钥（安全方式，不明文写在配置文件）

```bash
# 设置私钥
echo "你的私钥HEX" | npx wrangler secret put ED25519_PRIVATE_KEY

# 设置管理员密钥（自定义）
echo "你的管理员密钥" | npx wrangler secret put ADMIN_KEY
```

### 7. 部署

```bash
npx wrangler deploy
```

部署成功后输出：
```
Published gwbhj-server
  https://gwbhj-server.你的子域.workers.dev
```

### 8. 测试

```bash
# 健康检查
curl https://gwbhj-server.你的子域.workers.dev/health

# 生成 5 个卡密
curl -X POST https://gwbhj-server.你的子域.workers.dev/admin/generate \
  -H "Content-Type: application/json" \
  -d '{"count":5,"admin_key":"你的管理员密钥"}'

# 查看卡密列表
curl "https://gwbhj-server.你的子域.workers.dev/admin/list?admin_key=你的管理员密钥"
```

## 部署后配置

部署完成后，需要将服务端信息填入客户端代码：

### 1. 填入 WebUI

编辑 `module/webroot/script.js` 第 2 行：

```js
const SERVER = 'https://gwbhj-server.你的子域.workers.dev';
```

### 2. 填入 .so

编辑 `src/spoof_module.cpp` 中的公钥：

```cpp
static const char* SERVER_PUBLIC_KEY_B64 =
    "你生成的公钥Base64";
```

### 3. 重新编译 .so 并打包模块

```bash
./build.sh
```

## API 参考

| 接口 | 方法 | 说明 |
|------|------|------|
| `/health` | GET | 健康检查 |
| `/api/bind` | POST | 卡密绑定 |
| `/api/verify` | POST | 定期验证 |
| `/api/clash-info` | POST | 互抵详情 |
| `/admin/generate` | POST | 生成卡密 |
| `/admin/delete` | POST | 删除卡密 |
| `/admin/list` | GET | 卡密列表 |

### 卡密绑定示例

```bash
curl -X POST https://你的域名/api/bind \
  -H "Content-Type: application/json" \
  -d '{
    "code": "GWBHJ-XXXX-XXXX-XXXX",
    "device_hash": "sha256hash...",
    "device_info": {"brand":"Xiaomi","model":"14","serial":"ABC123"},
    "nonce": "random-uuid",
    "ts": 1719012345
  }'
```

成功返回 license.json（Ed25519 签名）：

```json
{
  "code_id": "GWBHJ-XXXX-XXXX-XXXX",
  "device_hash": "sha256...",
  "active": true,
  "revoked": false,
  "last_server_ok": 1719012345,
  "check_interval": 600,
  "grace_seconds": 86400,
  "lock_until": null,
  "clash_count": 0,
  "signature": "base64_signature"
}
```

### 生成卡密示例

```bash
curl -X POST https://你的域名/admin/generate \
  -H "Content-Type: application/json" \
  -d '{"count":5,"admin_key":"你的管理员密钥"}'
```

返回：

```json
{
  "codes": ["GWBHJ-A2B3-C4D5-E6F7", "GWBHJ-G8H9-J0K1-L2M3", ...],
  "count": 5
}
```

## 费用

Cloudflare Workers 免费套餐：

| 项目 | 免费额度 |
|------|---------|
| 请求数 | 10万次/天 |
| KV 读取 | 10万次/天 |
| KV 写入 | 1000次/天 |
| CPU 时间 | 10ms/请求 |

对于个人使用完全足够。如果用户量增大，$5/月 的付费套餐可覆盖 1000万请求/月。

## 数据备份

KV 数据可导出：

```bash
npx wrangler kv:key list --namespace-id=你的KV_ID
npx wrangler kv:key get "code:GWBHJ-XXXX-XXXX-XXXX" --namespace-id=你的KV_ID
```

## 迁移到独立服务器

用户量增大后可迁移：

1. API 路径保持不变
2. 只需修改 `script.js` 和 `.so` 中的 `SERVER_URL`
3. Ed25519 密钥对继续沿用
4. KV 数据导出为 JSON 后导入数据库
