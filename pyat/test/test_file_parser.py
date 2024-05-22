from math import sqrt  # noqa: F401

import numpy as np
import pytest
from numpy.testing import assert_allclose as assert_close

from at import UnorderedParser, MadxParser

__all__ = []

test_data1 = """
/* test block comment
delimiter:      ";"
linecomment:    "!", "//"
end of comments */

V1 = 42.0 ;                  // Test line comment
V2 = 2*(sqrt(4) + 2**2) ;    // Test expression

label : command1, flag1,     ! Test continuation
-flag2, title="test parser", ! Test disabled flag
arg1=v1, arg2=v2, arg3=v3 ;  ! Test postponed definition

V3 = True ; V4 = False ;     ! Test several commands
"""

test_data2 = r"""
/* test block comment
delimiter:      end-of-line
linecomment:    "#"
continuation:   "\"
end of comments */

V1 = 42.0                     # Test line comment
V2 = 2*(sqrt(4) + 2**2)       # Test expression

label : command1, flag1, \    # Test continuation
-flag2, title="test parser",\ # Test disabled flag
arg1=v1, arg2=v2, arg3=v3     # Test postponed definition

V3 = True
"""

test_data = dict(data1=test_data1, data2=test_data2)

madx_data = """
BEAM, PARTICLE='electron', RADIATE, ENERGY=6.0, SEQUENCE=RING.1;
BEAM, PARTICLE='positron', ENERGY=2.0;

Q1: QUADRUPOLE, L:=QL ;   ! check forward reference

Q1.F: Q1, K1=0.5 ;
Q1.D: Q1, K1=-Q1.F->K1;   ! check attribute access
Q1.F, TILT=0.001;         ! check element update
SOL1: SOLENOID, L=0.5,  K1S=3.0;
MULT1: MULTIPOLE, KNL={1.0, 1.0, 2.0, 6.0};
HK1: HKICKER,   L=0,    KICK=0.001;
VK1: VKICKER,   L=0,    KICK=-0.001;
HVK2: KICKER,   L=0,    VKICK=VKICK;
BPM1: MONITOR,  L=0.1;
BPM2: VMONITOR;
RFCAVITY, VOLT=5, FREQ=352.2, HARMON=31, AT=0.0;

CELL.1: SEQUENCE, L=4.0;
QFCELL: Q1.F,   AT=0.75;
Q1.D,           AT=2.0, FROM=QFCELL;
ENDSEQUENCE;

RING.1: SEQUENCE, L=LRING;
CELL.1, AT=2.0;
SOL1,   AT=4.5;
MULT1,  AT=4.8;
HK1,    AT=4.8;
VK1,    AT=4.8;
HVK2,   AT=4.8;
BPM1,   AT=4.9;
BPM2,   AT=5;
CELL.1, AT=7.0;
ENDSEQUENCE;

VALUE, EMASS;

QL = sqrt(4)/2;          ! check arithmetic
LRING = 9.0;
VKICK = 0.003;
"""

# Global definitions for BaseParser

true = True
false = False


def command1(**kwargs):
    """Sample command testing that arguments are as expected"""
    print(kwargs)
    assert kwargs["title"] == "test parser"
    assert kwargs["flag1"] is True
    assert kwargs["flag2"] is False
    assert kwargs["arg1"] == 42.0
    assert kwargs["arg2"] == 12.0
    assert kwargs["arg3"] is True
    return "done"

# End of global definitions


@pytest.mark.parametrize(
    "delimiter, linecomment, data", [[";", ("!", "//"), "data1"], [None, "#", "data2"]]
)
def test_unordered_parser(delimiter, linecomment, data):
    parser = UnorderedParser(
        globals(),
        blockcomment=("/*", "*/"),
        linecomment=linecomment,
        delimiter=delimiter,
    )
    parser.parse_lines(test_data[data].splitlines())
    assert parser["label"] == "done"


def test_madx_parser():
    # fmt: off
    expected_ring_pos = np.array([
        0.0,    0.25,   1.25,   2.25,   3.25,   4.0,    4.25,   4.75,   4.8,
        4.8,    4.8,    4.8,    4.8,    4.85,   4.9,    4.9,    4.95,   5.0,
        5.0,    5.25,   6.25,   7.25,   8.25,   9.0,
    ])
    expected_cell_pos = np.array([0.,   0.25,   1.25,   2.25,   3.25,   4.])
    # fmt: on
    parser = MadxParser()
    parser.parse_lines(madx_data.splitlines())

    ring = parser.lattice(use="ring.1")
    assert len(ring) == 23
    assert ring.energy == 6.0e09
    assert str(ring.particle) == "electron"
    assert ring.circumference == 9.0
    assert ring.is_6d is True
    assert_close(ring.get_s_pos(), expected_ring_pos)

    cell = parser.lattice(use="cell.1")
    assert len(cell) == 5
    assert cell.energy == 2.0e09
    assert str(cell.particle) == "positron"
    assert cell.circumference == 4.0
    assert cell.is_6d is False
    assert_close(cell.get_s_pos(), expected_cell_pos)
