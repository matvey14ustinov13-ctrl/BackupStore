#!/usr/bin/env python3
"""Builds catalog.json for BackupStore from public GitHub releases."""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

OWNER = "matvey14ustinov13-ctrl"
STORE_REPO = "BackupStore"
API = "https://api.github.com"
TOKEN = os.environ.get("GITHUB_TOKEN", "").strip()

HEADERS = {
    "Accept": "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
    "User-Agent": "BackupStore-Catalog",
}
if TOKEN:
    HEADERS["Authorization"] = f"Bearer {TOKEN}"


def api_get(path: str):
    request = urllib.request.Request(API + path, headers=HEADERS)
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.load(response)


def get_repositories() -> list[dict]:
    repositories: list[dict] = []
    for page in range(1, 11):
        chunk = api_get(f"/users/{OWNER}/repos?per_page=100&sort=updated&type=owner&page={page}")
        if not isinstance(chunk, list):
            raise RuntimeError("GitHub returned an unexpected repository list")
        repositories.extend(chunk)
        if len(chunk) < 100:
            break
    return repositories


def latest_release(full_name: str) -> dict | None:
    releases = api_get(f"/repos/{full_name}/releases?per_page=10")
    if not isinstance(releases, list):
        return None
    fallback = None
    for release in releases:
        if release.get("draft"):
            continue
        if not release.get("prerelease"):
            return release
        if fallback is None:
            fallback = release
    return fallback


def release_payload(release: dict) -> dict:
    assets = []
    for asset in release.get("assets", []):
        assets.append(
            {
                "name": asset.get("name", ""),
                "browser_download_url": asset.get("browser_download_url", ""),
                "content_type": asset.get("content_type", ""),
                "size": asset.get("size", 0),
            }
        )
    return {
        "tag_name": release.get("tag_name", ""),
        "name": release.get("name") or release.get("tag_name", ""),
        "body": release.get("body") or "",
        "published_at": release.get("published_at") or "",
        "html_url": release.get("html_url") or "",
        "prerelease": bool(release.get("prerelease")),
        "assets": assets,
    }


def main() -> int:
    repositories = get_repositories()
    apps: list[dict] = []

    for repo in repositories:
        if not isinstance(repo, dict) or repo.get("fork"):
            continue
        name = repo.get("name", "")
        if not name or name.lower() == STORE_REPO.lower():
            continue

        try:
            release = latest_release(repo["full_name"])
        except urllib.error.HTTPError as error:
            print(f"skip {name}: HTTP {error.code}", file=sys.stderr)
            continue
        if not release or not release.get("tag_name"):
            continue

        license_info = repo.get("license") or {}
        apps.append(
            {
                "name": name,
                "full_name": repo.get("full_name", ""),
                "description": repo.get("description") or "",
                "html_url": repo.get("html_url") or "",
                "updated_at": repo.get("updated_at") or "",
                "stargazers_count": repo.get("stargazers_count", 0),
                "archived": bool(repo.get("archived")),
                "license": {"spdx_id": license_info.get("spdx_id") or ""},
                "topics": repo.get("topics") or [],
                "latest_release": release_payload(release),
            }
        )
        print(f"added {name} {release.get('tag_name')}")

    apps.sort(key=lambda item: item.get("name", "").lower())
    catalog = {
        "generated_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
        "apps": apps,
    }

    output = Path(__file__).resolve().parent.parent / "catalog.json"
    output.write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {output} with {len(apps)} app(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
