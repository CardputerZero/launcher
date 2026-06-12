#!/usr/bin/env python3
"""
FirmwareManagementV3 命令行客户端

使用方法:
    python3 firmware_manager.py <command> [options]

示例:
    # 登录（保存 token 到本地）
    python3 firmware_manager.py login --email user@example.com --password mypass

    # 查看公开固件列表（免登录）
    python3 firmware_manager.py public-list

    # 查看管理固件列表（需登录）
    python3 firmware_manager.py list
    python3 firmware_manager.py list --sku M5Stack-CardputerZero

    # 上传固件（需登录）
    python3 firmware_manager.py upload --file firmware.bin --avatar logo.png \\
        --name "MyFirmware" --sku M5Stack-Zero --version v1.0.0 \\
        --description "Initial release" --class firmware

    # 查看固件详情（需登录）
    python3 firmware_manager.py detail --id <firmware_id>

    # 删除固件（需登录）
    python3 firmware_manager.py delete --id <firmware_id>
"""

import argparse
import json
import os
import sys

try:
    import requests
except ImportError:
    print("缺少依赖库 requests，请先执行: pip3 install requests")
    sys.exit(1)

# ──────────────────────── 常量 ────────────────────────────────────
LOGIN_URL = "https://uiflow2.m5stack.com/m5stack/api/v2/login/doLogin"
BASE_URL  = "https://ota.m5stack.com/ota/api/v3/firmware-management"
TOKEN_FILE = os.path.join(os.path.dirname(__file__), ".fw_manager_token")
TIMEOUT = 30


# ──────────────────────── 工具函数 ────────────────────────────────

def _pretty(obj) -> str:
    return json.dumps(obj, ensure_ascii=False, indent=2)


def _print_response(resp: requests.Response) -> dict:
    """打印 HTTP 响应并返回解析后的 JSON 体。"""
    print(f"HTTP {resp.status_code}")
    try:
        body = resp.json()
        print(_pretty(body))
        return body
    except ValueError:
        print(resp.text)
        return {}


def _load_token() -> str:
    """从本地文件读取 token。"""
    if os.path.exists(TOKEN_FILE):
        with open(TOKEN_FILE, "r", encoding="utf-8") as f:
            return f.read().strip()
    return ""


def _save_token(token: str) -> None:
    """将 token 保存到本地文件。"""
    with open(TOKEN_FILE, "w", encoding="utf-8") as f:
        f.write(token)
    print(f"✓ Token 已保存到 {TOKEN_FILE}")


def _auth_headers(token: str = "") -> dict:
    """构造鉴权请求头。"""
    t = token or _load_token()
    if not t:
        print("⚠ 未找到 token，请先执行: python3 firmware_manager.py login")
        sys.exit(1)
    return {"token": t}


def _url(path: str) -> str:
    return f"{BASE_URL}{path}"


# ──────────────────────── 接口封装 ────────────────────────────────

def cmd_login(args: argparse.Namespace) -> None:
    """2.1 登录，获取并保存 token。"""
    print(f"→ 登录: {args.email}")
    resp = requests.post(
        LOGIN_URL,
        json={"email": args.email, "password": args.password},
        timeout=TIMEOUT,
    )
    body = _print_response(resp)
    if body.get("code") == 200:
        token = body["data"]["token"]
        _save_token(token)
        print(f"✓ 登录成功，用户: {body['data'].get('userName', '')}")
    else:
        print(f"✗ 登录失败: {body.get('msg', '')}")
        sys.exit(1)


def cmd_public_list(args: argparse.Namespace) -> None:
    """5.3 公开固件列表（按 SKU 分组，免登录）。"""
    print("→ 获取公开固件列表")
    resp = requests.get(_url("/public/list"), timeout=TIMEOUT)
    body = _print_response(resp)

    data = body.get("data") or []
    if isinstance(data, list) and data:
        print(f"\n共 {len(data)} 个 SKU:")
        for group in data:
            sku = group.get("sku", "?")
            items = group.get("items") or []
            print(f"  [{sku}]  {len(items)} 个版本")
            for item in items:
                ver = item.get("version", "?")
                url = item.get("url", "")
                remark = item.get("remark", "")
                print(f"    • {ver}  {remark}  {url}")


def cmd_list(args: argparse.Namespace) -> None:
    """5.2 管理固件列表（需登录）。"""
    params = {}
    if args.sku:
        params["sku"] = args.sku
        print(f"→ 获取固件列表，SKU 过滤: {args.sku}")
    else:
        print("→ 获取固件列表（全部）")

    resp = requests.get(
        _url("/list"),
        params=params,
        headers=_auth_headers(),
        timeout=TIMEOUT,
    )
    body = _print_response(resp)

    data = body.get("data") or []
    if isinstance(data, list) and data:
        print(f"\n共 {len(data)} 条记录:")
        for item in data:
            print(
                f"  id={item.get('id')}  sku={item.get('sku')}  "
                f"version={item.get('version')}  name={item.get('firmwareName')}  "
                f"createTime={item.get('createTime')}"
            )


