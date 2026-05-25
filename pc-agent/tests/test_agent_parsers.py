"""Tests de las funciones puras de parseo en agent.py
(token OAuth, headers de rate-limit). Sin red."""
import time

import agent


# ── _extract_access_token ───────────────────────────────────────────────
def test_extract_token_claudeai_oauth_shape():
    raw = '{"claudeAiOauth": {"accessToken": "abc123"}}'
    assert agent._extract_access_token(raw) == "abc123"


def test_extract_token_snake_case_shape():
    raw = '{"claude_ai_oauth": {"access_token": "tok"}}'
    assert agent._extract_access_token(raw) == "tok"


def test_extract_token_flat_shape():
    raw = '{"accessToken": "flat"}'
    assert agent._extract_access_token(raw) == "flat"


def test_extract_token_invalid_json():
    assert agent._extract_access_token("no es json") is None


def test_extract_token_missing_field():
    assert agent._extract_access_token('{"otra": 1}') is None


# ── _pct ──────────────────────────────────────────────────────────────────
def test_pct_none_is_zero():
    assert agent._pct(None) == 0


def test_pct_fraction_scales_to_percent():
    assert agent._pct("0.5") == 50
    assert agent._pct("1.0") == 100


def test_pct_already_percent_passthrough():
    assert agent._pct("85") == 85


def test_pct_invalid_is_zero():
    assert agent._pct("abc") == 0


# ── _reset_minutes ─────────────────────────────────────────────────────────
def test_reset_minutes_seconds():
    assert agent._reset_minutes("300") == 5


def test_reset_minutes_empty_is_zero():
    assert agent._reset_minutes("") == 0
    assert agent._reset_minutes(None) == 0


def test_reset_minutes_epoch_future():
    future = str(time.time() + 3600)   # +1h en epoch
    assert 59 <= agent._reset_minutes(future) <= 60


def test_reset_minutes_iso_format():
    from datetime import datetime, timezone
    future = datetime.fromtimestamp(time.time() + 1800, tz=timezone.utc)
    iso = future.isoformat().replace("+00:00", "Z")
    assert 29 <= agent._reset_minutes(iso) <= 30


# ── _build_from_headers ─────────────────────────────────────────────────────
def test_build_from_headers_maps_keys():
    h = {
        "anthropic-ratelimit-unified-5h-utilization": "0.42",
        "anthropic-ratelimit-unified-5h-reset": "600",
        "anthropic-ratelimit-unified-7d-utilization": "10",
        "anthropic-ratelimit-unified-7d-reset": "0",
        "anthropic-ratelimit-unified-5h-status": "allowed",
    }
    d = agent._build_from_headers(h)
    assert d["ok"] is True
    assert d["s"] == 42
    assert d["sr"] == 10
    assert d["w"] == 10
    assert d["st"] == "allowed"


def test_build_from_headers_defaults_when_missing():
    d = agent._build_from_headers({})
    assert d["ok"] is True
    assert d["s"] == 0
    assert d["st"] == "unknown"
