#!/usr/bin/env python3
"""
生成企业级认证服务器所需的本地自签名 TLS 证书（仅用于本地联调 demo）。

用法：
    py -3.12 tools/generate_cert.py
    或  python tools/generate_cert.py   （需已安装 cryptography）

产出：
    tools/certs/server.key   PKCS#8 私钥（PEM）
    tools/certs/server.crt   X.509 证书（PEM，SAN: localhost / 127.0.0.1，有效期 10 年）

安全说明（重要）：
    1. 自签名证书只用于本地联调。生产环境必须使用受信任 CA 签发的证书，
       并把私钥托管到 KMS/HSM，绝不允许随源码分发。
    2. 客户端通过"证书固定（certificate pinning）"只信任本证书文件，
       用于演示企业级"信任锚"做法；生产环境应改为系统 CA 链 + 域名校验。
    3. 私钥文件权限应限制为当前用户可读（Windows 下默认继承用户权限）。
"""
import ipaddress
import os
import sys
from datetime import datetime, timedelta, timezone

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

CERT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "certs")
KEY_PATH = os.path.join(CERT_DIR, "server.key")
CRT_PATH = os.path.join(CERT_DIR, "server.crt")
VALID_DAYS = 3650
KEY_BITS = 2048


def main() -> None:
    os.makedirs(CERT_DIR, exist_ok=True)

    key = rsa.generate_private_key(public_exponent=65537, key_size=KEY_BITS)

    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "localhost")])
    now = datetime.now(timezone.utc)

    cert = (
        x509.CertificateBuilder()
        .subject_name(name)
        .issuer_name(name)
        .public_key(key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - timedelta(days=1))
        .not_valid_after(now + timedelta(days=VALID_DAYS))
        .add_extension(
            x509.SubjectAlternativeName(
                [
                    x509.DNSName("localhost"),
                    x509.IPAddress(ipaddress.IPv4Address("127.0.0.1")),
                ]
            ),
            critical=False,
        )
        .add_extension(
            x509.BasicConstraints(ca=False, path_length=None),
            critical=True,
        )
        .add_extension(
            x509.KeyUsage(
                digital_signature=True,
                key_encipherment=True,
                content_commitment=False,
                data_encipherment=False,
                key_agreement=False,
                key_cert_sign=False,
                crl_sign=False,
                encipher_only=False,
                decipher_only=False,
            ),
            critical=True,
        )
        .add_extension(
            x509.ExtendedKeyUsage([x509.oid.ExtendedKeyUsageOID.SERVER_AUTH]),
            critical=False,
        )
        .sign(key, hashes.SHA256())
    )

    with open(KEY_PATH, "wb") as f:
        f.write(
            key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.PKCS8,
                encryption_algorithm=serialization.NoEncryption(),
            )
        )
    with open(CRT_PATH, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))

    print(f"OK: {CRT_PATH}")
    print(f"OK: {KEY_PATH}")


if __name__ == "__main__":
    try:
        main()
    except ImportError:
        print(
            "缺少 cryptography，请先安装：\n"
            "    py -3.12 -m pip install cryptography -i https://pypi.tuna.tsinghua.edu.cn/simple",
            file=sys.stderr,
        )
        sys.exit(1)
