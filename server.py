#!/usr/bin/env python3
"""
Campus Lost & Found — Local Development Server
================================================
Serves the frontend, data files, and provides two API endpoints:

  POST /api/run-matcher   — Executes the C++ matching engine
  POST /api/add-item      — Adds a lost/found item to items.json

Usage:
  python server.py [port]          (default port: 8080)

Requirements:
  - Python 3.7+  (standard library only)
  - backend/campus_matcher.exe must be compiled
"""

import http.server
import json
import os
import shutil
import subprocess
import sys
import re
import time
from pathlib import Path

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
PROJECT_ROOT = Path(__file__).resolve().parent
ITEMS_JSON = PROJECT_ROOT / "data" / "items.json"
MATCHES_JSON = PROJECT_ROOT / "data" / "matches.json"
MATCHER_EXE = PROJECT_ROOT / "backend" / "campus_matcher.exe"

# ---------------------------------------------------------------------------
# Request Handler
# ---------------------------------------------------------------------------

class CampusHandler(http.server.SimpleHTTPRequestHandler):
    """Extends SimpleHTTPRequestHandler with POST API routes."""

    def __init__(self, *args, **kwargs):
        # Serve files from the project root
        super().__init__(*args, directory=str(PROJECT_ROOT), **kwargs)

    # ---- Routing -----------------------------------------------------------

    def do_POST(self):
        if self.path == "/api/run-matcher":
            self._handle_run_matcher()
        elif self.path == "/api/add-item":
            self._handle_add_item()
        elif self.path == "/api/update-match":
            self._handle_update_match()
        elif self.path == "/api/delete-item":
            self._handle_delete_item()
        else:
            self._send_json(404, {"error": "Not found"})

    # ---- CORS (for local dev if needed) ------------------------------------

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.end_headers()

    # ---- POST /api/run-matcher ---------------------------------------------

    def _handle_run_matcher(self):
        # Check that the executable exists
        if not MATCHER_EXE.exists():
            self._send_json(500, {
                "success": False,
                "error": "campus_matcher.exe not found. Compile with: "
                         "g++ -std=c++17 -Wall -Wextra -o backend/campus_matcher.exe "
                         "backend/main.cpp backend/matcher.cpp",
                "output": "",
                "matchCount": 0
            })
            return

        # Check that items.json exists
        if not ITEMS_JSON.exists():
            self._send_json(400, {
                "success": False,
                "error": "data/items.json not found.",
                "output": "",
                "matchCount": 0
            })
            return

        try:
            result = subprocess.run(
                [str(MATCHER_EXE), str(ITEMS_JSON), str(MATCHES_JSON)],
                capture_output=True,
                text=True,
                timeout=30,
                cwd=str(PROJECT_ROOT)
            )

            output = result.stdout + result.stderr

            if result.returncode != 0:
                self._send_json(500, {
                    "success": False,
                    "error": f"Matcher exited with code {result.returncode}",
                    "output": output,
                    "matchCount": 0
                })
                return

            # Count matches from the generated file
            match_count = 0
            if MATCHES_JSON.exists():
                try:
                    with open(MATCHES_JSON, "r", encoding="utf-8") as f:
                        data = json.load(f)
                    match_count = len(data.get("matches", []))
                except (json.JSONDecodeError, IOError):
                    pass

            self._send_json(200, {
                "success": True,
                "output": output,
                "matchCount": match_count
            })

        except subprocess.TimeoutExpired:
            self._send_json(500, {
                "success": False,
                "error": "Matcher timed out after 30 seconds.",
                "output": "",
                "matchCount": 0
            })
        except Exception as e:
            self._send_json(500, {
                "success": False,
                "error": str(e),
                "output": "",
                "matchCount": 0
            })

    # ---- POST /api/add-item ------------------------------------------------

    def _handle_add_item(self):
        # Read request body
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            payload = json.loads(body.decode("utf-8"))
        except (ValueError, json.JSONDecodeError) as e:
            self._send_json(400, {"error": f"Invalid JSON: {e}"})
            return

        # Validate required fields
        required = ["type", "title", "category", "description", "location",
                     "reporter", "email"]
        missing = [f for f in required if not payload.get(f, "").strip()]
        if missing:
            self._send_json(400, {
                "error": f"Missing required fields: {', '.join(missing)}"
            })
            return

        item_type = payload["type"]  # "lost" or "found"
        if item_type not in ("lost", "found"):
            self._send_json(400, {"error": "type must be 'lost' or 'found'"})
            return

        # Read current items.json
        try:
            with open(ITEMS_JSON, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError) as e:
            self._send_json(500, {"error": f"Cannot read items.json: {e}"})
            return

        # Determine the list and generate a unique ID
        list_key = "lost_items" if item_type == "lost" else "found_items"
        prefix = "L" if item_type == "lost" else "F"
        items_list = data.get(list_key, [])

        # Find the highest existing numeric ID to avoid collisions
        max_num = 0
        for item in items_list:
            match = re.match(rf"^{prefix}(\d+)$", item.get("id", ""))
            if match:
                max_num = max(max_num, int(match.group(1)))
        new_id = f"{prefix}{str(max_num + 1).zfill(3)}"

        # Build the new item
        tags_raw = payload.get("tags", "")
        if isinstance(tags_raw, str):
            tags = [t.strip() for t in tags_raw.split(",") if t.strip()]
        elif isinstance(tags_raw, list):
            tags = tags_raw
        else:
            tags = []

        new_item = {
            "id": new_id,
            "title": payload["title"].strip(),
            "category": payload["category"].strip(),
            "description": payload["description"].strip(),
            "location": payload["location"].strip(),
            "date": payload.get("date", time.strftime("%Y-%m-%d")),
            "time": payload.get("time", time.strftime("%H:%M")),
            "reporter": payload["reporter"].strip(),
            "email": payload["email"].strip(),
            "status": "open",
            "color": payload.get("color", "").strip(),
            "brand": payload.get("brand", "").strip(),
            "tags": tags
        }

        # Handle optional structured location
        if payload.get("block"):
            new_item["block"] = payload["block"].strip()
            new_item["building"] = payload.get("building", "").strip()
            new_item["floor"] = payload.get("floor", "").strip()
            new_item["room"] = payload.get("room", "").strip()

        image_url = payload.get("imageUrl", "").strip()
        if image_url:
            # Reject javascript: and data: schemes — they bypass HTML-escaping
            # in <img src> and enable stored XSS. Check is case-insensitive.
            lowered = image_url.lower()
            if lowered.startswith("javascript:") or lowered.startswith("data:"):
                self._send_json(400, {
                    "error": "Invalid imageUrl: javascript: and data: schemes are not permitted."
                })
                return
            new_item["imageUrl"] = image_url

        # Backup items.json before modifying
        backup_path = ITEMS_JSON.with_suffix(".json.bak")
        try:
            shutil.copy2(str(ITEMS_JSON), str(backup_path))
        except IOError:
            pass  # Non-critical — proceed even if backup fails

        # Add item and save
        items_list.append(new_item)
        data[list_key] = items_list

        try:
            # Write to a temp file first, then rename (atomic-ish on Windows)
            tmp_path = ITEMS_JSON.with_suffix(".json.tmp")
            with open(tmp_path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
                f.write("\n")
            # Replace original
            shutil.move(str(tmp_path), str(ITEMS_JSON))
        except IOError as e:
            self._send_json(500, {"error": f"Failed to save items.json: {e}"})
            return

        self._send_json(201, {
            "success": True,
            "item": new_item,
            "totalLost": len(data.get("lost_items", [])),
            "totalFound": len(data.get("found_items", []))
        })

    # ---- POST /api/update-match --------------------------------------------

    def _handle_update_match(self):
        # Read request body
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            payload = json.loads(body.decode("utf-8"))
        except (ValueError, json.JSONDecodeError) as e:
            self._send_json(400, {"error": f"Invalid JSON: {e}"})
            return

        match_id = payload.get("match_id", "").strip()
        new_status = payload.get("status", "").strip()

        if not match_id or not new_status:
            self._send_json(400, {"error": "Missing match_id or status"})
            return

        if new_status not in ("confirmed", "rejected", "pending"):
            self._send_json(400, {
                "error": "status must be 'confirmed', 'rejected', or 'pending'"
            })
            return

        # Read current matches.json
        if not MATCHES_JSON.exists():
            self._send_json(404, {"error": "matches.json not found. Run the matcher first."})
            return

        try:
            with open(MATCHES_JSON, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            self._send_json(500, {"error": f"Cannot read matches.json: {e}"})
            return

        # Find and update the match
        matches = data.get("matches", [])
        found = False
        for match in matches:
            if match.get("id") == match_id:
                match["status"] = new_status
                found = True
                break

        if not found:
            self._send_json(404, {"error": f"Match '{match_id}' not found"})
            return

        # Write back atomically
        try:
            tmp_path = MATCHES_JSON.with_suffix(".json.tmp")
            with open(tmp_path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
                f.write("\n")
            shutil.move(str(tmp_path), str(MATCHES_JSON))
        except IOError as e:
            self._send_json(500, {"error": f"Failed to save matches.json: {e}"})
            return

        self._send_json(200, {
            "success": True,
            "match_id": match_id,
            "status": new_status
        })

    # ---- POST /api/delete-item -----------------------------------------------

    def _handle_delete_item(self):
        # Read request body
        try:
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            payload = json.loads(body.decode("utf-8"))
        except (ValueError, json.JSONDecodeError) as e:
            self._send_json(400, {"error": f"Invalid JSON: {e}"})
            return

        item_id = payload.get("id", "").strip()
        if not item_id:
            self._send_json(400, {"error": "Missing 'id' field"})
            return

        # Read current items.json
        try:
            with open(ITEMS_JSON, "r", encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            self._send_json(500, {"error": f"Cannot read items.json: {e}"})
            return

        # Find and remove the item
        found = False
        for key in ("lost_items", "found_items"):
            items = data.get(key, [])
            for i, item in enumerate(items):
                if item.get("id") == item_id:
                    items.pop(i)
                    found = True
                    break
            if found:
                break

        if not found:
            self._send_json(404, {"error": f"Item '{item_id}' not found"})
            return

        # Write back atomically
        try:
            backup = ITEMS_JSON.with_suffix(".json.bak")
            shutil.copy2(str(ITEMS_JSON), str(backup))

            tmp_path = ITEMS_JSON.with_suffix(".json.tmp")
            with open(tmp_path, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
                f.write("\n")
            shutil.move(str(tmp_path), str(ITEMS_JSON))
        except IOError as e:
            self._send_json(500, {"error": f"Failed to save items.json: {e}"})
            return

        # Prune any matches.json entries that referenced the deleted item,
        # so the sidebar match count and the Matches list stay in sync.
        try:
            if MATCHES_JSON.exists():
                with open(MATCHES_JSON, "r", encoding="utf-8") as f:
                    matches_data = json.load(f)
                original_count = len(matches_data.get("matches", []))
                matches_data["matches"] = [
                    m for m in matches_data.get("matches", [])
                    if m.get("lost_id") != item_id and m.get("found_id") != item_id
                ]
                if len(matches_data["matches"]) != original_count:
                    matches_tmp = MATCHES_JSON.with_suffix(".json.tmp")
                    with open(matches_tmp, "w", encoding="utf-8") as f:
                        json.dump(matches_data, f, indent=2, ensure_ascii=False)
                        f.write("\n")
                    shutil.move(str(matches_tmp), str(MATCHES_JSON))
        except (IOError, json.JSONDecodeError):
            # Non-fatal: the item itself was deleted successfully either way.
            # A stale match entry will simply be filtered out on the next
            # matcher run if this cleanup step doesn't succeed.
            pass

        self._send_json(200, {
            "success": True,
            "deleted_id": item_id,
            "totalLost": len(data.get("lost_items", [])),
            "totalFound": len(data.get("found_items", []))
        })

    # ---- Helpers -----------------------------------------------------------

    def _send_json(self, status_code, data):
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        """Prefix log messages with a timestamp."""
        sys.stderr.write("[%s] %s\n" % (
            time.strftime("%H:%M:%S"),
            format % args
        ))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print("=" * 50)
    print("  Campus Lost & Found — Development Server")
    print("=" * 50)
    print(f"  Project root:  {PROJECT_ROOT}")
    print(f"  Matcher:       {'✓ Found' if MATCHER_EXE.exists() else '✗ Not compiled'}")
    print(f"  Items data:    {'✓ Found' if ITEMS_JSON.exists() else '✗ Missing'}")
    print(f"  Matches data:  {'✓ Found' if MATCHES_JSON.exists() else '○ Will be generated'}")
    print()
    print(f"  Frontend:      http://localhost:{PORT}/frontend/")
    print(f"  API:           POST /api/run-matcher")
    print(f"                 POST /api/add-item")
    print("=" * 50)
    print()

    with http.server.HTTPServer(("", PORT), CampusHandler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")
