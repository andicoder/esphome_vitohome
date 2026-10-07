"""The text platform's two day-program formats.

`schaltzeiten` is the 8-byte ON/OFF day block of the boiler controllers.
`wpr_day` is the WPR heat-pump layout: eight 3-byte periods (start, end,
mode) per day, read as one 24-byte block and written period by period.
Hardware-read on a V200WO1A (HK1 at 0x9200) on 2026-10-07.
"""

import os
import sys

import pytest

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

import esphome.config_validation as cv  # noqa: E402
from esphome.core import CORE  # noqa: E402

from components.vitohome.text import CONFIG_SCHEMA, SCHALTZEITEN_LENGTH, WPR_DAY_LENGTH, day_program_length  # noqa: E402


@pytest.fixture(autouse=True)
def _fresh_core():
    CORE.reset()
    yield
    CORE.reset()


_NAME_SEQ = iter(range(1000))


def _cfg(**overrides):
    base = {"name": f"Heizen HK1 Mo {next(_NAME_SEQ)}", "address": 0x9200}
    base.update(overrides)
    return base


def test_format_defaults_to_the_boiler_day_block():
    cfg = CONFIG_SCHEMA(_cfg())
    assert cfg["format"] == "schaltzeiten"
    assert day_program_length(cfg) == SCHALTZEITEN_LENGTH == 8


def test_wpr_day_reads_eight_three_byte_periods():
    cfg = CONFIG_SCHEMA(_cfg(format="wpr_day"))
    assert day_program_length(cfg) == WPR_DAY_LENGTH == 24


def test_unknown_format_is_rejected():
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(format="kw_day"))


def test_read_only_twin_exists_as_a_text_sensor():
    """A text entity can be edited from Home Assistant; until the day programs
    have an editor that cannot produce a broken program, a deployment can show
    them through the read-only text_sensor type instead."""
    from components.vitohome.text_sensor import CONFIG_SCHEMA as TS_SCHEMA
    from components.vitohome.text_sensor import TEXT_SENSOR_TYPES

    assert "wpr_day" in TEXT_SENSOR_TYPES
    cfg = TS_SCHEMA({"name": f"Heizen Mo ro {next(_NAME_SEQ)}", "type": "wpr_day", "address": 0x9200})
    assert cfg["length"] == WPR_DAY_LENGTH
