import shutil
import pytest

def pytest_addoption(parser):
    parser.addoption('--test-output-dir', action='store', default='/tmp/test-out', help='where all the files related with batsim executions should be put')
    parser.addoption('--clear', action='store_true', help='clears the test output directory before running tests')

@pytest.fixture(scope="session", autouse=True)
def test_root_dir(pytestconfig):
    out_dir = pytestconfig.getoption("--test-output-dir")
    if pytestconfig.getoption("--clear"):
        shutil.rmtree(out_dir, ignore_errors=True)

    return pytestconfig.getoption("--test-output-dir")
