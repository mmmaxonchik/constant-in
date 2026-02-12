# Environment for test framework

PYTHON=python3
RM=rm

PYTHON_OBJS=*.pyc __pycache__
SYSFILTER_ARTIFACTS=*.json *.log

.DEFAULT_GOAL := all
.PHONY: cleanobj cleantest

cleanobj:
	$(RM) -rfv *.o $(PYTHON_OBJS)

testclean:
	$(RM) -fv $(SYSFILTER_ARTIFACTS)

sysfilter:
	$(MAKE) -C ../../
