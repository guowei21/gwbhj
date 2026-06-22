#!/usr/bin/env bash
# deploy.sh — GWBHJ 服务端一键部署到 Cloudflare Workers
set -e

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO/server"

echo "=== GWBHJ 服务端部署 ==="
echo ""

# 1. 安装依赖
echo "[1/5] 安装依赖..."
npm install

# 2. 检查是否已登录 Cloudflare
echo "[2/5] 检查 Cloudflare 登录状态..."
if ! npx wrangler whoami 2>/dev/null | grep -q "Account"; then
    echo "未登录 Cloudflare，正在打开浏览器登录..."
    npx wrangler login
fi

# 3. 创建 KV 命名空间
echo "[3/5] 创建 KV 命名空间..."
KV_OUTPUT=$(npx wrangler kv:namespace create GWBHJ_KV 2>&1 || true)
KV_ID=$(echo "$KV_OUTPUT" | grep -oP 'id = "\K[^"]+' || echo "")

if [ -z "$KV_ID" ]; then
    echo "KV 命名空间可能已存在，尝试列出..."
    KV_ID=$(npx wrangler kv:namespace list 2>&1 | grep -oP '"id": "\K[^"]+' | head -1 || echo "")
fi

if [ -z "$KV_ID" ]; then
    echo "错误: 无法获取 KV ID，请手动创建"
    echo "运行: npx wrangler kv:namespace create GWBHJ_KV"
    echo "然后将返回的 id 填入 wrangler.toml"
    exit 1
fi

echo "KV ID: $KV_ID"

# 更新 wrangler.toml
sed -i "s/STEP4_FILL_KV_ID/$KV_ID/" wrangler.toml || \
sed -i '' "s/STEP4_FILL_KV_ID/$KV_ID/" wrangler.toml

echo "已更新 wrangler.toml 中的 KV ID"

# 4. 设置密钥
echo "[4/5] 设置密钥 (交互式输入)..."
echo ""
echo "请输入 Ed25519 私钥 (HEX):"
echo "  如果没有密钥，先运行: npm run generate-keys"
read -p "ED25519_PRIVATE_KEY: " PRIV_KEY

if [ -z "$PRIV_KEY" ]; then
    echo "错误: 私钥不能为空"
    exit 1
fi

echo "$PRIV_KEY" | npx wrangler secret put ED25519_PRIVATE_KEY

read -p "ADMIN_KEY (管理后台密钥，自定义一个): " ADMIN_KEY_VAL
if [ -z "$ADMIN_KEY_VAL" ]; then
    ADMIN_KEY_VAL="gwbhj_admin_$(date +%s)"
    echo "使用默认管理员密钥: $ADMIN_KEY_VAL"
fi
echo "$ADMIN_KEY_VAL" | npx wrangler secret put ADMIN_KEY

# 5. 部署
echo "[5/5] 部署到 Cloudflare Workers..."
npx wrangler deploy

echo ""
echo "=== 部署完成 ==="
echo ""
echo "服务端 URL: https://gwbhj-server.<你的子域>.workers.dev"
echo "管理员密钥: $ADMIN_KEY_VAL"
echo ""
echo "接下来需要:"
echo "1. 将服务端 URL 填入 module/webroot/script.js 的 SERVER 变量"
echo "2. 将服务端公钥 Base64 填入 src/spoof_module.cpp 的 SERVER_PUBLIC_KEY_B64"
echo "3. 重新编译 .so 并打包模块"
echo ""
echo "测试 API:"
echo "  curl https://gwbhj-server.<子域>.workers.dev/health"
