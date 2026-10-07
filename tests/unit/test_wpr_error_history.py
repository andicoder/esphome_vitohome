"""text_sensor type wpr_error_history: the WPR heat-pump fault buffer.

'WPRError' at 0xA801 is 30 entries of 8 bytes, fetched one Remote_Procedure_Call
per entry index (P300 function code 0x07). See decode.h WprFaultEntry.
"""

import os
import sys

import pytest

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

import esphome.config_validation as cv  # noqa: E402
from esphome.core import CORE  # noqa: E402

from components.vitohome.text_sensor import (  # noqa: E402
    CONFIG_SCHEMA,
    TEXT_SENSOR_TYPES,
    WPR_FAULT_ENTRY_LENGTH,
)


@pytest.fixture(autouse=True)
def _fresh_core():
    CORE.reset()
    yield
    CORE.reset()


_NAME_SEQ = iter(range(1000))


def _cfg(**overrides):
    base = {"name": f"Fehlerhistorie {next(_NAME_SEQ)}", "type": "wpr_error_history", "address": 0xA801}
    base.update(overrides)
    return base


def test_type_is_registered():
    assert "wpr_error_history" in TEXT_SENSOR_TYPES


def test_defaults_to_thirty_eight_byte_entries():
    cfg = CONFIG_SCHEMA(_cfg())
    assert cfg["entries"] == 30
    assert cfg["length"] == WPR_FAULT_ENTRY_LENGTH == 8


def test_entry_length_is_fixed_by_the_wire_layout():
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(length=9))


def test_entries_are_bounded():
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(entries=0))
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(entries=256))
