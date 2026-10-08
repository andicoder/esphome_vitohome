"""text_sensor type raw with `rpc:`: probe a Remote_Procedure_Call by hand.

Some WPR data is reachable only as an RPC with fixed parameters -- the operating
log ("Betriebstagebuch") is function code 0x07 at 0xB800 with two parameter
bytes (openv #480). `rpc:` sends those bytes instead of a READ and publishes the
answer as hex, so a layout can be read off a real device before it is decoded.
"""

import os
import sys

import pytest

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

import esphome.config_validation as cv  # noqa: E402
from esphome.core import CORE  # noqa: E402

from components.vitohome.text_sensor import CONFIG_SCHEMA  # noqa: E402


@pytest.fixture(autouse=True)
def _fresh_core():
    CORE.reset()
    yield
    CORE.reset()


_NAME_SEQ = iter(range(1000))


def _cfg(**overrides):
    base = {"name": f"Probe {next(_NAME_SEQ)}", "type": "raw", "address": 0xB800}
    base.update(overrides)
    return base


def test_rpc_parameters_are_accepted():
    cfg = CONFIG_SCHEMA(_cfg(rpc=[0x00, 0x01]))
    assert cfg["rpc"] == [0x00, 0x01]


def test_raw_without_rpc_stays_a_plain_read():
    assert "rpc" not in CONFIG_SCHEMA(_cfg())


def test_rpc_takes_one_to_four_bytes():
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(rpc=[]))
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(rpc=[0, 0, 0, 0, 0]))


def test_rpc_parameter_is_a_byte():
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_cfg(rpc=[0x100]))
