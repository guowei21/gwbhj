# GWBHJ 服务端部署指南（Tencent EdgeOne）

## 目录结构

```text
server/
├── functions/
│   └── [[default]].js      # EdgeOne Pages Functions 入口，匹配所有 API 路径
├── edgeone.json            # EdgeOne Pages 配置
├── package.json
└── generate_keys.js        # Ed25519 密钥生成脚本
```

## 1. 准备 EdgeOne

1. 登录 EdgeOne Makers: https://edgeone.ai/
2. 创建一个 Pages 项目，导入当前 GitHub 仓库。
3. 项目根目录建议设置为 `server`。
4. 构建命令可留空或使用 `npm install`。

## 2. 创建 KV 命名空间

1. 进入 EdgeOne 控制台。
2. 打开 `Storage -> KV`。
3. 启用 KV 后创建命名空间，例如 `gwbhj_kv`。
4. 回到 Pages 项目，进入 `KV Storage`。
5. 绑定该命名空间，变量名必须设置为：

```text
GWBHJ_KV
```

服务端函数会通过 `env.GWBHJ_KV` 或全局 `GWBHJ_KV` 读取该绑定。

## 3. 生成 Ed25519 密钥对

本地执行：

```bash
cd server
npm install
npm run generate-keys
```

保存输出：

- `Private Key (HEX)`：只放服务端环境变量，不能泄露。
- `Public Key (Base64)`：填入模块 `.so`。

## 4. 配置环境变量

在 EdgeOne Pages 项目中配置环境变量：

| 变量名 | 值 |
|------|----|
| `ED25519_PRIVATE_KEY` | 上一步生成的 Private Key HEX |
| `ADMIN_KEY` | 自定义管理员密钥 |

不要把私钥写入 Git 仓库。

## 5. 部署

推荐方式：直接推送 GitHub 分支，EdgeOne Pages 自动构建部署。

也可以本地使用 EdgeOne CLI：

```bash
npm install -g edgeone
cd server
npm install
edgeone pages link
edgeone pages deploy
```

## 6. 测试 API

部署后得到域名，例如：

```text
https://gwbhj-server.edgeone.app
```

测试健康检查：

```bash
curl https://gwbhj-server.edgeone.app/health
```

成功返回：

```json
{"status":"ok","platform":"edgeone","time":1719012345}
```

生成卡密：

```bash
curl -X POST https://gwbhj-server.edgeone.app/admin/generate \
  -H "Content-Type: application/json" \
  -d '{"count":5,"admin_key":"你的ADMIN_KEY"}'
```

查看卡密列表：

```bash
curl "https://gwbhj-server.edgeone.app/admin/list?admin_key=你的ADMIN_KEY"
```

## 7. 回填模块配置

### WebUI 服务端地址

编辑 `module/webroot/script.js`：

```js
const SERVER = 'https://gwbhj-server.edgeone.app';
```

### .so 公钥

编辑 `src/spoof_module.cpp`：

```cpp
static const char* SERVER_PUBLIC_KEY_B64 =
    "你的 Public Key Base64";
```

然后重新编译打包模块。

## 8. API 路径

| 路径 | 方法 | 说明 |
|------|------|------|
| `/health` | GET | 健康检查 |
| `/api/bind` | POST | 绑定卡密 |
| `/api/verify` | POST | 验证卡密 |
| `/api/clash-info` | POST | 查询互抵详情 |
| `/admin/generate` | POST | 生成卡密 |
| `/admin/delete` | POST | 删除卡密 |
| `/admin/list` | GET | 查询卡密列表 |

## 9. 注意事项

- KV 变量名必须是 `GWBHJ_KV`。
- `ADMIN_KEY` 和 `ED25519_PRIVATE_KEY` 必须配置为 EdgeOne 环境变量。
- EdgeOne KV 是最终一致性，跨节点可能有最多约 60 秒缓存延迟。
- 私钥丢失后，所有旧授权都无法继续签发同源 license。
