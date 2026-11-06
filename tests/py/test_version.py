import rydtd


def test_version_string():
    v = rydtd.version()
    assert isinstance(v, str)
    assert v.count(".") == 2


def test_platform_tag():
    assert rydtd.platform_tag().count("-") == 2


def test_has_openmp_is_bool():
    assert isinstance(rydtd.has_openmp(), bool)