def cmd_upload(args: argparse.Namespace) -> None:
    """5.1 上传固件（需登录）。"""
    if not os.path.isfile(args.file):
        print(f"✗ 固件文件不存在: {args.file}")
        sys.exit(1)
    if not os.path.isfile(args.avatar):
        print(f"✗ 头像文件不存在: {args.avatar}")
        sys.exit(1)

    print(f"→ 上传固件: {args.file}")
    print(f"  头像:   {args.avatar}")
    print(f"  固件名: {args.name}")
    print(f"  SKU:    {args.sku}")
    print(f"  版本:   {args.version}")

    data = {
        "firmwareName": args.name,
        "sku": args.sku,
        "version": args.version,
        "description": args.description,
    }
    if args.fw_class:
        data["class"] = args.fw_class
    if args.operator:
        data["operator"] = args.operator

    with open(args.file, "rb") as fw, open(args.avatar, "rb") as av:
        resp = requests.post(
            _url("/upload"),
            headers=_auth_headers(),
            data=data,
            files={
                "file":   (os.path.basename(args.file),   fw, "application/octet-stream"),
                "avatar": (os.path.basename(args.avatar), av, "application/octet-stream"),
            },
            timeout=TIMEOUT,
        )

    body = _print_response(resp)
    if body.get("code") == 200:
        fw_id = None
        d = body.get("data")
        if isinstance(d, dict):
            fw_id = d.get("id")
        elif d:
            fw_id = d
        if fw_id:
            print(f"✓ 上传成功，固件 id: {fw_id}")
        else:
            print("✓ 上传成功")
    else:
        print(f"✗ 上传失败: {body.get('msg', '')}")
        sys.exit(1)


def cmd_detail(args: argparse.Namespace) -> None:
    """5.4 固件详情（需登录）。"""
    print(f"→ 查询固件详情，id: {args.id}")
    resp = requests.get(
        _url("/detail"),
        params={"id": args.id},
        headers=_auth_headers(),
        timeout=TIMEOUT,
    )
    body = _print_response(resp)

    if body.get("code") == 200:
        d = body.get("data", {})
        print("\n── 固件详情 ──")
        fields = [
            ("id",           "ID"),
            ("firmwareName", "固件名"),
            ("sku",          "SKU"),
            ("version",      "版本"),
            ("class",        "类名"),
            ("description",  "描述"),
            ("fileUrl",      "下载地址"),
            ("avatarUrl",    "头像地址"),
            ("fileMd5",      "MD5"),
            ("fileSize",     "文件大小(bytes)"),
            ("operator",     "操作人"),
            ("createTime",   "创建时间"),
            ("updateTime",   "更新时间"),
        ]
        for key, label in fields:
            print(f"  {label:<16}: {d.get(key, '')}")
    else:
        print(f"✗ 查询失败: {body.get('msg', '')}")
        sys.exit(1)


def cmd_delete(args: argparse.Namespace) -> None:
    """5.5 删除固件（需登录）。"""
    print(f"→ 删除固件，id: {args.id}")
    if not args.yes:
        confirm = input("确认删除？(y/N): ").strip().lower()
        if confirm not in ("y", "yes"):
            print("已取消。")
            return

    resp = requests.delete(
        _url("/delete"),
        params={"id": args.id},
        headers=_auth_headers(),
        timeout=TIMEOUT,
    )
    body = _print_response(resp)
    if body.get("code") == 200:
        print("✓ 删除成功")
    else:
        print(f"✗ 删除失败: {body.get('msg', '')}")
        sys.exit(1)


def cmd_whoami(_args: argparse.Namespace) -> None:
    """显示当前已保存的 token（前 30 字符）。"""
    token = _load_token()
    if token:
        print(f"当前 Token: {token[:30]}...  (来源: {TOKEN_FILE})")
    else:
        print("尚未登录，请执行: python3 firmware_manager.py login")


# ──────────────────────── CLI 入口 ────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="firmware_manager.py",
        description="FirmwareManagementV3 命令行客户端",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = parser.add_subparsers(dest="command", metavar="<command>")
    sub.required = True

    # ── login ──
    p_login = sub.add_parser("login", help="登录并保存 token")
    p_login.add_argument("--email",    required=True, help="登录邮箱")
    p_login.add_argument("--password", required=True, help="登录密码")
    p_login.set_defaults(func=cmd_login)

    # ── whoami ──
    p_whoami = sub.add_parser("whoami", help="显示当前已登录账号的 token 信息")
    p_whoami.set_defaults(func=cmd_whoami)

    # ── public-list ──
    p_pub = sub.add_parser("public-list", help="公开固件列表（无需登录）")
    p_pub.set_defaults(func=cmd_public_list)

    # ── list ──
    p_list = sub.add_parser("list", help="管理固件列表（需登录）")
    p_list.add_argument("--sku", default="", help="按 SKU 过滤（可选）")
    p_list.set_defaults(func=cmd_list)

    # ── upload ──
    p_upload = sub.add_parser("upload", help="上传固件（需登录）")
    p_upload.add_argument("--file",        required=True,  help="固件文件路径")
    p_upload.add_argument("--avatar",      required=True,  help="头像/封面图片路径")
    p_upload.add_argument("--name",        required=True,  dest="name",     help="固件名 (firmwareName)")
    p_upload.add_argument("--sku",         required=True,  help="SKU")
    p_upload.add_argument("--version",     required=True,  help="版本号")
    p_upload.add_argument("--description", required=True,  help="描述")
    p_upload.add_argument("--class",       default="",     dest="fw_class", help="类名（可选）")
    p_upload.add_argument("--operator",    default="",     help="操作人（可选）")
    p_upload.set_defaults(func=cmd_upload)

    # ── detail ──
    p_detail = sub.add_parser("detail", help="固件详情（需登录）")
    p_detail.add_argument("--id", required=True, help="固件 ID")
    p_detail.set_defaults(func=cmd_detail)

    # ── delete ──
    p_delete = sub.add_parser("delete", help="删除固件（需登录）")
    p_delete.add_argument("--id",  required=True,      help="固件 ID")
    p_delete.add_argument("-y", "--yes", action="store_true", help="跳过确认提示")
    p_delete.set_defaults(func=cmd_delete)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
