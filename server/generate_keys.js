// generate_keys.js — 生成 Ed25519 密钥对
// 运行: node generate_keys.js

async function main() {
    const { sign, getPublicKey, utils } = require('@noble/ed25519');
    
    const privateKey = utils.randomPrivateKey();
    const privateKeyHex = Buffer.from(privateKey).toString('hex');
    const publicKey = await getPublicKey(privateKeyHex);
    const publicKeyHex = Buffer.from(publicKey).toString('hex');
    const publicKeyBase64 = Buffer.from(publicKey).toString('base64');
    
    console.log('=== Ed25519 密钥对 ===');
    console.log('Private Key (HEX):', privateKeyHex);
    console.log('Public Key (HEX):', publicKeyHex);
    console.log('Public Key (Base64):', publicKeyBase64);
    console.log('');
    console.log('=== 需要填入的位置 ===');
    console.log('私钥 HEX -> EdgeOne 项目环境变量 ED25519_PRIVATE_KEY');
    console.log('公钥 Base64 -> src/spoof_module.cpp 的 SERVER_PUBLIC_KEY_B64');
    console.log('公钥 Base64 -> module/webroot/script.js 的 SERVER_PUBLIC_KEY (验签用)');
}

main();
